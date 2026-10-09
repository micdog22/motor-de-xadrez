#include "position.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace xadrez {

const char* const START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

namespace zobrist {
Key PieceSquare[12][64];
Key Castling[16];
Key EnPassant[8];
Key SideToMove;
}  // namespace zobrist

namespace {

const char PIECE_CHARS[] = "PNBRQKpnbrqk";

// Direitos que continuam valendo depois de um lance que sai ou chega nesta casa.
int CastlingMask[64];

// splitmix64: gerador simples e de boa qualidade para as chaves Zobrist.
std::uint64_t splitmix64(std::uint64_t& state) {
  std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

void init_tables() {
  std::uint64_t seed = 0x5EED2026ULL;
  for (auto& row : zobrist::PieceSquare)
    for (Key& k : row) k = splitmix64(seed);
  for (Key& k : zobrist::Castling) k = splitmix64(seed);
  for (Key& k : zobrist::EnPassant) k = splitmix64(seed);
  zobrist::SideToMove = splitmix64(seed);

  for (int& m : CastlingMask) m = ALL_CASTLING;
  CastlingMask[E1] &= ~(WHITE_OO | WHITE_OOO);
  CastlingMask[H1] &= ~WHITE_OO;
  CastlingMask[A1] &= ~WHITE_OOO;
  CastlingMask[E8] &= ~(BLACK_OO | BLACK_OOO);
  CastlingMask[H8] &= ~BLACK_OO;
  CastlingMask[A8] &= ~BLACK_OOO;
}

std::string square_text(Square s) {
  std::string t;
  t += char('a' + file_of(s));
  t += char('1' + rank_of(s));
  return t;
}

bool parse_number(const std::string& text, int minimum, int maximum, int& out) {
  if (text.empty() || text.size() > 6) return false;
  int value = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') return false;
    value = value * 10 + (ch - '0');
  }
  if (value < minimum || value > maximum) return false;
  out = value;
  return true;
}

}  // namespace

void init() {
  static const bool done = (attacks::init(), init_tables(), true);
  (void)done;
}

Position::Position() {
  init();
  clear();
  set_fen(START_FEN);
}

void Position::clear() {
  for (Bitboard& b : byType_) b = 0;
  for (Bitboard& b : byColor_) b = 0;
  for (Piece& p : board_) p = NO_PIECE;
  sideToMove_ = WHITE;
  fullmove_ = 1;
  states_.clear();
  states_.reserve(512);
  states_.push_back(State());
}

void Position::put_piece(Piece p, Square s) {
  const Bitboard b = square_bb(s);
  board_[s] = p;
  byType_[type_of(p)] |= b;
  byColor_[color_of(p)] |= b;
}

void Position::remove_piece(Square s) {
  const Piece p = board_[s];
  const Bitboard b = square_bb(s);
  byType_[type_of(p)] ^= b;
  byColor_[color_of(p)] ^= b;
  board_[s] = NO_PIECE;
}

void Position::move_piece(Square from, Square to) {
  const Piece p = board_[from];
  const Bitboard fromTo = square_bb(from) | square_bb(to);
  byType_[type_of(p)] ^= fromTo;
  byColor_[color_of(p)] ^= fromTo;
  board_[from] = NO_PIECE;
  board_[to] = p;
}

bool Position::set_fen(const std::string& fenText, std::string* error) {
  const Position saved = *this;
  auto fail = [&](const std::string& message) {
    *this = saved;
    if (error) *error = message;
    return false;
  };

  std::istringstream is(fenText);
  std::vector<std::string> fields;
  for (std::string field; is >> field;) fields.push_back(field);
  if (fields.size() < 4 || fields.size() > 6)
    return fail("a FEN precisa de 4 a 6 campos: tabuleiro, vez, roque, en passant, meios lances e lance");

  clear();

  // 1. Tabuleiro, da fileira 8 para a 1, com dígitos para casas vazias.
  int rank = 7, file = 0;
  for (char ch : fields[0]) {
    if (ch == '/') {
      if (file != 8) return fail("a fileira " + std::to_string(rank + 1) + " não tem 8 casas");
      if (--rank < 0) return fail("o tabuleiro tem mais de 8 fileiras");
      file = 0;
      continue;
    }
    if (ch >= '1' && ch <= '8') {
      file += ch - '0';
      if (file > 8) return fail("a fileira " + std::to_string(rank + 1) + " tem mais de 8 casas");
      continue;
    }
    const char* found = ch ? std::strchr(PIECE_CHARS, ch) : nullptr;
    if (!found) return fail(std::string("caractere inválido no tabuleiro: '") + ch + "'");
    if (file > 7) return fail("a fileira " + std::to_string(rank + 1) + " tem mais de 8 casas");
    put_piece(Piece(found - PIECE_CHARS), make_square(file, rank));
    ++file;
  }
  if (rank != 0 || file != 8) return fail("o tabuleiro precisa de 8 fileiras com 8 casas cada");

  // 2. Vez de jogar.
  if (fields[1] == "w")
    sideToMove_ = WHITE;
  else if (fields[1] == "b")
    sideToMove_ = BLACK;
  else
    return fail("a vez deve ser 'w' (brancas) ou 'b' (pretas)");

  // 3. Direitos de roque.
  State& s = states_.back();
  if (fields[2] != "-") {
    for (char ch : fields[2]) {
      const int bit = ch == 'K' ? WHITE_OO : ch == 'Q' ? WHITE_OOO : ch == 'k' ? BLACK_OO : ch == 'q' ? BLACK_OOO : 0;
      if (!bit || (s.castling & bit)) return fail("roque inválido: use '-' ou as letras KQkq sem repetir");
      s.castling |= bit;
    }
  }

  // 4. Casa de en passant.
  if (fields[3] != "-") {
    const std::string& t = fields[3];
    if (t.size() != 2 || t[0] < 'a' || t[0] > 'h' || t[1] < '1' || t[1] > '8')
      return fail("casa de en passant inválida: " + t);
    s.epSquare = make_square(t[0] - 'a', t[1] - '1');
  }

  // 5 e 6. Relógio dos 50 lances e número do lance.
  if (fields.size() >= 5 && !parse_number(fields[4], 0, 9999, s.halfmoveClock))
    return fail("contador de meios lances inválido: " + fields[4]);
  if (fields.size() >= 6 && !parse_number(fields[5], 1, 99999, fullmove_))
    return fail("número do lance inválido: " + fields[5]);

  std::string why;
  if (!validate(&why)) return fail(why);

  s.checkers = attackers_to(king_square(sideToMove_), pieces()) & pieces(~sideToMove_);
  s.epInKey = s.epSquare != NO_SQUARE && legal_ep_exists(s.epSquare);
  s.key = compute_key();
  return true;
}

bool Position::validate(std::string* error) const {
  auto fail = [&](const std::string& message) {
    if (error) *error = message;
    return false;
  };
  for (Color c : {WHITE, BLACK}) {
    const char* name = c == WHITE ? "brancas" : "pretas";
    if (popcount(pieces(c, KING)) != 1) return fail(std::string("as ") + name + " precisam ter exatamente um rei");
    if (popcount(pieces(c, PAWN)) > 8) return fail(std::string("as ") + name + " têm mais de 8 peões");
    if (popcount(pieces(c)) > 16) return fail(std::string("as ") + name + " têm mais de 16 peças");
  }
  if (pieces(PAWN) & (RANK_1_BB | RANK_8_BB)) return fail("há peão na primeira ou na última fileira");
  if (attacked_by(sideToMove_, king_square(~sideToMove_), pieces()))
    return fail("o rei do lado que não está com a vez está em xeque");

  const int c = st().castling;
  if ((c & WHITE_OO) && (board_[E1] != W_KING || board_[H1] != W_ROOK))
    return fail("roque K exige rei branco em e1 e torre em h1");
  if ((c & WHITE_OOO) && (board_[E1] != W_KING || board_[A1] != W_ROOK))
    return fail("roque Q exige rei branco em e1 e torre em a1");
  if ((c & BLACK_OO) && (board_[E8] != B_KING || board_[H8] != B_ROOK))
    return fail("roque k exige rei preto em e8 e torre em h8");
  if ((c & BLACK_OOO) && (board_[E8] != B_KING || board_[A8] != B_ROOK))
    return fail("roque q exige rei preto em e8 e torre em a8");

  const Square ep = st().epSquare;
  if (ep != NO_SQUARE) {
    const Color us = sideToMove_;
    const int forward = us == WHITE ? 8 : -8;  // direção em que o lado da vez avança
    if (rank_of(ep) != (us == WHITE ? 5 : 2))
      return fail("casa de en passant na fileira errada: " + square_text(ep));
    if (board_[ep] != NO_PIECE || board_[ep + forward] != NO_PIECE ||
        board_[ep - forward] != make_piece(~us, PAWN))
      return fail("a casa de en passant " + square_text(ep) + " não corresponde a um peão que avançou duas casas");
  }
  return true;
}

std::string Position::fen() const {
  std::string s;
  for (int r = 7; r >= 0; --r) {
    int empty = 0;
    for (int f = 0; f < 8; ++f) {
      const Piece p = board_[make_square(f, r)];
      if (p == NO_PIECE) {
        ++empty;
        continue;
      }
      if (empty) s += char('0' + empty);
      empty = 0;
      s += PIECE_CHARS[p];
    }
    if (empty) s += char('0' + empty);
    if (r) s += '/';
  }
  s += sideToMove_ == WHITE ? " w " : " b ";
  const int c = st().castling;
  if (!c) s += '-';
  if (c & WHITE_OO) s += 'K';
  if (c & WHITE_OOO) s += 'Q';
  if (c & BLACK_OO) s += 'k';
  if (c & BLACK_OOO) s += 'q';
  s += ' ';
  s += st().epSquare == NO_SQUARE ? std::string("-") : square_text(st().epSquare);
  s += ' ' + std::to_string(st().halfmoveClock) + ' ' + std::to_string(fullmove_);
  return s;
}

Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
  return (attacks::Pawn[BLACK][s] & pieces(WHITE, PAWN)) |
         (attacks::Pawn[WHITE][s] & pieces(BLACK, PAWN)) |
         (attacks::Knight[s] & pieces(KNIGHT)) |
         (attacks::bishop(s, occupied) & (pieces(BISHOP) | pieces(QUEEN))) |
         (attacks::rook(s, occupied) & (pieces(ROOK) | pieces(QUEEN))) |
         (attacks::King[s] & pieces(KING));
}

bool Position::attacked_by(Color c, Square s, Bitboard occupied) const {
  return (attacks::Pawn[~c][s] & pieces(c, PAWN)) || (attacks::Knight[s] & pieces(c, KNIGHT)) ||
         (attacks::King[s] & pieces(c, KING)) ||
         (attacks::bishop(s, occupied) & (pieces(c, BISHOP) | pieces(c, QUEEN))) ||
         (attacks::rook(s, occupied) & (pieces(c, ROOK) | pieces(c, QUEEN)));
}

namespace {
// Depois de capturar en passant (peão de from vai para ep e some o peão em captured),
// o rei de "us" fica atacado? Cobre a cravada na fileira, que nenhuma máscara de
// cravada comum detecta, porque duas peças saem da mesma linha ao mesmo tempo.
bool ep_leaves_king_safe(const Position& pos, Square from, Square ep) {
  const Color us = pos.side_to_move(), them = ~us;
  const Square captured = ep + (us == WHITE ? -8 : 8);
  const Square ksq = pos.king_square(us);
  const Bitboard occ = (pos.pieces() ^ square_bb(from) ^ square_bb(captured)) | square_bb(ep);
  return !((attacks::rook(ksq, occ) & (pos.pieces(them, ROOK) | pos.pieces(them, QUEEN))) ||
           (attacks::bishop(ksq, occ) & (pos.pieces(them, BISHOP) | pos.pieces(them, QUEEN))) ||
           (attacks::Knight[ksq] & pos.pieces(them, KNIGHT)) ||
           (attacks::Pawn[us][ksq] & pos.pieces(them, PAWN) & ~square_bb(captured)));
}
}  // namespace

bool Position::ep_capture_is_legal(Square from) const {
  return st().epSquare != NO_SQUARE && ep_leaves_king_safe(*this, from, st().epSquare);
}

bool Position::legal_ep_exists(Square ep) const {
  Bitboard candidates = attacks::Pawn[~sideToMove_][ep] & pieces(sideToMove_, PAWN);
  while (candidates) {
    if (ep_leaves_king_safe(*this, pop_lsb(candidates), ep)) return true;
  }
  return false;
}

bool Position::has_non_pawn_material(Color c) const {
  return (pieces(c) & ~pieces(PAWN) & ~pieces(KING)) != 0;
}

Key Position::compute_key() const {
  Key k = 0;
  for (Square s = 0; s < 64; ++s)
    if (board_[s] != NO_PIECE) k ^= zobrist::PieceSquare[board_[s]][s];
  k ^= zobrist::Castling[st().castling];
  if (sideToMove_ == BLACK) k ^= zobrist::SideToMove;
  if (st().epSquare != NO_SQUARE && legal_ep_exists(st().epSquare)) k ^= zobrist::EnPassant[file_of(st().epSquare)];
  return k;
}

void Position::make_move(Move m) {
  const Square from = move_from(m), to = move_to(m);
  const int flag = move_flag(m);
  const Color us = sideToMove_, them = ~us;
  const Piece pc = board_[from];

  State next = st();
  Key key = next.key ^ zobrist::SideToMove;
  if (next.epInKey) key ^= zobrist::EnPassant[file_of(next.epSquare)];
  next.epSquare = NO_SQUARE;
  next.epInKey = false;
  next.captured = NO_PIECE;
  ++next.halfmoveClock;
  ++next.pliesFromNull;

  if (flag == FLAG_EP_CAPTURE) {
    const Square capturedSq = to + (us == WHITE ? -8 : 8);
    next.captured = board_[capturedSq];
    key ^= zobrist::PieceSquare[next.captured][capturedSq];
    remove_piece(capturedSq);
  } else if (is_capture(m)) {
    next.captured = board_[to];
    key ^= zobrist::PieceSquare[next.captured][to];
    remove_piece(to);
  }
  if (next.captured != NO_PIECE) next.halfmoveClock = 0;

  if (is_castle(m)) {
    const Square rookFrom = flag == FLAG_KING_CASTLE ? from + 3 : from - 4;
    const Square rookTo = flag == FLAG_KING_CASTLE ? from + 1 : from - 1;
    const Piece rook = board_[rookFrom];
    key ^= zobrist::PieceSquare[rook][rookFrom] ^ zobrist::PieceSquare[rook][rookTo];
    move_piece(rookFrom, rookTo);
  }

  key ^= zobrist::PieceSquare[pc][from] ^ zobrist::PieceSquare[pc][to];
  move_piece(from, to);

  if (type_of(pc) == PAWN) {
    next.halfmoveClock = 0;
    if (is_promotion(m)) {
      const Piece promoted = make_piece(us, promotion_type(m));
      key ^= zobrist::PieceSquare[pc][to] ^ zobrist::PieceSquare[promoted][to];
      remove_piece(to);
      put_piece(promoted, to);
    } else if (flag == FLAG_DOUBLE_PUSH) {
      next.epSquare = (from + to) / 2;
    }
  }

  const int castling = next.castling & CastlingMask[from] & CastlingMask[to];
  if (castling != next.castling) {
    key ^= zobrist::Castling[next.castling] ^ zobrist::Castling[castling];
    next.castling = castling;
  }

  sideToMove_ = them;
  if (us == BLACK) ++fullmove_;

  next.checkers = attackers_to(king_square(them), pieces()) & pieces(us);
  if (next.epSquare != NO_SQUARE && legal_ep_exists(next.epSquare)) {
    next.epInKey = true;
    key ^= zobrist::EnPassant[file_of(next.epSquare)];
  }
  next.key = key;
  states_.push_back(next);
}

void Position::unmake_move(Move m) {
  const Square from = move_from(m), to = move_to(m);
  const int flag = move_flag(m);
  const Piece captured = st().captured;

  sideToMove_ = ~sideToMove_;
  const Color us = sideToMove_;
  if (us == BLACK) --fullmove_;

  if (is_promotion(m)) {
    remove_piece(to);
    put_piece(make_piece(us, PAWN), to);
  }
  move_piece(to, from);

  if (is_castle(m)) {
    const Square rookFrom = flag == FLAG_KING_CASTLE ? from + 3 : from - 4;
    const Square rookTo = flag == FLAG_KING_CASTLE ? from + 1 : from - 1;
    move_piece(rookTo, rookFrom);
  }

  if (captured != NO_PIECE) {
    const Square capturedSq = flag == FLAG_EP_CAPTURE ? to + (us == WHITE ? -8 : 8) : to;
    put_piece(captured, capturedSq);
  }
  states_.pop_back();
}

// Lance nulo: passa a vez sem mexer peças. Só é usado fora de xeque, então o
// adversário também não está em xeque depois dele.
void Position::make_null_move() {
  State next = st();
  next.key ^= zobrist::SideToMove;
  if (next.epInKey) next.key ^= zobrist::EnPassant[file_of(next.epSquare)];
  next.epSquare = NO_SQUARE;
  next.epInKey = false;
  next.captured = NO_PIECE;
  next.checkers = 0;
  ++next.halfmoveClock;
  next.pliesFromNull = 0;
  sideToMove_ = ~sideToMove_;
  states_.push_back(next);
}

void Position::unmake_null_move() {
  states_.pop_back();
  sideToMove_ = ~sideToMove_;
}

int Position::repetition_count() const {
  const int n = int(states_.size());
  const int window = std::min(st().halfmoveClock, st().pliesFromNull);
  int count = 0;
  // Só posições com o mesmo lado na vez (de dois em dois) e depois do último
  // lance irreversível (captura, lance de peão ou perda de direito de roque).
  for (int i = n - 3; i >= 0 && i >= n - 1 - window; i -= 2) {
    if (states_[i].key == st().key) ++count;
  }
  return count;
}

bool Position::is_insufficient_material() const {
  if (pieces(PAWN) | pieces(ROOK) | pieces(QUEEN)) return false;
  const Bitboard minors = pieces(KNIGHT) | pieces(BISHOP);
  if (popcount(minors) <= 1) return true;  // rei contra rei, ou rei e uma peça menor
  if (pieces(KNIGHT)) return false;        // com cavalos ainda existe mate possível
  // Só bispos, todos na mesma cor de casa: nenhum mate é possível.
  const Bitboard bishops = pieces(BISHOP);
  return (bishops & DARK_SQUARES_BB) == 0 || (bishops & ~DARK_SQUARES_BB) == 0;
}

}  // namespace xadrez
