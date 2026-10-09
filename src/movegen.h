// Geração de lances legais, perft e resultado da partida.
#ifndef XADREZ_MOVEGEN_H
#define XADREZ_MOVEGEN_H

#include <cstdint>

#include "position.h"

namespace xadrez {

struct MoveList {
  Move moves[MAX_MOVES];
  int count = 0;

  void push(Move m) { moves[count++] = m; }
  int size() const { return count; }
  Move operator[](int i) const { return moves[i]; }
  Move& operator[](int i) { return moves[i]; }
  const Move* begin() const { return moves; }
  const Move* end() const { return moves + count; }
  bool contains(Move m) const {
    for (int i = 0; i < count; ++i)
      if (moves[i] == m) return true;
    return false;
  }
};

// Todos os lances legais da posição.
void generate_legal(const Position& pos, MoveList& list);
// Só capturas e promoções a dama (legais), para a busca de quiescência.
void generate_noisy(const Position& pos, MoveList& list);

// Número de posições folha a uma profundidade fixa (conta lances na última camada).
std::uint64_t perft(Position& pos, int depth);

enum class GameResult { Ongoing, Checkmate, Stalemate, InsufficientMaterial, Threefold, FiftyMoves };
GameResult game_result(const Position& pos);

}  // namespace xadrez

#endif
