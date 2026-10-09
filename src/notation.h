// Notação: casas, lances UCI (e2e4) e notação algébrica (SAN) em inglês e em português.
#ifndef XADREZ_NOTATION_H
#define XADREZ_NOTATION_H

#include <string>
#include <vector>

#include "movegen.h"

namespace xadrez {

// Letras das peças. Em inglês: N (knight), B, R, Q, K. Em português: C (cavalo),
// B (bispo), T (torre), D (dama) e R (rei).
enum class Notation { English, Portuguese };
const char* piece_letters(Notation n);  // indexado por PieceType: "PNBRQK" ou "PCBTDR"

std::string square_name(Square s);
Square parse_square(const std::string& text);  // NO_SQUARE se for inválida

std::string move_to_uci(Move m);
// Procura o lance UCI entre os lances legais. Devolve MOVE_NONE se não achar.
Move parse_uci(const Position& pos, const std::string& text);
bool looks_like_uci(const std::string& text);

// SAN completo, com desambiguação mínima e sufixo + ou #. A posição volta igual.
std::string move_to_san(Position& pos, Move m, Notation n = Notation::English);
std::string line_to_san(const Position& pos, const std::vector<Move>& line, Notation n = Notation::English);

enum class SanStatus { Ok, Invalid, NotFound, Ambiguous };
// Leitura tolerante: aceita x opcional, = opcional na promoção, roque com O ou 0,
// sufixos +#!? e origem completa (Cg1f3). Promoção sem peça vira dama.
SanStatus parse_san(const Position& pos, const std::string& text, Notation n, Move& out);

// Diagrama do tabuleiro em texto, visto pelo lado indicado. Peças brancas em
// maiúsculas e pretas em minúsculas, ou símbolos Unicode.
std::string board_diagram(const Position& pos, Color perspective, Notation n, bool unicode);

}  // namespace xadrez

#endif
