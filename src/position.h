// Posição: tabuleiro em bitboards, FEN, fazer e desfazer lances, Zobrist e regras de empate.
#ifndef XADREZ_POSITION_H
#define XADREZ_POSITION_H

#include <string>
#include <vector>

#include "bitboard.h"

namespace xadrez {

namespace zobrist {
extern Key PieceSquare[12][64];
extern Key Castling[16];
extern Key EnPassant[8];
extern Key SideToMove;
}  // namespace zobrist

// Monta tabelas de ataque e chaves Zobrist. Pode ser chamada mais de uma vez.
void init();

extern const char* const START_FEN;

class Position {
 public:
  Position();  // posição inicial
  // Lê uma FEN. Em caso de erro devolve false, explica em *error e não altera a posição.
  bool set_fen(const std::string& fen, std::string* error = nullptr);
  std::string fen() const;

  Piece piece_on(Square s) const { return board_[s]; }
  Bitboard pieces() const { return byColor_[WHITE] | byColor_[BLACK]; }
  Bitboard pieces(Color c) const { return byColor_[c]; }
  Bitboard pieces(PieceType pt) const { return byType_[pt]; }
  Bitboard pieces(Color c, PieceType pt) const { return byColor_[c] & byType_[pt]; }
  Color side_to_move() const { return sideToMove_; }
  Square king_square(Color c) const { return lsb(pieces(c, KING)); }

  Bitboard checkers() const { return st().checkers; }
  bool in_check() const { return st().checkers != 0; }
  int castling_rights() const { return st().castling; }
  Square ep_square() const { return st().epSquare; }
  int halfmove_clock() const { return st().halfmoveClock; }
  int fullmove_number() const { return fullmove_; }
  Key key() const { return st().key; }
  int plies_played() const { return int(states_.size()) - 1; }

  // Peças das duas cores que atacam a casa s, com a ocupação occupied.
  Bitboard attackers_to(Square s, Bitboard occupied) const;
  bool attacked_by(Color c, Square s, Bitboard occupied) const;
  // A captura en passant do peão em from é legal? (simula o tabuleiro sem os dois peões)
  bool ep_capture_is_legal(Square from) const;
  bool has_non_pawn_material(Color c) const;

  void make_move(Move m);
  void unmake_move(Move m);
  void make_null_move();
  void unmake_null_move();

  // Quantas vezes a posição atual já apareceu antes, desde o último lance irreversível.
  int repetition_count() const;
  bool is_repetition() const { return repetition_count() > 0; }
  bool is_insufficient_material() const;

  // Chave calculada do zero, para conferir a versão incremental nos testes.
  Key compute_key() const;

 private:
  // Tudo o que não dá para recalcular ao desfazer um lance fica aqui.
  struct State {
    Key key = 0;
    Bitboard checkers = 0;
    int castling = 0;
    Square epSquare = NO_SQUARE;  // casa de en passant como na FEN
    bool epInKey = false;         // só entra na chave se a captura for realmente legal
    int halfmoveClock = 0;
    int pliesFromNull = 0;
    Piece captured = NO_PIECE;
  };

  const State& st() const { return states_.back(); }
  void clear();
  void put_piece(Piece p, Square s);
  void remove_piece(Square s);
  void move_piece(Square from, Square to);
  bool legal_ep_exists(Square ep) const;
  bool validate(std::string* error) const;

  Bitboard byType_[6];
  Bitboard byColor_[2];
  Piece board_[64];
  Color sideToMove_ = WHITE;
  int fullmove_ = 1;
  std::vector<State> states_;
};

}  // namespace xadrez

#endif
