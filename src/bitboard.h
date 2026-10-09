// Bitboards: conjuntos de casas guardados em 64 bits, tabelas de ataque e magic bitboards.
#ifndef XADREZ_BITBOARD_H
#define XADREZ_BITBOARD_H

#include "types.h"

namespace xadrez {

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;
constexpr Bitboard RANK_1_BB = 0xFFULL;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;
// Casas escuras: a1 é escura, b1 é clara e assim por diante.
constexpr Bitboard DARK_SQUARES_BB = 0xAA55AA55AA55AA55ULL;

inline Bitboard file_bb(int file) { return FILE_A_BB << file; }
inline Bitboard rank_bb(int rank) { return RANK_1_BB << (8 * rank); }

// Contagem de bits ligados (técnica SWAR): soma os bits em pares, depois em grupos
// de 4 e de 8; a multiplicação final acumula os oito bytes no byte mais alto.
inline int popcount(Bitboard b) {
  b = b - ((b >> 1) & 0x5555555555555555ULL);
  b = (b & 0x3333333333333333ULL) + ((b >> 2) & 0x3333333333333333ULL);
  b = (b + (b >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
  return int((b * 0x0101010101010101ULL) >> 56);
}

namespace detail {
// Sequência de De Bruijn de ordem 6: multiplicar uma potência de 2 por ela deixa
// nos 6 bits mais altos um padrão diferente para cada posição do bit.
constexpr Bitboard DEBRUIJN64 = 0x03F79D71B4CB0A89ULL;
struct DeBruijnTable {
  int index[64];
  constexpr DeBruijnTable() : index() {
    for (int i = 0; i < 64; ++i) index[((Bitboard(1) << i) * DEBRUIJN64) >> 58] = i;
  }
};
inline constexpr DeBruijnTable DEBRUIJN{};
}  // namespace detail

// Índice do bit menos significativo (b não pode ser zero). b & (0 - b) isola esse bit.
inline Square lsb(Bitboard b) {
  return detail::DEBRUIJN.index[((b & (0 - b)) * detail::DEBRUIJN64) >> 58];
}

inline Square pop_lsb(Bitboard& b) {
  const Square s = lsb(b);
  b &= b - 1;
  return s;
}

inline bool more_than_one(Bitboard b) { return (b & (b - 1)) != 0; }

namespace attacks {

// Um número mágico por casa: (ocupação & máscara) * mágico >> deslocamento vira um
// índice sem colisões destrutivas na tabela de ataques daquela casa.
struct Magic {
  Bitboard mask = 0;
  Bitboard magic = 0;
  Bitboard* table = nullptr;
  unsigned shift = 0;
  unsigned index(Bitboard occupied) const {
    return unsigned(((occupied & mask) * magic) >> shift);
  }
};

extern Bitboard Knight[64];
extern Bitboard King[64];
extern Bitboard Pawn[2][64];      // casas atacadas por um peão da cor indicada
extern Bitboard Between[64][64];  // casas estritamente entre duas casas alinhadas
extern Bitboard Line[64][64];     // a linha inteira que passa pelas duas casas
extern Magic BishopMagics[64];
extern Magic RookMagics[64];

// Monta todas as tabelas. Pode ser chamada várias vezes; só a primeira trabalha.
void init();

inline Bitboard bishop(Square s, Bitboard occupied) {
  const Magic& m = BishopMagics[s];
  return m.table[m.index(occupied)];
}

inline Bitboard rook(Square s, Bitboard occupied) {
  const Magic& m = RookMagics[s];
  return m.table[m.index(occupied)];
}

inline Bitboard queen(Square s, Bitboard occupied) { return bishop(s, occupied) | rook(s, occupied); }

// Versão lenta, que percorre cada raio casa a casa. Serve para montar as tabelas
// mágicas e como referência nos testes.
Bitboard sliding_slow(PieceType pt, Square s, Bitboard occupied);

}  // namespace attacks
}  // namespace xadrez

#endif
