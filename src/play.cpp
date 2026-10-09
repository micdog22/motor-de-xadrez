#include "play.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "search.h"
#include "uci.h"

namespace xadrez {
namespace {

struct PlayedMove {
  Move move;
  bool byHuman;
};

std::string trim(const std::string& s) {
  const std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  const std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::string lowercase(std::string s) {
  for (char& ch : s)
    if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
  return s;
}

std::string result_message(GameResult r, Color sideToMove, Color human) {
  switch (r) {
    case GameResult::Checkmate:
      return sideToMove == human ? "Xeque-mate. O motor venceu." : "Xeque-mate! Você venceu.";
    case GameResult::Stalemate:
      return "Empate por afogamento: quem tem a vez não está em xeque e não tem lance legal.";
    case GameResult::InsufficientMaterial:
      return "Empate por material insuficiente: nenhum dos lados consegue dar mate.";
    case GameResult::Threefold:
      return "Empate por tripla repetição: a mesma posição apareceu três vezes.";
    case GameResult::FiftyMoves:
      return "Empate pela regra dos 50 lances: 50 lances de cada lado sem captura nem lance de peão.";
    case GameResult::Ongoing:
      break;
  }
  return "";
}

// Lê o lance digitado: formato UCI (e2e4), SAN na notação escolhida ou na outra.
bool parse_user_move(const Position& pos, const std::string& text, Notation preferred, Move& out,
                     std::string& error) {
  if (looks_like_uci(text)) {
    out = parse_uci(pos, text);
    if (out != MOVE_NONE) return true;
    error = "Lance ilegal nesta posição: " + text + ". Digite 'lances' para ver as opções.";
    return false;
  }
  const Notation other = preferred == Notation::Portuguese ? Notation::English : Notation::Portuguese;
  Move a = MOVE_NONE, b = MOVE_NONE;
  const SanStatus sa = parse_san(pos, text, preferred, a);
  const SanStatus sb = parse_san(pos, text, other, b);
  if (sa == SanStatus::Ok && sb == SanStatus::Ok && a != b) {
    error = "Lance ambíguo: em português R é o rei e em inglês R é a torre. Use T para a torre, "
            "K para o rei ou o formato UCI (como " + move_to_uci(a) + ").";
    return false;
  }
  if (sa == SanStatus::Ok || sb == SanStatus::Ok) {
    out = sa == SanStatus::Ok ? a : b;
    return true;
  }
  if (sa == SanStatus::Ambiguous || sb == SanStatus::Ambiguous) {
    error = "Lance ambíguo: mais de uma peça pode ir para essa casa. Indique a coluna ou a fileira "
            "de origem (por exemplo, Cbd2 ou T1e2).";
    return false;
  }
  if (sa == SanStatus::NotFound || sb == SanStatus::NotFound) {
    error = "Lance ilegal nesta posição: " + text + ". Digite 'lances' para ver as opções.";
    return false;
  }
  error = "Não entendi '" + text + "'. Exemplos: e4, Cf3, O-O, exd5, e8=D ou e2e4. Digite 'ajuda'.";
  return false;
}

const char* const HELP_TEXT =
    "Comandos:\n"
    "  e4, Cf3, Nf3, O-O, exd5, e8=D   lance em notação algébrica (português ou inglês)\n"
    "  e2e4, e7e8q                     lance no formato UCI\n"
    "  desfazer                        volta o seu último lance e a resposta do motor\n"
    "  dica                            o motor sugere um lance para você\n"
    "  lances                          lista os lances legais\n"
    "  tabuleiro                       mostra o tabuleiro de novo\n"
    "  fen                             mostra a posição em FEN\n"
    "  ajuda                           mostra esta ajuda\n"
    "  sair                            encerra a partida\n";

std::string format_seconds(long long ms) {
  const long long tenths = (ms + 50) / 100;
  return std::to_string(tenths / 10) + "," + std::to_string(tenths % 10) + " s";
}

}  // namespace

std::string format_thousands(std::uint64_t n) {
  std::string digits = std::to_string(n), out;
  for (std::size_t i = 0; i < digits.size(); ++i) {
    if (i > 0 && (digits.size() - i) % 3 == 0) out += '.';
    out += digits[i];
  }
  return out;
}

std::string format_eval(int score, Color sideToMove) {
  if (is_mate_score(score)) {
    const int moves = mate_in_moves(score);  // positivo: quem tem a vez dá o mate
    const bool whiteMates = (moves > 0) == (sideToMove == WHITE);
    return "mate em " + std::to_string(std::abs(moves)) + (whiteMates ? " para as brancas" : " para as pretas");
  }
  const int white = sideToMove == WHITE ? score : -score;
  const int a = std::abs(white);
  std::string text = white > 0 ? "+" : white < 0 ? "-" : "";
  text += std::to_string(a / 100) + ",";
  text += char('0' + (a % 100) / 10);
  text += char('0' + a % 10);
  return text;
}

int play_game(std::istream& in, std::ostream& out, const PlayOptions& options) {
  Position pos;
  if (!options.fen.empty()) {
    std::string error;
    if (!pos.set_fen(options.fen, &error)) {
      out << "FEN inválida: " << error << "\n";
      return 2;
    }
  }
  TranspositionTable tt;
  auto searcher = std::make_unique<Searcher>(tt);
  std::vector<PlayedMove> played;
  const Color human = options.human;
  const Notation notation = options.notation;

  auto show_board = [&]() { out << "\n" << board_diagram(pos, human, notation, options.unicode); };
  auto limits_for_move = [&](long long maxTimeMs) {
    SearchLimits limits;
    if (options.depth > 0)
      limits.depth = options.depth;
    else
      limits.movetime = std::max(1LL, maxTimeMs);
    return limits;
  };

  out << ENGINE_NAME << ": você joga de " << (human == WHITE ? "brancas" : "pretas")
      << ". Digite 'ajuda' para ver os comandos.\n"
      << "As avaliações são em peões, do ponto de vista das brancas (positivo é bom para as brancas).\n";
  show_board();

  bool announced = false;
  while (true) {
    const GameResult result = game_result(pos);
    if (result != GameResult::Ongoing) {
      if (!announced) {
        out << "\n" << result_message(result, pos.side_to_move(), human) << "\n";
        out << "Fim de partida. Digite 'desfazer' para voltar um lance ou 'sair' para encerrar.\n";
        announced = true;
      }
    } else if (pos.side_to_move() != human) {
      out << "O motor está pensando...\n" << std::flush;
      searcher->reset_stop();
      const SearchResult r = searcher->search(pos, limits_for_move(options.timeMs));
      const std::string san = move_to_san(pos, r.bestMove, notation);
      out << "Motor jogou: " << san << " (avaliação " << format_eval(r.score, pos.side_to_move())
          << ", profundidade " << r.depth << ", " << format_thousands(r.nodes) << " nós em "
          << format_seconds(r.timeMs) << ")\n";
      pos.make_move(r.bestMove);
      played.push_back({r.bestMove, false});
      show_board();
      continue;
    }

    out << "\nSeu lance: " << std::flush;
    std::string line;
    if (!std::getline(in, line)) {
      out << "\n";
      break;
    }
    const std::string text = trim(line);
    const std::string command = lowercase(text);
    if (text.empty()) continue;

    if (command == "sair") {
      out << "Até a próxima!\n";
      break;
    } else if (command == "ajuda") {
      out << HELP_TEXT;
    } else if (command == "tabuleiro") {
      show_board();
    } else if (command == "fen") {
      out << pos.fen() << "\n";
    } else if (command == "lances") {
      MoveList list;
      generate_legal(pos, list);
      std::string all;
      for (Move m : list) all += (all.empty() ? "" : " ") + move_to_san(pos, m, notation);
      out << (all.empty() ? std::string("Nenhum lance legal.") : "Lances legais (" + std::to_string(list.size()) + "): " + all)
          << "\n";
    } else if (command == "desfazer") {
      bool humanMoved = false;
      for (const PlayedMove& p : played) humanMoved = humanMoved || p.byHuman;
      if (!humanMoved) {
        out << "Nada para desfazer.\n";
        continue;
      }
      do {
        pos.unmake_move(played.back().move);
        played.pop_back();
      } while (!played.empty() && pos.side_to_move() != human);
      announced = false;
      out << "Lance desfeito.\n";
      show_board();
    } else if (command == "dica") {
      if (result != GameResult::Ongoing) {
        out << "A partida terminou: não há o que sugerir.\n";
        continue;
      }
      searcher->reset_stop();
      const SearchResult r = searcher->search(pos, limits_for_move(std::min(options.timeMs, 2000LL)));
      out << "Dica: " << move_to_san(pos, r.bestMove, notation) << " (avaliação "
          << format_eval(r.score, pos.side_to_move()) << ")\n";
    } else if (result != GameResult::Ongoing) {
      out << "A partida terminou. Digite 'desfazer' ou 'sair'.\n";
    } else {
      Move m = MOVE_NONE;
      std::string error;
      if (!parse_user_move(pos, text, notation, m, error)) {
        out << error << "\n";
        continue;
      }
      out << "Você jogou " << move_to_san(pos, m, notation) << ".\n";
      pos.make_move(m);
      played.push_back({m, true});
      show_board();
    }
  }
  return 0;
}

}  // namespace xadrez
