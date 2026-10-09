#include "search.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "eval.h"

namespace xadrez {
namespace {

// Mate a N meios lances da raiz é guardado na tabela como "mate a N meios lances
// deste nó", para que o valor continue certo quando a posição aparecer em outra altura.
int score_to_tt(int s, int ply) {
  if (s >= VALUE_MATE_IN_MAX_PLY) return s + ply;
  if (s <= -VALUE_MATE_IN_MAX_PLY) return s - ply;
  return s;
}

int score_from_tt(int s, int ply) {
  if (s >= VALUE_MATE_IN_MAX_PLY) return s - ply;
  if (s <= -VALUE_MATE_IN_MAX_PLY) return s + ply;
  return s;
}

// Ordem dos lances: lance da tabela, capturas (MVV-LVA), promoção a dama, killers e
// por fim os lances quietos pelo histórico.
constexpr int TT_MOVE_SCORE = 30000000;
constexpr int CAPTURE_SCORE = 20000000;
constexpr int QUEEN_PROMO_SCORE = 19000000;
constexpr int KILLER1_SCORE = 18000000;
constexpr int KILLER2_SCORE = 17000000;
constexpr int HISTORY_MAX = 1000000;
constexpr long long MOVE_OVERHEAD_MS = 30;  // folga para a comunicação com a interface

// MVV-LVA: vítima mais valiosa primeiro; entre vítimas iguais, o atacante mais barato.
const int MvvLvaRank[6] = {1, 2, 3, 4, 5, 6};
const int QsearchValue[6] = {100, 320, 335, 480, 960, 0};

void pick_next(MoveList& list, int* scores, int start) {
  int best = start;
  for (int i = start + 1; i < list.size(); ++i)
    if (scores[i] > scores[best]) best = i;
  std::swap(list[start], list[best]);
  std::swap(scores[start], scores[best]);
}

}  // namespace

bool is_mate_score(int score) { return std::abs(score) >= VALUE_MATE_IN_MAX_PLY; }

int mate_in_moves(int score) { return score > 0 ? (VALUE_MATE - score + 1) / 2 : -(VALUE_MATE + score) / 2; }

Searcher::Searcher(TranspositionTable& tt) : tt_(tt) { clear_history(); }

void Searcher::request_stop() {
  {
    std::lock_guard<std::mutex> lock(waitMutex_);
    stop_.store(true);
  }
  waitCv_.notify_all();
}

void Searcher::reset_stop() { stop_.store(false); }

void Searcher::clear_history() {
  for (auto& k : killers_) k[0] = k[1] = MOVE_NONE;
  std::memset(history_, 0, sizeof history_);
}

long long Searcher::elapsed_ms() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count();
}

void Searcher::wait_for_stop() {
  std::unique_lock<std::mutex> lock(waitMutex_);
  waitCv_.wait(lock, [this] { return stop_.load(); });
}

// Gestão de tempo: um limite "suave" (não começa outra iteração depois dele) e um
// "rígido" (interrompe a busca no meio).
void Searcher::setup_time(const SearchLimits& limits, Color us) {
  softLimit_ = hardLimit_ = 0;
  timeManaged_ = false;
  if (limits.infinite) return;
  if (limits.movetime > 0) {
    hardLimit_ = std::max(1LL, limits.movetime - std::min(MOVE_OVERHEAD_MS, limits.movetime / 10));
    softLimit_ = hardLimit_;
    return;
  }
  if (limits.time[us] >= 0) {
    timeManaged_ = true;
    const long long remaining = std::max(1LL, limits.time[us] - MOVE_OVERHEAD_MS);
    const long long movesLeft = limits.movestogo > 0 ? std::min(limits.movestogo, 40) : 30;
    long long soft = remaining / movesLeft + std::max(0LL, limits.inc[us]) * 3 / 4;
    soft = std::min(soft, remaining * 8 / 10);
    const long long hard = std::min(soft * 4, remaining * 9 / 10);
    softLimit_ = std::max(1LL, soft);
    hardLimit_ = std::max(softLimit_, hard);
  }
}

bool Searcher::should_stop() {
  if (stopped_) return true;
  if (rootDepth_ <= 1) return false;  // a primeira iteração sempre termina
  if (stop_.load(std::memory_order_relaxed) || (limits_.nodes && nodes_ >= limits_.nodes) ||
      (hardLimit_ > 0 && (nodes_ & 1023) == 0 && elapsed_ms() >= hardLimit_))
    stopped_ = true;
  return stopped_;
}

SearchResult Searcher::search(const Position& root, const SearchLimits& limits, const InfoCallback& onInfo) {
  start_ = std::chrono::steady_clock::now();
  limits_ = limits;
  nodes_ = 0;
  stopped_ = false;
  rootDepth_ = 0;
  setup_time(limits, root.side_to_move());
  tt_.new_search();
  for (auto& k : killers_) k[0] = k[1] = MOVE_NONE;
  for (auto& byColor : history_)
    for (auto& row : byColor)
      for (int& h : row) h /= 8;

  Position pos = root;
  SearchResult result;
  MoveList rootMoves;
  generate_legal(pos, rootMoves);
  if (rootMoves.size() == 0) {
    result.score = pos.in_check() ? -VALUE_MATE : 0;
    if (onInfo) {
      SearchInfo info;
      info.score = result.score;
      onInfo(info);
    }
    if (limits.infinite) wait_for_stop();
    return result;
  }
  result.bestMove = rootMoves[0];

  const int maxDepth = limits.depth > 0 ? std::min(limits.depth, MAX_PLY - 1) : MAX_PLY - 1;
  int previous = 0;
  for (int depth = 1; depth <= maxDepth; ++depth) {
    rootDepth_ = depth;
    seldepth_ = 0;
    int score = 0;
    if (depth >= 5 && !is_mate_score(previous)) {
      // Janela de aspiração: começa estreita em volta do valor anterior e alarga
      // só se o resultado cair fora dela.
      int delta = 30;
      int alpha = std::max(previous - delta, -VALUE_INFINITE);
      int beta = std::min(previous + delta, VALUE_INFINITE);
      while (true) {
        score = negamax(pos, depth, alpha, beta, 0, false);
        if (stopped_) break;
        if (score <= alpha)
          alpha = std::max(score - delta, -VALUE_INFINITE);
        else if (score >= beta)
          beta = std::min(score + delta, VALUE_INFINITE);
        else
          break;
        delta *= 2;
      }
    } else {
      score = negamax(pos, depth, -VALUE_INFINITE, VALUE_INFINITE, 0, false);
    }
    if (stopped_) break;  // iteração incompleta: vale o resultado da anterior

    previous = score;
    result.depth = depth;
    result.score = score;
    result.pv.assign(pv_[0], pv_[0] + pvLength_[0]);
    if (!result.pv.empty()) result.bestMove = result.pv[0];
    result.ponderMove = result.pv.size() > 1 ? result.pv[1] : MOVE_NONE;
    result.nodes = nodes_;
    result.timeMs = elapsed_ms();
    if (onInfo) {
      SearchInfo info;
      info.depth = depth;
      info.seldepth = seldepth_;
      info.score = score;
      info.nodes = nodes_;
      info.timeMs = result.timeMs;
      info.hashfull = tt_.hashfull();
      info.pv = result.pv;
      onInfo(info);
    }

    if (stop_.load() || (limits.nodes && nodes_ >= limits.nodes)) break;
    if (timeManaged_) {
      if (rootMoves.size() == 1) break;                      // lance único: não gasta o relógio
      if (result.timeMs >= softLimit_ * 6 / 10) break;       // a próxima iteração não caberia
      if (is_mate_score(score) && depth >= 2 * std::abs(mate_in_moves(score)) + 2) break;
    }
  }
  result.nodes = nodes_;
  result.timeMs = elapsed_ms();
  // No modo infinite o protocolo exige esperar o "stop" antes de responder.
  if (limits.infinite) wait_for_stop();
  return result;
}

int Searcher::negamax(Position& pos, int depth, int alpha, int beta, int ply, bool allowNull) {
  pvLength_[ply] = ply;
  const bool rootNode = ply == 0;
  const bool pvNode = beta - alpha > 1;

  if (!rootNode) {
    if (should_stop()) return 0;
    if (pos.is_repetition() || pos.is_insufficient_material()) return 0;
    // Poda pela distância do mate: daqui não sai mate mais rápido do que um já garantido.
    alpha = std::max(alpha, -VALUE_MATE + ply);
    beta = std::min(beta, VALUE_MATE - ply - 1);
    if (alpha >= beta) return alpha;
  }

  const bool inCheck = pos.in_check();
  if (ply >= MAX_PLY - 1) return inCheck ? 0 : evaluate(pos);
  if (inCheck) ++depth;  // extensão de xeque: nunca pare a análise com o rei em xeque
  if (depth <= 0) return quiesce(pos, alpha, beta, ply);

  ++nodes_;
  seldepth_ = std::max(seldepth_, ply);

  TTEntry entry;
  Move ttMove = MOVE_NONE;
  if (tt_.probe(pos.key(), entry)) {
    ttMove = entry.move;
    if (!pvNode && entry.depth >= depth) {
      const int s = score_from_tt(entry.score, ply);
      if (entry.bound == BOUND_EXACT || (entry.bound == BOUND_LOWER && s >= beta) ||
          (entry.bound == BOUND_UPPER && s <= alpha))
        return s;
    }
  }

  // Lance nulo: se mesmo passando a vez o adversário não consegue ficar abaixo de beta
  // numa busca rasa, a posição é boa demais e o ramo pode ser cortado.
  if (!pvNode && !inCheck && allowNull && depth >= 3 && std::abs(beta) < VALUE_MATE_IN_MAX_PLY &&
      pos.has_non_pawn_material(pos.side_to_move()) && evaluate(pos) >= beta) {
    const int reduction = 3 + depth / 6;
    pos.make_null_move();
    const int score = -negamax(pos, depth - 1 - reduction, -beta, -beta + 1, ply + 1, false);
    pos.unmake_null_move();
    if (stopped_) return 0;
    if (score >= beta) return score >= VALUE_MATE_IN_MAX_PLY ? beta : score;
  }

  MoveList& moves = lists_[ply];
  generate_legal(pos, moves);
  if (moves.size() == 0) return inCheck ? -VALUE_MATE + ply : 0;  // mate ou afogamento
  if (!rootNode && pos.halfmove_clock() >= 100) return 0;        // 50 lances (sem mate)

  int* scores = moveScores_[ply];
  score_moves(pos, moves, scores, ttMove, ply);

  const Color us = pos.side_to_move();
  const int originalAlpha = alpha;
  int bestScore = -VALUE_INFINITE;
  Move bestMove = MOVE_NONE;
  for (int i = 0; i < moves.size(); ++i) {
    pick_next(moves, scores, i);
    const Move m = moves[i];
    const bool quiet = !is_capture(m) && !is_promotion(m);
    pos.make_move(m);
    const bool givesCheck = pos.in_check();
    int score;
    if (i == 0) {
      score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, true);
    } else {
      // PVS: os outros lances são testados com janela nula, só para provar que não
      // superam alfa. Lances quietos tardios ainda ganham uma redução (LMR).
      int reduction = 0;
      if (depth >= 3 && i >= 3 && quiet && !inCheck && !givesCheck && m != killers_[ply][0] &&
          m != killers_[ply][1])
        reduction = (i >= 8 && depth >= 6) ? 2 : 1;
      score = -negamax(pos, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);
      if (score > alpha && reduction > 0) score = -negamax(pos, depth - 1, -alpha - 1, -alpha, ply + 1, true);
      if (score > alpha && score < beta) score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, true);
    }
    pos.unmake_move(m);
    if (stopped_) return 0;

    if (score > bestScore) {
      bestScore = score;
      bestMove = m;
      if (score > alpha) {
        alpha = score;
        pv_[ply][ply] = m;
        for (int j = ply + 1; j < pvLength_[ply + 1]; ++j) pv_[ply][j] = pv_[ply + 1][j];
        pvLength_[ply] = pvLength_[ply + 1];
        if (score >= beta) {
          if (quiet) {
            if (killers_[ply][0] != m) {
              killers_[ply][1] = killers_[ply][0];
              killers_[ply][0] = m;
            }
            int& h = history_[us][move_from(m)][move_to(m)];
            h += depth * depth;
            if (h > HISTORY_MAX)
              for (auto& byColor : history_)
                for (auto& row : byColor)
                  for (int& v : row) v /= 2;
          }
          break;
        }
      }
    }
  }

  const Bound bound = bestScore >= beta ? BOUND_LOWER : bestScore > originalAlpha ? BOUND_EXACT : BOUND_UPPER;
  tt_.store(pos.key(), bestMove, score_to_tt(bestScore, ply), depth, bound);
  return bestScore;
}

// Quiescência: no fim da busca principal só capturas e promoções continuam, até a
// posição ficar "quieta". Evita o efeito horizonte de avaliar no meio de uma troca.
int Searcher::quiesce(Position& pos, int alpha, int beta, int ply) {
  pvLength_[ply] = ply;
  if (should_stop()) return 0;
  ++nodes_;
  seldepth_ = std::max(seldepth_, ply);
  if (pos.is_insufficient_material()) return 0;

  const bool inCheck = pos.in_check();
  if (ply >= MAX_PLY - 1) return inCheck ? 0 : evaluate(pos);

  MoveList& moves = lists_[ply];
  int bestScore, standPat = 0;
  if (inCheck) {
    // Em xeque não existe "ficar parado": todas as respostas legais são analisadas.
    generate_legal(pos, moves);
    if (moves.size() == 0) return -VALUE_MATE + ply;
    bestScore = -VALUE_INFINITE;
  } else {
    standPat = evaluate(pos);
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;
    bestScore = standPat;
    generate_noisy(pos, moves);
  }

  int* scores = moveScores_[ply];
  score_moves(pos, moves, scores, MOVE_NONE, ply);
  for (int i = 0; i < moves.size(); ++i) {
    pick_next(moves, scores, i);
    const Move m = moves[i];
    // Poda delta: nem ganhando a peça capturada (com folga) daria para chegar em alfa.
    if (!inCheck && !is_promotion(m)) {
      const PieceType victim = move_flag(m) == FLAG_EP_CAPTURE ? PAWN : type_of(pos.piece_on(move_to(m)));
      if (standPat + QsearchValue[victim] + 200 < alpha) continue;
    }
    pos.make_move(m);
    const int score = -quiesce(pos, -beta, -alpha, ply + 1);
    pos.unmake_move(m);
    if (stopped_) return 0;
    if (score > bestScore) {
      bestScore = score;
      if (score > alpha) {
        alpha = score;
        if (score >= beta) break;
      }
    }
  }
  return bestScore;
}

void Searcher::score_moves(const Position& pos, const MoveList& list, int* scores, Move ttMove, int ply) const {
  const Color us = pos.side_to_move();
  for (int i = 0; i < list.size(); ++i) {
    const Move m = list[i];
    int s;
    if (m == ttMove) {
      s = TT_MOVE_SCORE;
    } else if (is_capture(m)) {
      const PieceType victim = move_flag(m) == FLAG_EP_CAPTURE ? PAWN : type_of(pos.piece_on(move_to(m)));
      const PieceType attacker = type_of(pos.piece_on(move_from(m)));
      s = CAPTURE_SCORE + 8 * MvvLvaRank[victim] - MvvLvaRank[attacker];
      if (is_promotion(m) && promotion_type(m) == QUEEN) s += 64;
    } else if (is_promotion(m)) {
      s = promotion_type(m) == QUEEN ? QUEEN_PROMO_SCORE : -1000 + int(promotion_type(m));
    } else if (m == killers_[ply][0]) {
      s = KILLER1_SCORE;
    } else if (m == killers_[ply][1]) {
      s = KILLER2_SCORE;
    } else {
      s = history_[us][move_from(m)][move_to(m)];
    }
    scores[i] = s;
  }
}

}  // namespace xadrez
