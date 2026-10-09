#include "notation.h"

namespace xadrez {

const char* piece_letters(Notation n) { return n == Notation::Portuguese ? "PCBTDR" : "PNBRQK"; }

std::string square_name(Square s) {
  std::string t;
  t += char('a' + file_of(s));
  t += char('1' + rank_of(s));
  return t;
}

Square parse_square(const std::string& text) {
  if (text.size() != 2 || text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8') return NO_SQUARE;
  return make_square(text[0] - 'a', text[1] - '1');
}

std::string move_to_uci(Move m) {
  if (m == MOVE_NONE) return "0000";
  std::string s = square_name(move_from(m)) + square_name(move_to(m));
  if (is_promotion(m)) s += "nbrq"[promotion_type(m) - KNIGHT];
  return s;
}

bool looks_like_uci(const std::string& text) {
  if (text.size() != 4 && text.size() != 5) return false;
  if (parse_square(text.substr(0, 2)) == NO_SQUARE || parse_square(text.substr(2, 2)) == NO_SQUARE) return false;
  return text.size() == 4 || std::string("nbrq").find(text[4]) != std::string::npos;
}

Move parse_uci(const Position& pos, const std::string& text) {
  if (!looks_like_uci(text)) return MOVE_NONE;
  const Square from = parse_square(text.substr(0, 2));
  const Square to = parse_square(text.substr(2, 2));
  MoveList list;
  generate_legal(pos, list);
  for (Move m : list) {
    if (move_from(m) != from || move_to(m) != to) continue;
    if (is_promotion(m) != (text.size() == 5)) continue;
    if (is_promotion(m) && "nbrq"[promotion_type(m) - KNIGHT] != text[4]) continue;
    return m;
  }
  return MOVE_NONE;
}

std::string move_to_san(Position& pos, Move m, Notation n) {
  const char* letters = piece_letters(n);
  const Square from = move_from(m), to = move_to(m);
  std::string s;
  if (is_castle(m)) {
    s = move_flag(m) == FLAG_KING_CASTLE ? "O-O" : "O-O-O";
  } else {
    const PieceType pt = type_of(pos.piece_on(from));
    if (pt == PAWN) {
      if (is_capture(m)) {
        s += char('a' + file_of(from));
        s += 'x';
      }
      s += square_name(to);
      if (is_promotion(m)) {
        s += '=';
        s += letters[promotion_type(m)];
      }
    } else {
      s += letters[pt];
      // Desambiguação: outra peça do mesmo tipo também chega na casa de destino?
      MoveList list;
      generate_legal(pos, list);
      bool ambiguous = false, sameFile = false, sameRank = false;
      for (Move other : list) {
        const Square of = move_from(other);
        if (other == m || move_to(other) != to || of == from) continue;
        if (type_of(pos.piece_on(of)) != pt) continue;
        ambiguous = true;
        if (file_of(of) == file_of(from)) sameFile = true;
        if (rank_of(of) == rank_of(from)) sameRank = true;
      }
      if (ambiguous) {
        if (!sameFile)
          s += char('a' + file_of(from));
        else if (!sameRank)
          s += char('1' + rank_of(from));
        else
          s += square_name(from);
      }
      if (is_capture(m)) s += 'x';
      s += square_name(to);
    }
  }
  pos.make_move(m);
  if (pos.in_check()) {
    MoveList replies;
    generate_legal(pos, replies);
    s += replies.size() ? '+' : '#';
  }
  pos.unmake_move(m);
  return s;
}

std::string line_to_san(const Position& pos, const std::vector<Move>& line, Notation n) {
  Position p = pos;
  std::string text;
  for (Move m : line) {
    if (!text.empty()) text += ' ';
    text += move_to_san(p, m, n);
    p.make_move(m);
  }
  return text;
}

SanStatus parse_san(const Position& pos, const std::string& input, Notation n, Move& out) {
  out = MOVE_NONE;
  std::string s = input;
  while (!s.empty() && std::string("+#!? ").find(s.back()) != std::string::npos) s.pop_back();
  while (!s.empty() && s.front() == ' ') s.erase(s.begin());
  if (s.empty()) return SanStatus::Invalid;

  MoveList list;
  generate_legal(pos, list);

  // Roque, com a letra O ou com o dígito zero.
  std::string castle;
  for (char ch : s) castle += (ch == '0' || ch == 'o') ? 'O' : ch;
  if (castle == "O-O" || castle == "O-O-O") {
    const int flag = castle == "O-O" ? FLAG_KING_CASTLE : FLAG_QUEEN_CASTLE;
    for (Move m : list)
      if (move_flag(m) == flag) {
        out = m;
        return SanStatus::Ok;
      }
    return SanStatus::NotFound;
  }

  const std::string letters = piece_letters(n);
  auto piece_from_letter = [&](char ch) {
    const std::size_t i = letters.find(ch);
    return (i == std::string::npos || i == 0) ? NO_PIECE_TYPE : PieceType(i);
  };

  // Peça (maiúscula); sem letra é peão. Colunas são sempre minúsculas.
  PieceType pt = PAWN;
  std::size_t pos0 = 0;
  if (s[0] >= 'A' && s[0] <= 'Z') {
    pt = piece_from_letter(s[0]);
    if (pt == NO_PIECE_TYPE || pt == PAWN) return SanStatus::Invalid;
    pos0 = 1;
  }

  // Promoção no fim: "=D", "=Q" ou só a letra.
  PieceType promo = NO_PIECE_TYPE;
  if (s.size() > pos0 && s.back() >= 'A' && s.back() <= 'Z') {
    promo = piece_from_letter(s.back());
    if (promo == NO_PIECE_TYPE || promo == PAWN || promo == KING || pt != PAWN) return SanStatus::Invalid;
    s.pop_back();
    if (!s.empty() && s.back() == '=') s.pop_back();
  }

  // Destino: as duas últimas posições.
  if (s.size() < pos0 + 2) return SanStatus::Invalid;
  const Square to = parse_square(s.substr(s.size() - 2));
  if (to == NO_SQUARE) return SanStatus::Invalid;

  // O que sobra no meio: coluna e/ou fileira de origem, x ou : de captura, hífen.
  int fromFile = -1, fromRank = -1;
  for (std::size_t i = pos0; i + 2 < s.size(); ++i) {
    const char ch = s[i];
    if (ch == 'x' || ch == ':' || ch == '-') continue;
    if (ch >= 'a' && ch <= 'h' && fromFile < 0 && fromRank < 0)
      fromFile = ch - 'a';
    else if (ch >= '1' && ch <= '8' && fromRank < 0)
      fromRank = ch - '1';
    else
      return SanStatus::Invalid;
  }
  // Lance de peão sem coluna de origem é um avanço: "d5" nunca significa "cxd5".
  if (pt == PAWN && fromFile < 0) fromFile = file_of(to);

  int matches = 0;
  for (Move m : list) {
    const Square from = move_from(m);
    if (move_to(m) != to || type_of(pos.piece_on(from)) != pt || is_castle(m)) continue;
    if (fromFile >= 0 && file_of(from) != fromFile) continue;
    if (fromRank >= 0 && rank_of(from) != fromRank) continue;
    if (is_promotion(m) && promotion_type(m) != (promo == NO_PIECE_TYPE ? QUEEN : promo)) continue;
    if (!is_promotion(m) && promo != NO_PIECE_TYPE) continue;
    ++matches;
    out = m;
  }
  if (matches == 0) return SanStatus::NotFound;
  if (matches > 1) {
    out = MOVE_NONE;
    return SanStatus::Ambiguous;
  }
  return SanStatus::Ok;
}

std::string board_diagram(const Position& pos, Color perspective, Notation n, bool unicode) {
  static const char* const glyphs[12] = {"\u2659", "\u2658", "\u2657", "\u2656", "\u2655", "\u2654",
                                         "\u265F", "\u265E", "\u265D", "\u265C", "\u265B", "\u265A"};
  const char* letters = piece_letters(n);
  std::string out;
  for (int row = 0; row < 8; ++row) {
    const int rank = perspective == WHITE ? 7 - row : row;
    out += "  ";
    out += char('1' + rank);
    out += " ";
    for (int col = 0; col < 8; ++col) {
      const int file = perspective == WHITE ? col : 7 - col;
      const Piece p = pos.piece_on(make_square(file, rank));
      out += ' ';
      if (p == NO_PIECE)
        out += '.';
      else if (unicode)
        out += glyphs[p];
      else
        out += color_of(p) == WHITE ? letters[type_of(p)] : char(letters[type_of(p)] - 'A' + 'a');
    }
    out += '\n';
  }
  out += "    ";
  for (int col = 0; col < 8; ++col) {
    out += ' ';
    out += char('a' + (perspective == WHITE ? col : 7 - col));
  }
  out += '\n';
  return out;
}

}  // namespace xadrez
