// Modo "xadrez jogar": partida contra o motor no terminal, em português.
#ifndef XADREZ_PLAY_H
#define XADREZ_PLAY_H

#include <cstdint>
#include <iosfwd>
#include <string>

#include "notation.h"

namespace xadrez {

struct PlayOptions {
  Color human = WHITE;
  long long timeMs = 1000;  // tempo do motor por lance
  int depth = 0;            // maior que zero: profundidade fixa em vez de tempo
  std::string fen;          // vazio: posição inicial
  Notation notation = Notation::Portuguese;
  bool unicode = false;
};

// Devolve o código de saída do programa (0 normal, 2 FEN inválida).
int play_game(std::istream& in, std::ostream& out, const PlayOptions& options);

// "+0,35" do ponto de vista das brancas, ou "mate em 3 para as pretas".
std::string format_eval(int score, Color sideToMove);
// 1234567 vira "1.234.567".
std::string format_thousands(std::uint64_t n);

}  // namespace xadrez

#endif
