#include "tt.h"

#include <algorithm>

namespace xadrez {

TranspositionTable::TranspositionTable(std::size_t megabytes) { resize(megabytes); }

void TranspositionTable::resize(std::size_t megabytes) {
  megabytes = std::clamp(megabytes, kMinMb, kMaxMb);
  // Maior potência de 2 de entradas que cabe no tamanho pedido: o índice vira um AND.
  const std::size_t wanted = megabytes * 1024 * 1024 / sizeof(TTEntry);
  std::size_t entries = 1;
  while (entries * 2 <= wanted) entries *= 2;
  std::vector<TTEntry>(entries).swap(table_);  // libera de fato a memória antiga
  mask_ = entries - 1;
  megabytes_ = megabytes;
  generation_ = 0;
}

void TranspositionTable::clear() {
  std::fill(table_.begin(), table_.end(), TTEntry());
  generation_ = 0;
}

bool TranspositionTable::probe(Key key, TTEntry& out) const {
  const TTEntry& e = table_[key & mask_];
  if (e.bound == BOUND_NONE || e.key != key) return false;
  out = e;
  return true;
}

void TranspositionTable::store(Key key, Move move, int score, int depth, Bound bound) {
  TTEntry& e = table_[key & mask_];
  // Mesma posição sem lance novo: preserva o lance antigo, que ainda serve para ordenar.
  if (e.key == key && move == MOVE_NONE) move = e.move;
  // Substitui entradas de outras buscas, de outras posições ou mais rasas.
  if (e.bound == BOUND_NONE || e.key != key || e.generation != generation_ || depth + 2 >= e.depth ||
      bound == BOUND_EXACT) {
    e.key = key;
    e.move = move;
    e.score = std::int16_t(score);
    e.depth = std::int16_t(depth);
    e.bound = std::uint8_t(bound);
    e.generation = generation_;
  }
}

int TranspositionTable::hashfull() const {
  const std::size_t sample = std::min<std::size_t>(1000, table_.size());
  int used = 0;
  for (std::size_t i = 0; i < sample; ++i)
    if (table_[i].bound != BOUND_NONE && table_[i].generation == generation_) ++used;
  return int(used * 1000 / sample);
}

}  // namespace xadrez
