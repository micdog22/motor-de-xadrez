// Tipos básicos do motor: cores, peças, casas, lances e constantes.
#ifndef XADREZ_TYPES_H
#define XADREZ_TYPES_H

#include <cstdint>

namespace xadrez {

using Bitboard = std::uint64_t;
using Key = std::uint64_t;
using Square = int;
using Move = std::uint16_t;

enum Color : int { WHITE = 0, BLACK = 1 };
inline Color operator~(Color c) { return Color(c ^ 1); }

enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE_TYPE };

enum Piece : int {
  W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
  B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  NO_PIECE
};

inline Piece make_piece(Color c, PieceType pt) { return Piece(int(c) * 6 + int(pt)); }
inline Color color_of(Piece p) { return Color(int(p) / 6); }
inline PieceType type_of(Piece p) { return PieceType(int(p) % 6); }

// Casas numeradas de 0 (a1) a 63 (h8): casa = 8 * fileira + coluna.
enum : int {
  A1 = 0, B1, C1, D1, E1, F1, G1, H1,
  A8 = 56, B8, C8, D8, E8, F8, G8, H8,
  NO_SQUARE = 64
};

inline int file_of(Square s) { return s & 7; }
inline int rank_of(Square s) { return s >> 3; }
inline Square make_square(int file, int rank) { return rank * 8 + file; }
inline Bitboard square_bb(Square s) { return Bitboard(1) << s; }
// Fileira vista pelo lado c: a primeira fileira de cada jogador vale 0.
inline int relative_rank(Color c, Square s) { return c == WHITE ? rank_of(s) : 7 - rank_of(s); }

// Direitos de roque: um bit para cada um.
enum : int { WHITE_OO = 1, WHITE_OOO = 2, BLACK_OO = 4, BLACK_OOO = 8, ALL_CASTLING = 15 };

// Um lance cabe em 16 bits: origem (bits 0 a 5), destino (6 a 11) e tipo (12 a 15).
// O bit 2 do tipo marca captura e o bit 3 marca promoção, como na wiki de programação de xadrez.
enum MoveFlag : int {
  FLAG_QUIET = 0,
  FLAG_DOUBLE_PUSH = 1,
  FLAG_KING_CASTLE = 2,
  FLAG_QUEEN_CASTLE = 3,
  FLAG_CAPTURE = 4,
  FLAG_EP_CAPTURE = 5,
  FLAG_PROMO = 8,          // 8 a 11: promoção a cavalo, bispo, torre ou dama
  FLAG_PROMO_CAPTURE = 12  // 12 a 15: promoção com captura
};

// a1a1 nunca é um lance válido, então o valor zero serve como "nenhum lance".
constexpr Move MOVE_NONE = 0;

inline Move make_move(Square from, Square to, int flag) { return Move(from | (to << 6) | (flag << 12)); }
inline Square move_from(Move m) { return m & 63; }
inline Square move_to(Move m) { return (m >> 6) & 63; }
inline int move_flag(Move m) { return m >> 12; }
inline bool is_capture(Move m) { return (move_flag(m) & 4) != 0; }
inline bool is_promotion(Move m) { return (move_flag(m) & 8) != 0; }
inline bool is_castle(Move m) {
  return move_flag(m) == FLAG_KING_CASTLE || move_flag(m) == FLAG_QUEEN_CASTLE;
}
inline PieceType promotion_type(Move m) { return PieceType(KNIGHT + (move_flag(m) & 3)); }

constexpr int MAX_PLY = 128;    // profundidade máxima da busca, em meios lances
constexpr int MAX_MOVES = 256;  // nenhuma posição legal tem mais de 218 lances

}  // namespace xadrez

#endif
