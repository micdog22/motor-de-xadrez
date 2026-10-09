#include "movegen.h"

namespace xadrez {
namespace {

void push_promotions(MoveList& list, Square from, Square to, bool capture, bool onlyQueen) {
  const int base = capture ? FLAG_PROMO_CAPTURE : FLAG_PROMO;
  list.push(make_move(from, to, base + (QUEEN - KNIGHT)));
  if (onlyQueen) return;
  list.push(make_move(from, to, base + (ROOK - KNIGHT)));
  list.push(make_move(from, to, base + (BISHOP - KNIGHT)));
  list.push(make_move(from, to, base + (KNIGHT - KNIGHT)));
}

void push_targets(MoveList& list, Square from, Bitboard targets, Bitboard enemy) {
  while (targets) {
    const Square to = pop_lsb(targets);
    list.push(make_move(from, to, (enemy & square_bb(to)) ? FLAG_CAPTURE : FLAG_QUIET));
  }
}

// Gerador legal direto, sem "faz o lance e confere o rei":
//  1. o rei vai para casas não atacadas (calculadas sem o próprio rei no tabuleiro,
//     para que ele não se esconda atrás de si mesmo na linha de uma torre);
//  2. em xeque duplo só o rei se move;
//  3. em xeque simples as outras peças só podem capturar o atacante ou bloquear;
//  4. peças cravadas só andam sobre a linha que as liga ao rei;
//  5. en passant é testado simulando o tabuleiro, por causa do xeque descoberto na fileira.
template <bool NoisyOnly>
void generate(const Position& pos, MoveList& list) {
  const Color us = pos.side_to_move(), them = ~us;
  const Square ksq = pos.king_square(us);
  const Bitboard occ = pos.pieces();
  const Bitboard own = pos.pieces(us), enemy = pos.pieces(them);
  const Bitboard checkers = pos.checkers();

  // 1. Rei.
  Bitboard kingTargets = attacks::King[ksq] & ~own;
  if (NoisyOnly) kingTargets &= enemy;
  const Bitboard occWithoutKing = occ ^ square_bb(ksq);
  while (kingTargets) {
    const Square to = pop_lsb(kingTargets);
    if (!pos.attacked_by(them, to, occWithoutKing))
      list.push(make_move(ksq, to, (enemy & square_bb(to)) ? FLAG_CAPTURE : FLAG_QUIET));
  }

  // 2. Xeque duplo.
  if (more_than_one(checkers)) return;

  // 3. Casas permitidas para as outras peças.
  Bitboard target = ~own;
  if (checkers) target = attacks::Between[ksq][lsb(checkers)] | checkers;
  const Bitboard pieceTarget = NoisyOnly ? (target & enemy) : target;

  // 4. Cravadas: atiradores inimigos alinhados com o rei com exatamente uma peça
  // nossa no meio.
  Bitboard pinned = 0;
  Bitboard snipers =
      (attacks::rook(ksq, 0) & (pos.pieces(them, ROOK) | pos.pieces(them, QUEEN))) |
      (attacks::bishop(ksq, 0) & (pos.pieces(them, BISHOP) | pos.pieces(them, QUEEN)));
  while (snipers) {
    const Bitboard between = attacks::Between[ksq][pop_lsb(snipers)] & occ;
    if (between && !more_than_one(between) && (between & own)) pinned |= between;
  }

  // Cavalo cravado nunca se move: qualquer salto sai da linha.
  Bitboard knights = pos.pieces(us, KNIGHT) & ~pinned;
  while (knights) {
    const Square from = pop_lsb(knights);
    push_targets(list, from, attacks::Knight[from] & pieceTarget, enemy);
  }

  Bitboard diagonal = pos.pieces(us, BISHOP) | pos.pieces(us, QUEEN);
  while (diagonal) {
    const Square from = pop_lsb(diagonal);
    Bitboard t = attacks::bishop(from, occ) & pieceTarget;
    if (pinned & square_bb(from)) t &= attacks::Line[ksq][from];
    push_targets(list, from, t, enemy);
  }

  Bitboard straight = pos.pieces(us, ROOK) | pos.pieces(us, QUEEN);
  while (straight) {
    const Square from = pop_lsb(straight);
    Bitboard t = attacks::rook(from, occ) & pieceTarget;
    if (pinned & square_bb(from)) t &= attacks::Line[ksq][from];
    push_targets(list, from, t, enemy);
  }

  // Peões.
  const int up = us == WHITE ? 8 : -8;
  const Bitboard promotionRank = us == WHITE ? RANK_8_BB : RANK_1_BB;
  const int startRank = us == WHITE ? 1 : 6;
  const Square ep = pos.ep_square();
  Bitboard pawns = pos.pieces(us, PAWN);
  while (pawns) {
    const Square from = pop_lsb(pawns);
    const Bitboard allowed = (pinned & square_bb(from)) ? attacks::Line[ksq][from] : ~Bitboard(0);

    const Square to = from + up;
    if (!(occ & square_bb(to))) {
      if (square_bb(to) & target & allowed) {
        if (square_bb(to) & promotionRank)
          push_promotions(list, from, to, false, NoisyOnly);
        else if (!NoisyOnly)
          list.push(make_move(from, to, FLAG_QUIET));
      }
      if (!NoisyOnly && rank_of(from) == startRank) {
        const Square to2 = to + up;
        if (!(occ & square_bb(to2)) && (square_bb(to2) & target & allowed))
          list.push(make_move(from, to2, FLAG_DOUBLE_PUSH));
      }
    }

    Bitboard captures = attacks::Pawn[us][from] & enemy & target & allowed;
    while (captures) {
      const Square capTo = pop_lsb(captures);
      if (square_bb(capTo) & promotionRank)
        push_promotions(list, from, capTo, true, NoisyOnly);
      else
        list.push(make_move(from, capTo, FLAG_CAPTURE));
    }

    if (ep != NO_SQUARE && (attacks::Pawn[us][from] & square_bb(ep)) && pos.ep_capture_is_legal(from))
      list.push(make_move(from, ep, FLAG_EP_CAPTURE));
  }

  // Roque: o rei não pode estar em xeque, nem passar ou chegar em casa atacada.
  if (!NoisyOnly && !checkers) {
    const int rights = pos.castling_rights();
    const int shift = us == WHITE ? 0 : 56;
    const int oo = us == WHITE ? WHITE_OO : BLACK_OO;
    const int ooo = us == WHITE ? WHITE_OOO : BLACK_OOO;
    const Square e = E1 + shift, f = F1 + shift, g = G1 + shift;
    const Square d = D1 + shift, c = C1 + shift, b = B1 + shift;
    if ((rights & oo) && !(occ & (square_bb(f) | square_bb(g))) && !pos.attacked_by(them, f, occ) &&
        !pos.attacked_by(them, g, occ))
      list.push(make_move(e, g, FLAG_KING_CASTLE));
    if ((rights & ooo) && !(occ & (square_bb(d) | square_bb(c) | square_bb(b))) &&
        !pos.attacked_by(them, d, occ) && !pos.attacked_by(them, c, occ))
      list.push(make_move(e, c, FLAG_QUEEN_CASTLE));
  }
}

}  // namespace

void generate_legal(const Position& pos, MoveList& list) {
  list.count = 0;
  generate<false>(pos, list);
}

void generate_noisy(const Position& pos, MoveList& list) {
  list.count = 0;
  generate<true>(pos, list);
}

std::uint64_t perft(Position& pos, int depth) {
  if (depth <= 0) return 1;
  MoveList list;
  generate_legal(pos, list);
  // Contagem em bloco: na última camada basta saber quantos lances legais existem.
  if (depth == 1) return std::uint64_t(list.size());
  std::uint64_t total = 0;
  for (Move m : list) {
    pos.make_move(m);
    total += perft(pos, depth - 1);
    pos.unmake_move(m);
  }
  return total;
}

GameResult game_result(const Position& pos) {
  MoveList list;
  generate_legal(pos, list);
  // O mate vale mais que a regra dos 50 lances, por isso vem primeiro.
  if (list.size() == 0) return pos.in_check() ? GameResult::Checkmate : GameResult::Stalemate;
  if (pos.is_insufficient_material()) return GameResult::InsufficientMaterial;
  if (pos.repetition_count() >= 2) return GameResult::Threefold;
  if (pos.halfmove_clock() >= 100) return GameResult::FiftyMoves;
  return GameResult::Ongoing;
}

}  // namespace xadrez
