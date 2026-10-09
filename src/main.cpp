// Ponto de entrada: modo UCI (padrão), partida no terminal, perft, divide e bench.
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "play.h"
#include "uci.h"

using namespace xadrez;

namespace {

const char* const USAGE =
    "Uso:\n"
    "  xadrez                      modo UCI, para Arena, Cute Chess e outras interfaces\n"
    "  xadrez jogar [opções]       joga contra o motor no terminal\n"
    "  xadrez perft <prof> [FEN]   conta as posições a uma profundidade fixa\n"
    "  xadrez divide <prof> [FEN]  perft separado por lance da raiz\n"
    "  xadrez bench [prof]         mede a velocidade da busca em posições fixas\n"
    "  xadrez ajuda                mostra esta ajuda\n"
    "\n"
    "Opções de 'jogar':\n"
    "  --pretas               você joga de pretas (o motor começa)\n"
    "  --tempo <ms>           tempo do motor por lance (padrão: 1000)\n"
    "  --profundidade <n>     profundidade fixa em vez de tempo\n"
    "  --fen \"<FEN>\"          começa de uma posição específica\n"
    "  --notacao <pt|en>      letras das peças: pt (R D T B C) ou en (K Q R B N); padrão: pt\n"
    "  --unicode              desenha as peças com símbolos Unicode\n";

int fail_usage(const std::string& message) {
  std::cerr << "xadrez: " << message << "\n\n" << USAGE;
  return 2;
}

bool parse_int(const std::string& text, long long minimum, long long maximum, long long& out) {
  try {
    std::size_t used = 0;
    const long long v = std::stoll(text, &used);
    if (used != text.size() || v < minimum || v > maximum) return false;
    out = v;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

std::string join_fen(const std::vector<std::string>& args, std::size_t from) {
  std::string fen;
  for (std::size_t i = from; i < args.size(); ++i) fen += (fen.empty() ? "" : " ") + args[i];
  return fen.empty() ? START_FEN : fen;
}

double seconds_since(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::string decimal(double value, int places) {
  std::string text = std::to_string(value);
  const std::size_t dot = text.find('.');
  if (dot != std::string::npos) text = text.substr(0, places > 0 ? dot + 1 + std::size_t(places) : dot);
  for (char& ch : text)
    if (ch == '.') ch = ',';
  return text;
}

int run_perft(const std::vector<std::string>& args, bool divide) {
  long long depth = 0;
  if (args.size() < 2 || !parse_int(args[1], 1, 10, depth)) return fail_usage("informe a profundidade (de 1 a 10)");
  Position pos;
  std::string error;
  if (!pos.set_fen(join_fen(args, 2), &error)) {
    std::cerr << "xadrez: FEN inválida: " << error << "\n";
    return 2;
  }
  const auto start = std::chrono::steady_clock::now();
  std::uint64_t total = 0;
  if (divide) {
    MoveList list;
    generate_legal(pos, list);
    for (Move m : list) {
      pos.make_move(m);
      const std::uint64_t n = perft(pos, int(depth) - 1);
      pos.unmake_move(m);
      total += n;
      std::cout << move_to_uci(m) << ": " << n << "\n";
    }
    std::cout << "\nLances: " << list.size() << "\nTotal: " << total << "\n";
  } else {
    total = perft(pos, int(depth));
    const double secs = seconds_since(start);
    std::cout << "perft(" << depth << ") = " << total << " em " << decimal(secs, 3) << " s ("
              << decimal(double(total) / (secs > 0 ? secs : 1e-9) / 1e6, 1) << " milhões de posições por segundo)\n";
  }
  return 0;
}

int run_bench(const std::vector<std::string>& args) {
  long long depth = 9;
  if (args.size() >= 2 && !parse_int(args[1], 1, 30, depth)) return fail_usage("profundidade inválida para bench");
  static const char* const fens[] = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
  };
  TranspositionTable tt;
  Searcher searcher(tt);
  std::uint64_t nodes = 0;
  const auto start = std::chrono::steady_clock::now();
  for (const char* fen : fens) {
    Position pos;
    pos.set_fen(fen);
    tt.clear();
    searcher.clear_history();
    SearchLimits limits;
    limits.depth = int(depth);
    searcher.reset_stop();
    const SearchResult r = searcher.search(pos, limits);
    nodes += r.nodes;
    std::cout << move_to_san(pos, r.bestMove, Notation::English) << "  " << format_uci_score(r.score) << "  "
              << r.nodes << " nós  " << fen << "\n";
  }
  const double secs = seconds_since(start);
  std::cout << "\nTotal: " << nodes << " nós em " << decimal(secs, 2) << " s ("
            << format_thousands(std::uint64_t(double(nodes) / (secs > 0 ? secs : 1e-9))) << " nós por segundo)\n";
  return 0;
}

int run_play(const std::vector<std::string>& args) {
  PlayOptions options;
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto value = [&]() -> const std::string* { return i + 1 < args.size() ? &args[++i] : nullptr; };
    long long n = 0;
    if (a == "--pretas") {
      options.human = BLACK;
    } else if (a == "--brancas") {
      options.human = WHITE;
    } else if (a == "--tempo") {
      const std::string* v = value();
      if (!v || !parse_int(*v, 10, 3600000, n)) return fail_usage("--tempo precisa de milissegundos (de 10 a 3600000)");
      options.timeMs = n;
    } else if (a == "--profundidade") {
      const std::string* v = value();
      if (!v || !parse_int(*v, 1, 64, n)) return fail_usage("--profundidade precisa de um número de 1 a 64");
      options.depth = int(n);
    } else if (a == "--fen") {
      const std::string* v = value();
      if (!v) return fail_usage("--fen precisa de uma posição entre aspas");
      options.fen = *v;
    } else if (a == "--notacao") {
      const std::string* v = value();
      if (v && *v == "pt")
        options.notation = Notation::Portuguese;
      else if (v && *v == "en")
        options.notation = Notation::English;
      else
        return fail_usage("--notacao aceita pt ou en");
    } else if (a == "--unicode") {
      options.unicode = true;
    } else {
      return fail_usage("opção desconhecida: " + a);
    }
  }
  return play_game(std::cin, std::cout, options);
}

}  // namespace

int main(int argc, char** argv) {
  init();
  const std::vector<std::string> args(argv + 1, argv + argc);
  const std::string cmd = args.empty() ? "uci" : args[0];

  if (cmd == "uci") {
    std::cout << ENGINE_NAME << " (protocolo UCI). Digite 'uci' para começar ou 'quit' para sair.\n" << std::flush;
    UciEngine engine(std::cout);
    engine.loop(std::cin);
    return 0;
  }
  if (cmd == "jogar") return run_play(args);
  if (cmd == "perft") return run_perft(args, false);
  if (cmd == "divide") return run_perft(args, true);
  if (cmd == "bench") return run_bench(args);
  if (cmd == "ajuda" || cmd == "--ajuda" || cmd == "-h" || cmd == "--help") {
    std::cout << ENGINE_NAME << ": motor de xadrez UCI com busca alfa-beta.\n\n" << USAGE;
    return 0;
  }
  if (cmd == "--versao" || cmd == "--version") {
    std::cout << ENGINE_NAME << "\n";
    return 0;
  }
  return fail_usage("comando desconhecido: " + cmd);
}
