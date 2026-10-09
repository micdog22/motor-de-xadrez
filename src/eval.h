// Avaliação estática: quanto a posição vale, em centipeões, para quem tem a vez.
#ifndef XADREZ_EVAL_H
#define XADREZ_EVAL_H

#include <string>

#include "position.h"

namespace xadrez {

// Valor de cada termo para as brancas e para as pretas, já interpolado pela fase.
struct EvalTrace {
  int material[2] = {0, 0};  // material e tabelas de casas
  int pawns[2] = {0, 0};     // peões dobrados, isolados e passados
  int mobility[2] = {0, 0};
  int kingSafety[2] = {0, 0};
  int pieces[2] = {0, 0};  // par de bispos e torres em colunas abertas
  int phase = 0;           // 24 no começo da partida, 0 só com reis e peões
  int total = 0;           // do ponto de vista das brancas
};

int evaluate(const Position& pos);  // ponto de vista de quem tem a vez
int evaluate(const Position& pos, EvalTrace& trace);
std::string eval_report(const Position& pos);

}  // namespace xadrez

#endif
