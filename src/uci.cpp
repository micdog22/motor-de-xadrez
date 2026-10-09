#include "uci.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>

#include "eval.h"
#include "notation.h"

namespace xadrez {

const char* const ENGINE_NAME = "Motor de Xadrez 1.0";
const char* const ENGINE_AUTHOR = "Michael Douglas";

namespace {

std::string lowercase(std::string s) {
  for (char& ch : s)
    if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
  return s;
}

bool read_number(std::istringstream& is, long long& value) {
  std::string token;
  if (!(is >> token)) return false;
  try {
    std::size_t used = 0;
    const long long v = std::stoll(token, &used);
    if (used != token.size()) return false;
    value = v;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

std::string format_info(const SearchInfo& info) {
  std::string line = "info depth " + std::to_string(info.depth);
  if (info.depth == 0) return line + " score " + format_uci_score(info.score);
  const long long ms = std::max(1LL, info.timeMs);
  line += " seldepth " + std::to_string(info.seldepth);
  line += " score " + format_uci_score(info.score);
  line += " nodes " + std::to_string(info.nodes);
  line += " nps " + std::to_string(info.nodes * 1000 / std::uint64_t(ms));
  line += " hashfull " + std::to_string(info.hashfull);
  line += " time " + std::to_string(info.timeMs);
  line += " pv";
  for (Move m : info.pv) line += " " + move_to_uci(m);
  return line;
}

}  // namespace

std::string format_uci_score(int score) {
  if (is_mate_score(score)) return "mate " + std::to_string(mate_in_moves(score));
  return "cp " + std::to_string(score);
}

UciEngine::UciEngine(std::ostream& out) : out_(out), searcher_(new Searcher(tt_)) {}

UciEngine::~UciEngine() { stop_search(); }

void UciEngine::send(const std::string& line) {
  std::lock_guard<std::mutex> lock(outMutex_);
  out_ << line << '\n' << std::flush;
}

void UciEngine::wait_search() {
  if (worker_.joinable()) worker_.join();
}

void UciEngine::stop_search() {
  if (worker_.joinable()) {
    searcher_->request_stop();
    worker_.join();
  }
}

void UciEngine::loop(std::istream& in) {
  std::string line;
  while (std::getline(in, line)) {
    if (!handle(line)) return;
  }
  // Fim da entrada: uma busca com limite termina normalmente; uma infinita é parada.
  if (infiniteSearch_)
    stop_search();
  else
    wait_search();
}

bool UciEngine::handle(const std::string& rawLine) {
  std::string line = rawLine;
  if (!line.empty() && line.back() == '\r') line.pop_back();
  std::istringstream is(line);
  std::string cmd;
  if (!(is >> cmd)) return true;

  if (cmd == "uci") {
    cmd_uci();
  } else if (cmd == "isready") {
    send("readyok");  // responde na hora, mesmo durante uma busca
  } else if (cmd == "ucinewgame") {
    stop_search();
    tt_.clear();
    searcher_->clear_history();
    pos_ = Position();
  } else if (cmd == "position") {
    stop_search();
    cmd_position(is);
  } else if (cmd == "go") {
    cmd_go(is);
  } else if (cmd == "stop") {
    stop_search();
  } else if (cmd == "quit") {
    stop_search();
    return false;
  } else if (cmd == "setoption") {
    stop_search();
    cmd_setoption(is);
  } else if (cmd == "d") {
    cmd_display();
  } else if (cmd == "eval") {
    std::string report = eval_report(pos_);
    if (!report.empty() && report.back() == '\n') report.pop_back();
    send(report);
  } else if (cmd == "perft") {
    long long depth = 0;
    stop_search();
    if (read_number(is, depth) && depth >= 1 && depth <= 10)
      cmd_perft(int(depth));
    else
      send("info string uso: perft <profundidade de 1 a 10>");
  } else if (cmd == "ponderhit" || cmd == "debug" || cmd == "register") {
    // Sem ponder e sem registro: nada a fazer.
  } else {
    send("info string comando desconhecido: " + cmd);
  }
  return true;
}

void UciEngine::cmd_uci() {
  send(std::string("id name ") + ENGINE_NAME);
  send(std::string("id author ") + ENGINE_AUTHOR);
  send("option name Hash type spin default " + std::to_string(TranspositionTable::kDefaultMb) + " min " +
       std::to_string(TranspositionTable::kMinMb) + " max " + std::to_string(TranspositionTable::kMaxMb));
  send("option name Clear Hash type button");
  send("uciok");
}

void UciEngine::cmd_position(std::istringstream& is) {
  std::string token;
  if (!(is >> token)) {
    send("info string uso: position startpos|fen <FEN> [moves <lances>]");
    return;
  }
  Position p;
  if (token == "startpos") {
    is >> token;
  } else if (token == "fen") {
    std::string fen;
    while (is >> token && token != "moves") fen += token + " ";
    std::string error;
    if (!p.set_fen(fen, &error)) {
      send("info string FEN inválida: " + error);
      return;
    }
  } else {
    send("info string uso: position startpos|fen <FEN> [moves <lances>]");
    return;
  }
  if (token == "moves") {
    while (is >> token) {
      const Move m = parse_uci(p, token);
      if (m == MOVE_NONE) {
        send("info string lance ilegal ignorado: " + token);
        break;
      }
      p.make_move(m);
    }
  }
  pos_ = p;
}

void UciEngine::cmd_go(std::istringstream& is) {
  SearchLimits limits;
  std::string token;
  long long value = 0;
  while (is >> token) {
    if (token == "infinite" || token == "ponder") {
      limits.infinite = true;
    } else if (token == "perft") {
      stop_search();
      if (read_number(is, value) && value >= 1 && value <= 10) cmd_perft(int(value));
      return;
    } else if (token == "searchmoves") {
      break;  // não suportado: analisa todos os lances
    } else if (read_number(is, value)) {
      // Algumas interfaces mandam relógio negativo quando o tempo estoura: vira zero.
      if (token == "wtime") limits.time[WHITE] = std::max(0LL, value);
      else if (token == "btime") limits.time[BLACK] = std::max(0LL, value);
      else if (token == "winc") limits.inc[WHITE] = std::max(0LL, value);
      else if (token == "binc") limits.inc[BLACK] = std::max(0LL, value);
      else if (token == "movestogo") limits.movestogo = int(std::clamp(value, 0LL, 1000LL));
      else if (token == "movetime") limits.movetime = std::max(1LL, value);
      else if (token == "depth") limits.depth = int(std::clamp(value, 1LL, (long long)MAX_PLY - 1));
      else if (token == "nodes") limits.nodes = std::uint64_t(std::max(1LL, value));
      else if (token == "mate") limits.depth = int(std::clamp(2 * value + 1, 1LL, (long long)MAX_PLY - 1));
    }
  }

  stop_search();
  searcher_->reset_stop();
  infiniteSearch_ = limits.infinite;
  const Position root = pos_;
  worker_ = std::thread([this, root, limits]() {
    const SearchResult r = searcher_->search(root, limits, [this](const SearchInfo& info) { send(format_info(info)); });
    std::string line = "bestmove ";
    line += r.bestMove == MOVE_NONE ? "(none)" : move_to_uci(r.bestMove);
    if (r.ponderMove != MOVE_NONE) line += " ponder " + move_to_uci(r.ponderMove);
    send(line);
  });
}

void UciEngine::cmd_setoption(std::istringstream& is) {
  std::string token, name, value;
  is >> token;  // "name"
  while (is >> token && token != "value") name += (name.empty() ? "" : " ") + token;
  while (is >> token) value += (value.empty() ? "" : " ") + token;
  const std::string key = lowercase(name);
  if (key == "hash") {
    try {
      const long long mb = std::stoll(value);
      tt_.resize(std::size_t(std::clamp(mb, (long long)TranspositionTable::kMinMb, (long long)TranspositionTable::kMaxMb)));
    } catch (const std::exception&) {
      send("info string valor inválido para Hash: " + value);
    }
  } else if (key == "clear hash") {
    tt_.clear();
  } else {
    send("info string opção desconhecida: " + name);
  }
}

void UciEngine::cmd_display() {
  std::string text = board_diagram(pos_, WHITE, Notation::English, false);
  text += "FEN: " + pos_.fen() + "\n";
  char key[32];
  std::snprintf(key, sizeof key, "%016llX", static_cast<unsigned long long>(pos_.key()));
  text += std::string("Chave Zobrist: ") + key + "\n";
  std::string checkers;
  for (Bitboard b = pos_.checkers(); b;) checkers += (checkers.empty() ? "" : " ") + square_name(pop_lsb(b));
  text += "Xeque de: " + (checkers.empty() ? std::string("nenhuma peça") : checkers);
  send(text);
}

void UciEngine::cmd_perft(int depth) {
  const auto start = std::chrono::steady_clock::now();
  Position p = pos_;
  MoveList list;
  generate_legal(p, list);
  std::uint64_t total = 0;
  for (Move m : list) {
    p.make_move(m);
    const std::uint64_t n = perft(p, depth - 1);
    p.unmake_move(m);
    total += n;
    send(move_to_uci(m) + ": " + std::to_string(n));
  }
  const long long ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  send("\nNós: " + std::to_string(total) + " (" + std::to_string(ms) + " ms)");
}

}  // namespace xadrez
