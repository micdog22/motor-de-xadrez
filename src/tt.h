// Tabela de transposição: memória de posições já analisadas, indexada pela chave Zobrist.
#ifndef XADREZ_TT_H
#define XADREZ_TT_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "types.h"

namespace xadrez {

// Que tipo de valor foi guardado. Numa busca alfa-beta nem todo resultado é exato:
// se houve corte beta, sabemos só que o valor é pelo menos aquele (limite inferior);
// se nenhum lance superou alfa, sabemos só que é no máximo aquele (limite superior).
enum Bound : std::uint8_t { BOUND_NONE = 0, BOUND_UPPER = 1, BOUND_LOWER = 2, BOUND_EXACT = 3 };

struct TTEntry {
  Key key = 0;
  Move move = MOVE_NONE;
  std::int16_t score = 0;
  std::int16_t depth = 0;
  std::uint8_t bound = BOUND_NONE;
  std::uint8_t generation = 0;
};

class TranspositionTable {
 public:
  static constexpr std::size_t kMinMb = 1;
  static constexpr std::size_t kMaxMb = 1024;
  static constexpr std::size_t kDefaultMb = 16;

  explicit TranspositionTable(std::size_t megabytes = kDefaultMb);
  void resize(std::size_t megabytes);
  void clear();
  void new_search() { ++generation_; }
  bool probe(Key key, TTEntry& out) const;
  void store(Key key, Move move, int score, int depth, Bound bound);
  int hashfull() const;  // ocupação por mil, como pede o protocolo UCI
  std::size_t megabytes() const { return megabytes_; }

 private:
  std::vector<TTEntry> table_;
  std::size_t mask_ = 0;
  std::size_t megabytes_ = 0;
  std::uint8_t generation_ = 0;
};

}  // namespace xadrez

#endif
