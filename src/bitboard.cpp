#include "bitboard.h"

#include <vector>

namespace xadrez {
namespace attacks {

Bitboard Knight[64];
Bitboard King[64];
Bitboard Pawn[2][64];
Bitboard Between[64][64];
Bitboard Line[64][64];
Magic BishopMagics[64];
Magic RookMagics[64];

namespace {

// Soma de 2^(bits da máscara) em todas as casas: 102400 para a torre e 5248 para o bispo.
Bitboard RookTable[102400];
Bitboard BishopTable[5248];

// Gerador xorshift64* com semente fixa: os números mágicos saem sempre iguais.
class Prng {
 public:
  explicit Prng(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next() {
    state_ ^= state_ >> 12;
    state_ ^= state_ << 25;
    state_ ^= state_ >> 27;
    return state_ * 2685821657736338717ULL;
  }
  // Candidatos com poucos bits ligados viram números mágicos com mais facilidade.
  std::uint64_t sparse() { return next() & next() & next(); }

 private:
  std::uint64_t state_;
};

bool on_board(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

Bitboard leaper(Square s, const int (*deltas)[2], int count) {
  Bitboard b = 0;
  for (int i = 0; i < count; ++i) {
    const int f = file_of(s) + deltas[i][0];
    const int r = rank_of(s) + deltas[i][1];
    if (on_board(f, r)) b |= square_bb(make_square(f, r));
  }
  return b;
}

// Para cada casa: calcula a máscara de casas relevantes, enumera todas as ocupações
// possíveis dessa máscara e sorteia candidatos até achar um número que leve cada
// ocupação a um índice cuja entrada guarda exatamente os ataques corretos.
//
// A semente do gerador é reiniciada em cada casa com um valor por fileira. Qualquer
// semente funciona; estas foram escolhidas porque acham os números em poucas tentativas.
void init_magics(PieceType pt, Bitboard* table, Magic* magics, const std::uint64_t (&seeds)[8]) {
  std::vector<Bitboard> occupancy(4096), reference(4096);
  std::vector<int> epoch(4096, 0);
  int attempt = 0;
  Bitboard* next = table;

  for (Square s = 0; s < 64; ++s) {
    // As bordas não importam: a peça sempre "chega" na última casa do raio.
    const Bitboard edges = ((RANK_1_BB | RANK_8_BB) & ~rank_bb(rank_of(s))) |
                           ((FILE_A_BB | FILE_H_BB) & ~file_bb(file_of(s)));
    Magic& m = magics[s];
    m.mask = sliding_slow(pt, s, 0) & ~edges;
    m.shift = unsigned(64 - popcount(m.mask));
    m.table = next;

    // Truque "carry-rippler": (b - mask) & mask percorre todos os subconjuntos da máscara.
    int size = 0;
    Bitboard b = 0;
    do {
      occupancy[size] = b;
      reference[size] = sliding_slow(pt, s, b);
      ++size;
      b = (b - m.mask) & m.mask;
    } while (b);
    next += size;

    Prng rng(seeds[rank_of(s)]);
    for (int i = 0; i < size;) {
      do {
        m.magic = rng.sparse();
      } while (popcount((m.magic * m.mask) >> 56) < 6);
      ++attempt;
      // epoch evita limpar a tabela a cada tentativa: entradas de tentativas
      // anteriores contam como vazias.
      for (i = 0; i < size; ++i) {
        const unsigned idx = m.index(occupancy[i]);
        if (epoch[idx] < attempt) {
          epoch[idx] = attempt;
          m.table[idx] = reference[i];
        } else if (m.table[idx] != reference[i]) {
          break;  // colisão destrutiva: tenta outro número
        }
      }
    }
  }
}

void init_all() {
  static const int knightDeltas[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                         {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
  static const int kingDeltas[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1},
                                       {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
  static const int whitePawnDeltas[2][2] = {{-1, 1}, {1, 1}};
  static const int blackPawnDeltas[2][2] = {{-1, -1}, {1, -1}};

  for (Square s = 0; s < 64; ++s) {
    Knight[s] = leaper(s, knightDeltas, 8);
    King[s] = leaper(s, kingDeltas, 8);
    Pawn[WHITE][s] = leaper(s, whitePawnDeltas, 2);
    Pawn[BLACK][s] = leaper(s, blackPawnDeltas, 2);
  }

  static const std::uint64_t rookSeeds[8] = {13336, 10316, 12504, 15706, 13884, 15100, 16645, 255};
  static const std::uint64_t bishopSeeds[8] = {13336, 11474, 10090, 1431, 15302, 16471, 3702, 19858};
  init_magics(ROOK, RookTable, RookMagics, rookSeeds);
  init_magics(BISHOP, BishopTable, BishopMagics, bishopSeeds);

  for (Square a = 0; a < 64; ++a) {
    for (Square b = 0; b < 64; ++b) {
      Between[a][b] = 0;
      Line[a][b] = 0;
      if (a == b) continue;
      for (PieceType pt : {BISHOP, ROOK}) {
        if (sliding_slow(pt, a, 0) & square_bb(b)) {
          Line[a][b] = (sliding_slow(pt, a, 0) & sliding_slow(pt, b, 0)) | square_bb(a) | square_bb(b);
          Between[a][b] = sliding_slow(pt, a, square_bb(b)) & sliding_slow(pt, b, square_bb(a));
        }
      }
    }
  }
}

}  // namespace

Bitboard sliding_slow(PieceType pt, Square s, Bitboard occupied) {
  static const int rookDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  static const int bishopDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  const int (*dirs)[2] = pt == ROOK ? rookDirs : bishopDirs;
  Bitboard result = 0;
  for (int d = 0; d < 4; ++d) {
    int f = file_of(s) + dirs[d][0];
    int r = rank_of(s) + dirs[d][1];
    while (on_board(f, r)) {
      const Bitboard b = square_bb(make_square(f, r));
      result |= b;
      if (occupied & b) break;  // a primeira peça no caminho bloqueia o raio
      f += dirs[d][0];
      r += dirs[d][1];
    }
  }
  return result;
}

void init() {
  // Inicialização de variável estática local é segura entre threads (C++11).
  static const bool done = (init_all(), true);
  (void)done;
}

}  // namespace attacks
}  // namespace xadrez
