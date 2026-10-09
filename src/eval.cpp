#include "eval.h"

#include <algorithm>
#include <cstdio>

namespace xadrez {
namespace {

// Pontuação dupla: meio-jogo (mg) e final (eg). A avaliação final interpola as duas
// conforme o material que ainda existe no tabuleiro (avaliação "tapered").
struct Score {
  int mg = 0;
  int eg = 0;
};

constexpr Score S(int mg, int eg) { return Score{mg, eg}; }
Score& operator+=(Score& a, Score b) {
  a.mg += b.mg;
  a.eg += b.eg;
  return a;
}
Score operator*(Score a, int k) { return S(a.mg * k, a.eg * k); }

const Score PieceValue[6] = {S(100, 125), S(320, 290), S(335, 310), S(480, 530), S(960, 980), S(0, 0)};

// Tabelas de casas do ponto de vista das brancas, com a fileira 8 em cima (como se lê
// um diagrama). Para uma peça branca na casa s o índice é s ^ 56; para uma preta, s.
// Os valores seguem a ideia da "Simplified Evaluation Function" de Tomasz Michniewski.
// clang-format off
const int PawnMg[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
   50,  50,  50,  50,  50,  50,  50,  50,
   10,  10,  20,  30,  30,  20,  10,  10,
    5,   5,  10,  25,  25,  10,   5,   5,
    0,   0,   0,  20,  20,   0,   0,   0,
    5,  -5, -10,   0,   0, -10,  -5,   5,
    5,  10,  10, -20, -20,  10,  10,   5,
    0,   0,   0,   0,   0,   0,   0,   0};
const int PawnEg[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
   60,  60,  60,  60,  60,  60,  60,  60,
   35,  35,  35,  35,  35,  35,  35,  35,
   20,  20,  20,  20,  20,  20,  20,  20,
   10,  10,  10,  10,  10,  10,  10,  10,
    5,   5,   5,   5,   5,   5,   5,   5,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0};
const int KnightPst[64] = {
  -50, -40, -30, -30, -30, -30, -40, -50,
  -40, -20,   0,   0,   0,   0, -20, -40,
  -30,   0,  10,  15,  15,  10,   0, -30,
  -30,   5,  15,  20,  20,  15,   5, -30,
  -30,   0,  15,  20,  20,  15,   0, -30,
  -30,   5,  10,  15,  15,  10,   5, -30,
  -40, -20,   0,   5,   5,   0, -20, -40,
  -50, -40, -30, -30, -30, -30, -40, -50};
const int BishopPst[64] = {
  -20, -10, -10, -10, -10, -10, -10, -20,
  -10,   0,   0,   0,   0,   0,   0, -10,
  -10,   0,   5,  10,  10,   5,   0, -10,
  -10,   5,   5,  10,  10,   5,   5, -10,
  -10,   0,  10,  10,  10,  10,   0, -10,
  -10,  10,  10,  10,  10,  10,  10, -10,
  -10,   5,   0,   0,   0,   0,   5, -10,
  -20, -10, -10, -10, -10, -10, -10, -20};
const int RookPst[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
    5,  10,  10,  10,  10,  10,  10,   5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
   -5,   0,   0,   0,   0,   0,   0,  -5,
    0,   0,   0,   5,   5,   0,   0,   0};
const int QueenPst[64] = {
  -20, -10, -10,  -5,  -5, -10, -10, -20,
  -10,   0,   0,   0,   0,   0,   0, -10,
  -10,   0,   5,   5,   5,   5,   0, -10,
   -5,   0,   5,   5,   5,   5,   0,  -5,
    0,   0,   5,   5,   5,   5,   0,  -5,
  -10,   5,   5,   5,   5,   5,   0, -10,
  -10,   0,   5,   0,   0,   0,   0, -10,
  -20, -10, -10,  -5,  -5, -10, -10, -20};
const int KingMg[64] = {
  -30, -40, -40, -50, -50, -40, -40, -30,
  -30, -40, -40, -50, -50, -40, -40, -30,
  -30, -40, -40, -50, -50, -40, -40, -30,
  -30, -40, -40, -50, -50, -40, -40, -30,
  -20, -30, -30, -40, -40, -30, -30, -20,
  -10, -20, -20, -20, -20, -20, -20, -10,
   20,  20,   0,   0,   0,   0,  20,  20,
   20,  30,  10,   0,   0,  10,  30,  20};
const int KingEg[64] = {
  -50, -40, -30, -20, -20, -30, -40, -50,
  -30, -20, -10,   0,   0, -10, -20, -30,
  -30, -10,  20,  30,  30,  20, -10, -30,
  -30, -10,  30,  40,  40,  30, -10, -30,
  -30, -10,  30,  40,  40,  30, -10, -30,
  -30, -10,  20,  30,  30,  20, -10, -30,
  -30, -30,   0,   0,   0,   0, -30, -30,
  -50, -30, -30, -30, -30, -30, -30, -50};
// clang-format on

const int* const PstMg[6] = {PawnMg, KnightPst, BishopPst, RookPst, QueenPst, KingMg};
const int* const PstEg[6] = {PawnEg, KnightPst, BishopPst, RookPst, QueenPst, KingEg};

const Score Doubled = S(-10, -20);
const Score Isolated = S(-10, -15);
const Score Passed[8] = {S(0, 0),   S(5, 10),  S(10, 15), S(15, 25),
                         S(30, 45), S(50, 75), S(80, 120), S(0, 0)};
const Score BishopPair = S(30, 50);
const Score RookOpenFile = S(25, 10);
const Score RookSemiOpenFile = S(12, 6);
// Bônus por casa alcançável e quantas casas são "normais" para cada peça.
const Score MobilityWeight[6] = {S(0, 0), S(4, 4), S(5, 5), S(2, 4), S(1, 2), S(0, 0)};
const int MobilityBase[6] = {0, 4, 6, 7, 13, 0};
const int KingAttackWeight[6] = {0, 2, 2, 3, 5, 0};
const int PhaseWeight[6] = {0, 1, 1, 2, 4, 0};
constexpr int MaxPhase = 24;
constexpr int Tempo = 10;

Bitboard PassedMask[2][64];  // casas à frente nas três colunas: sem peão inimigo nelas, o peão é passado
Bitboard AdjacentFiles[8];

void init_masks() {
  for (int f = 0; f < 8; ++f)
    AdjacentFiles[f] = (f > 0 ? file_bb(f - 1) : 0) | (f < 7 ? file_bb(f + 1) : 0);
  for (Square s = 0; s < 64; ++s) {
    const Bitboard files = file_bb(file_of(s)) | AdjacentFiles[file_of(s)];
    Bitboard aheadWhite = 0, aheadBlack = 0;
    for (int r = rank_of(s) + 1; r < 8; ++r) aheadWhite |= rank_bb(r);
    for (int r = rank_of(s) - 1; r >= 0; --r) aheadBlack |= rank_bb(r);
    PassedMask[WHITE][s] = files & aheadWhite;
    PassedMask[BLACK][s] = files & aheadBlack;
  }
}

int taper(Score s, int phase) { return (s.mg * phase + s.eg * (MaxPhase - phase)) / MaxPhase; }

struct Terms {
  Score material, pawns, mobility, kingSafety, pieces;
};

Bitboard pawn_attacks(const Position& pos, Color c) {
  Bitboard result = 0, pawns = pos.pieces(c, PAWN);
  while (pawns) result |= attacks::Pawn[c][pop_lsb(pawns)];
  return result;
}

void evaluate_side(const Position& pos, Color us, Terms& t) {
  const Color them = ~us;
  const Bitboard occ = pos.pieces();
  const Bitboard ourPawns = pos.pieces(us, PAWN), theirPawns = pos.pieces(them, PAWN);

  // Material e tabelas de casas.
  for (int pt = PAWN; pt <= KING; ++pt) {
    Bitboard b = pos.pieces(us, PieceType(pt));
    while (b) {
      const Square s = pop_lsb(b);
      const int idx = us == WHITE ? (s ^ 56) : s;
      t.material += PieceValue[pt];
      t.material += S(PstMg[pt][idx], PstEg[pt][idx]);
    }
  }

  // Estrutura de peões.
  for (int f = 0; f < 8; ++f) {
    const int n = popcount(ourPawns & file_bb(f));
    if (n > 1) t.pawns += Doubled * (n - 1);
  }
  Bitboard b = ourPawns;
  while (b) {
    const Square s = pop_lsb(b);
    if (!(ourPawns & AdjacentFiles[file_of(s)])) t.pawns += Isolated;
    if (!(theirPawns & PassedMask[us][s])) t.pawns += Passed[relative_rank(us, s)];
  }

  // Mobilidade e ataques à zona do rei inimigo. Casas atacadas por peões inimigos
  // não contam: uma peça que vai para lá é simplesmente capturada.
  const Bitboard area = ~pos.pieces(us) & ~pawn_attacks(pos, them);
  const Square theirKing = pos.king_square(them);
  const Bitboard kingZone = attacks::King[theirKing] | square_bb(theirKing);
  int attackUnits = 0, attackers = 0;
  for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
    Bitboard pieces = pos.pieces(us, PieceType(pt));
    while (pieces) {
      const Square s = pop_lsb(pieces);
      Bitboard att = 0;
      switch (pt) {
        case KNIGHT: att = attacks::Knight[s]; break;
        case BISHOP: att = attacks::bishop(s, occ); break;
        case ROOK: att = attacks::rook(s, occ); break;
        default: att = attacks::queen(s, occ); break;
      }
      t.mobility += MobilityWeight[pt] * (popcount(att & area) - MobilityBase[pt]);
      if (att & kingZone) {
        ++attackers;
        attackUnits += KingAttackWeight[pt] * popcount(att & kingZone);
      }
      if (pt == ROOK) {
        const Bitboard file = file_bb(file_of(s));
        if (!(ourPawns & file)) t.pieces += (theirPawns & file) ? RookSemiOpenFile : RookOpenFile;
      }
    }
  }
  // Pressão sobre o rei: cresce com o quadrado das unidades de ataque, só vale no
  // meio-jogo e só com pelo menos dois atacantes e a dama em jogo.
  if (attackers >= 2 && pos.pieces(us, QUEEN)) t.kingSafety += S(std::min(400, attackUnits * attackUnits / 4), 0);

  // Escudo de peões na frente do nosso rei (só no meio-jogo).
  const Square ourKing = pos.king_square(us);
  const int kingFile = file_of(ourKing), kingRank = relative_rank(us, ourKing);
  int shield = 0;
  for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
    Bitboard onFile = ourPawns & file_bb(f);
    int nearest = 8;
    while (onFile) {
      const int r = relative_rank(us, pop_lsb(onFile));
      if (r > kingRank) nearest = std::min(nearest, r - kingRank);
    }
    if (nearest == 1)
      shield += 12;
    else if (nearest == 2)
      shield += 6;
    else if (nearest == 8)
      shield -= (theirPawns & file_bb(f)) ? 15 : 25;
  }
  t.kingSafety += S(shield, 0);

  if (popcount(pos.pieces(us, BISHOP)) >= 2) t.pieces += BishopPair;
}

int game_phase(const Position& pos) {
  int phase = 0;
  for (int pt = KNIGHT; pt <= QUEEN; ++pt) phase += PhaseWeight[pt] * popcount(pos.pieces(PieceType(pt)));
  return std::min(phase, MaxPhase);
}

}  // namespace

int evaluate(const Position& pos, EvalTrace& trace) {
  static const bool masksReady = (init_masks(), true);
  (void)masksReady;

  trace = EvalTrace();
  if (pos.is_insufficient_material()) return 0;

  Terms side[2];
  evaluate_side(pos, WHITE, side[WHITE]);
  evaluate_side(pos, BLACK, side[BLACK]);
  const int phase = game_phase(pos);
  trace.phase = phase;

  Score total;
  for (Color c : {WHITE, BLACK}) {
    const Terms& t = side[c];
    trace.material[c] = taper(t.material, phase);
    trace.pawns[c] = taper(t.pawns, phase);
    trace.mobility[c] = taper(t.mobility, phase);
    trace.kingSafety[c] = taper(t.kingSafety, phase);
    trace.pieces[c] = taper(t.pieces, phase);
    const int sign = c == WHITE ? 1 : -1;
    for (const Score& s : {t.material, t.pawns, t.mobility, t.kingSafety, t.pieces}) total += s * sign;
  }
  trace.total = taper(total, phase);
  return (pos.side_to_move() == WHITE ? trace.total : -trace.total) + Tempo;
}

int evaluate(const Position& pos) {
  EvalTrace trace;
  return evaluate(pos, trace);
}

// Completa com espaços até a largura pedida, contando caracteres e não bytes
// (as letras acentuadas ocupam dois bytes em UTF-8).
static std::string pad(const std::string& text, int width) {
  int chars = 0;
  for (unsigned char ch : text)
    if ((ch & 0xC0) != 0x80) ++chars;
  return text + std::string(std::size_t(std::max(0, width - chars)), ' ');
}

std::string eval_report(const Position& pos) {
  EvalTrace t;
  const int score = evaluate(pos, t);
  char numbers[64];
  std::string out = pad("Termo (centipeões)", 22) + " Brancas   Pretas    Saldo\n";
  auto row = [&](const char* name, const int* v) {
    std::snprintf(numbers, sizeof numbers, "%8d %8d %+8d\n", v[WHITE], v[BLACK], v[WHITE] - v[BLACK]);
    out += pad(name, 22) + numbers;
  };
  row("Material e casas", t.material);
  row("Estrutura de peões", t.pawns);
  row("Mobilidade", t.mobility);
  row("Segurança do rei", t.kingSafety);
  row("Bispos e torres", t.pieces);
  out += "Fase do jogo: " + std::to_string(t.phase) + " de 24 (24 = abertura, 0 = só reis e peões)\n";
  std::snprintf(numbers, sizeof numbers, "%+d", t.total);
  out += std::string("Total para as brancas: ") + numbers;
  std::snprintf(numbers, sizeof numbers, "%+d", score);
  out += std::string("; para quem tem a vez (com o bônus de vez): ") + numbers + "\n";
  return out;
}

}  // namespace xadrez
