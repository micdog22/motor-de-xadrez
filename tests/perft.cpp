// Suíte perft: lê um arquivo EPD com linhas "FEN ;D1 n1 ;D2 n2 ..." e confere cada
// contagem. Com --limite N, pula as profundidades cujo resultado esperado passa de N.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "movegen.h"

using namespace xadrez;

namespace {
// Número com vírgula decimal, como se escreve no Brasil.
std::string decimal(double value, const char* format) {
  char text[64];
  std::snprintf(text, sizeof text, format, value);
  std::string s = text;
  for (char& ch : s)
    if (ch == '.') ch = ',';
  return s;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "uso: testes_perft <arquivo.epd> [--limite N]\n";
    return 2;
  }
  std::uint64_t limit = 0;
  if (argc >= 4 && std::string(argv[2]) == "--limite") limit = std::stoull(argv[3]);

  std::ifstream file(argv[1]);
  if (!file) {
    std::cerr << "não consegui abrir " << argv[1] << "\n";
    return 2;
  }

  init();
  int checks = 0, failures = 0, skipped = 0;
  std::uint64_t totalNodes = 0;
  const auto start = std::chrono::steady_clock::now();
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    const std::size_t semi = line.find(';');
    std::string fen = line.substr(0, semi == std::string::npos ? line.size() : semi);
    while (!fen.empty() && fen.back() == ' ') fen.pop_back();
    Position pos;
    std::string error;
    ++checks;
    if (!pos.set_fen(fen, &error)) {
      ++failures;
      std::cout << "FALHOU  FEN inválida (" << error << "): " << fen << "\n";
      continue;
    }
    // A FEN escrita de volta precisa ser igual à lida.
    if (pos.fen() != fen) {
      ++failures;
      std::cout << "FALHOU  FEN de volta: " << pos.fen() << "\n";
    }

    std::istringstream rest(semi == std::string::npos ? "" : line.substr(semi));
    std::string token;
    while (rest >> token) {
      if (token.size() < 3 || token[0] != ';' || token[1] != 'D') continue;
      const int depth = std::stoi(token.substr(2));
      std::uint64_t expected = 0;
      rest >> expected;
      if (limit && expected > limit) {
        ++skipped;
        continue;
      }
      ++checks;
      const auto t0 = std::chrono::steady_clock::now();
      const std::uint64_t got = perft(pos, depth);
      const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      totalNodes += got;
      const bool ok = got == expected;
      if (!ok) ++failures;
      std::cout << (ok ? "ok      " : "FALHOU  ") << "D" << depth << " = " << got;
      if (!ok) std::cout << " (esperava " << expected << ")";
      std::cout << "  " << decimal(secs, "%.2f") << " s  " << fen << "\n";
    }
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::cout << "\nperft: " << checks << " verificações, " << failures << " falhas";
  if (skipped) std::cout << ", " << skipped << " profundidades puladas pelo limite";
  std::cout << " (" << decimal(double(totalNodes) / 1e6, "%.1f") << " milhões de posições em " << decimal(secs, "%.2f")
            << " s)\n";
  return failures ? 1 : 0;
}
