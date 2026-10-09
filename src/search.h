// Busca: aprofundamento iterativo, PVS alfa-beta, quiescência e gestão de tempo.
#ifndef XADREZ_SEARCH_H
#define XADREZ_SEARCH_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "movegen.h"
#include "tt.h"

namespace xadrez {

constexpr int VALUE_MATE = 32000;
constexpr int VALUE_INFINITE = 32001;
// Pontuações acima disso significam "mate em até MAX_PLY meios lances".
constexpr int VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY;

struct SearchLimits {
  int depth = 0;                 // 0: sem limite de profundidade
  std::uint64_t nodes = 0;       // 0: sem limite de nós
  long long movetime = 0;        // tempo fixo por lance, em ms (0: não usado)
  long long time[2] = {-1, -1};  // relógio das brancas e das pretas (-1: não informado)
  long long inc[2] = {0, 0};     // incremento por lance
  int movestogo = 0;             // lances até o próximo controle (0: partida inteira)
  bool infinite = false;         // só para com "stop"
};

// Resumo de uma iteração completa, usado nas linhas "info" do UCI.
struct SearchInfo {
  int depth = 0;
  int seldepth = 0;
  int score = 0;
  std::uint64_t nodes = 0;
  long long timeMs = 0;
  int hashfull = 0;
  std::vector<Move> pv;
};

struct SearchResult {
  Move bestMove = MOVE_NONE;
  Move ponderMove = MOVE_NONE;
  int score = 0;
  int depth = 0;
  std::uint64_t nodes = 0;
  long long timeMs = 0;
  std::vector<Move> pv;
};

bool is_mate_score(int score);
// Lances (não meios lances) até o mate: positivo se quem joga dá o mate.
int mate_in_moves(int score);

class Searcher {
 public:
  using InfoCallback = std::function<void(const SearchInfo&)>;

  explicit Searcher(TranspositionTable& tt);

  // Analisa a posição e devolve o melhor lance da última iteração completa.
  // Pode rodar numa thread separada; request_stop() pode vir de qualquer thread.
  SearchResult search(const Position& root, const SearchLimits& limits, const InfoCallback& onInfo = InfoCallback());

  void request_stop();
  void reset_stop();
  void clear_history();

 private:
  int negamax(Position& pos, int depth, int alpha, int beta, int ply, bool allowNull);
  int quiesce(Position& pos, int alpha, int beta, int ply);
  void score_moves(const Position& pos, const MoveList& list, int* scores, Move ttMove, int ply) const;
  bool should_stop();
  long long elapsed_ms() const;
  void setup_time(const SearchLimits& limits, Color us);
  void wait_for_stop();

  TranspositionTable& tt_;
  SearchLimits limits_;
  std::chrono::steady_clock::time_point start_;
  long long softLimit_ = 0;  // depois disso não começa outra iteração
  long long hardLimit_ = 0;  // depois disso interrompe a busca (0: sem limite)
  bool timeManaged_ = false;
  bool stopped_ = false;
  int rootDepth_ = 0;
  int seldepth_ = 0;
  std::uint64_t nodes_ = 0;

  std::atomic<bool> stop_{false};
  std::mutex waitMutex_;
  std::condition_variable waitCv_;

  // Uma lista de lances por camada, fora da pilha: a recursão fica leve mesmo em
  // threads com pilha pequena.
  MoveList lists_[MAX_PLY + 1];
  int moveScores_[MAX_PLY + 1][MAX_MOVES];
  Move pv_[MAX_PLY + 1][MAX_PLY + 1];
  int pvLength_[MAX_PLY + 1];
  Move killers_[MAX_PLY + 1][2];
  int history_[2][64][64];
};

}  // namespace xadrez

#endif
