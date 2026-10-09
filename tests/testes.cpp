// Testes de unidade do motor: bitboards, FEN, Zobrist, notação, regras de empate,
// avaliação, busca (mates em 1 e em 2), protocolo UCI e modo de jogo no terminal.
#include <cctype>
#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "eval.h"
#include "movegen.h"
#include "notation.h"
#include "play.h"
#include "search.h"
#include "teste.h"
#include "uci.h"

using namespace xadrez;

namespace {

const char* const KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

Position from_fen(const std::string& fen) {
  Position p;
  std::string error;
  if (!p.set_fen(fen, &error)) std::cerr << "FEN inválida no teste (" << error << "): " << fen << "\n";
  return p;
}

// Aplica uma lista de lances em SAN (inglês). Devolve false se algum não existir.
bool play_line(Position& pos, const std::vector<std::string>& sans) {
  for (const std::string& san : sans) {
    Move m = MOVE_NONE;
    if (parse_san(pos, san, Notation::English, m) != SanStatus::Ok) {
      std::cerr << "lance não encontrado no teste: " << san << "\n";
      return false;
    }
    pos.make_move(m);
  }
  return true;
}

// Espelha a posição: vira o tabuleiro de cabeça para baixo e troca as cores.
std::string mirror_fen(const std::string& fen) {
  std::istringstream is(fen);
  std::string board, side, castling, ep, half = "0", full = "1";
  is >> board >> side >> castling >> ep >> half >> full;
  std::vector<std::string> ranks;
  std::stringstream bs(board);
  for (std::string r; std::getline(bs, r, '/');) ranks.push_back(r);
  std::string mirrored;
  for (auto it = ranks.rbegin(); it != ranks.rend(); ++it) {
    if (!mirrored.empty()) mirrored += '/';
    for (char ch : *it) mirrored += char(std::isupper(static_cast<unsigned char>(ch)) ? std::tolower(ch) : std::toupper(ch));
  }
  std::string rights;
  for (char wanted : std::string("KQkq")) {
    const char source = char(std::isupper(static_cast<unsigned char>(wanted)) ? std::tolower(wanted) : std::toupper(wanted));
    if (castling.find(source) != std::string::npos) rights += wanted;
  }
  if (rights.empty()) rights = "-";
  if (ep != "-") ep[1] = ep[1] == '3' ? '6' : '3';
  return mirrored + (side == "w" ? " b " : " w ") + rights + " " + ep + " " + half + " " + full;
}

// Joga lances aleatórios (com semente fixa) e devolve as posições visitadas.
std::vector<std::string> random_positions(unsigned seed, int games, int maxPlies) {
  std::mt19937_64 rng(seed);
  std::vector<std::string> out;
  for (int g = 0; g < games; ++g) {
    Position pos;
    for (int ply = 0; ply < maxPlies; ++ply) {
      MoveList list;
      generate_legal(pos, list);
      if (list.size() == 0) break;
      pos.make_move(list[int(rng() % std::uint64_t(list.size()))]);
      out.push_back(pos.fen());
    }
  }
  return out;
}

// Força bruta independente da busca: existe mate em até n lances para quem tem a vez?
bool forced_mate(Position& pos, int n);

bool every_reply_loses(Position& pos, int n) {
  MoveList replies;
  generate_legal(pos, replies);
  if (replies.size() == 0) return pos.in_check();
  if (n <= 1) return false;
  for (Move r : replies) {
    pos.make_move(r);
    const bool ok = forced_mate(pos, n - 1);
    pos.unmake_move(r);
    if (!ok) return false;
  }
  return true;
}

bool forced_mate(Position& pos, int n) {
  MoveList list;
  generate_legal(pos, list);
  for (Move m : list) {
    pos.make_move(m);
    const bool ok = every_reply_loses(pos, n);
    pos.unmake_move(m);
    if (ok) return true;
  }
  return false;
}

SearchResult search_depth(const Position& pos, int depth) {
  TranspositionTable tt(4);
  Searcher searcher(tt);
  SearchLimits limits;
  limits.depth = depth;
  return searcher.search(pos, limits);
}

bool contains(const std::string& text, const std::string& piece) { return text.find(piece) != std::string::npos; }

}  // namespace

TEST(popcount_and_lsb, "contagem de bits e bit menos significativo") {
  CHECK_EQ(popcount(0), 0);
  CHECK_EQ(popcount(~Bitboard(0)), 64);
  CHECK_EQ(popcount(0x8000000000000001ULL), 2);
  for (int i = 0; i < 64; ++i) CHECK_EQ(lsb(Bitboard(1) << i), i);
  Bitboard b = square_bb(3) | square_bb(17) | square_bb(63);
  CHECK_EQ(pop_lsb(b), 3);
  CHECK_EQ(pop_lsb(b), 17);
  CHECK_EQ(pop_lsb(b), 63);
  CHECK_EQ(b, Bitboard(0));
}

TEST(leaper_attacks, "ataques de cavalo, rei e peão") {
  init();
  CHECK_EQ(attacks::Knight[A1], square_bb(make_square(1, 2)) | square_bb(make_square(2, 1)));
  CHECK_EQ(popcount(attacks::Knight[make_square(3, 3)]), 8);
  CHECK_EQ(popcount(attacks::Knight[H8]), 2);
  CHECK_EQ(popcount(attacks::King[A1]), 3);
  CHECK_EQ(popcount(attacks::King[make_square(4, 3)]), 8);
  CHECK_EQ(attacks::Pawn[WHITE][make_square(4, 3)], square_bb(make_square(3, 4)) | square_bb(make_square(5, 4)));
  CHECK_EQ(attacks::Pawn[WHITE][make_square(0, 1)], square_bb(make_square(1, 2)));
  CHECK_EQ(attacks::Pawn[BLACK][make_square(7, 6)], square_bb(make_square(6, 5)));
  CHECK_EQ(popcount(attacks::Between[A1][H8]), 6);
  CHECK_EQ(attacks::Between[A1][make_square(1, 2)], Bitboard(0));
  CHECK(attacks::Line[A1][make_square(2, 2)] & square_bb(H8));
}

TEST(magic_matches_ray_loops, "magic bitboards iguais ao laço por raios") {
  init();
  std::mt19937_64 rng(2026);
  int mismatches = 0;
  for (Square s = 0; s < 64; ++s) {
    for (int i = 0; i < 300; ++i) {
      const Bitboard occ = rng() & rng();
      if (attacks::rook(s, occ) != attacks::sliding_slow(ROOK, s, occ)) ++mismatches;
      if (attacks::bishop(s, occ) != attacks::sliding_slow(BISHOP, s, occ)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

TEST(fen_round_trip, "FEN de ida e volta") {
  const std::vector<std::string> fens = {
      START_FEN,
      KIWIPETE,
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
      "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",
      "4k3/8/8/8/8/8/8/4K3 w - - 75 120",
  };
  for (const std::string& fen : fens) CHECK_EQ(from_fen(fen).fen(), fen);
  // Campos de relógio são opcionais na entrada.
  Position p;
  CHECK(p.set_fen("4k3/8/8/8/8/8/8/4K3 b - -"));
  CHECK_EQ(p.fen(), std::string("4k3/8/8/8/8/8/8/4K3 b - - 0 1"));
}

TEST(invalid_fen_rejected, "FEN inválida é recusada sem estragar a posição") {
  const std::vector<std::string> bad = {
      "",
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",          // 7 fileiras
      "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  // dígito 9
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNX w KQkq - 0 1",  // peça desconhecida
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",  // vez inválida
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkk - 0 1",  // roque repetido
      "4k3/8/8/8/8/8/8/8 w - - 0 1",                                // sem rei branco
      "4k3/8/8/8/8/8/8/3KK3 w - - 0 1",                             // dois reis
      "P3k3/8/8/8/8/8/8/4K3 w - - 0 1",                             // peão na oitava
      "4k3/8/8/8/8/8/4q3/4K3 b - - 0 1",                            // quem não joga está em xeque
      "4k3/8/8/8/8/8/8/4K3 w K - 0 1",                              // roque sem torre
      "4k3/8/8/8/8/8/8/4K3 w - e3 0 1",                             // en passant sem peão
      "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e3 0 2",  // fileira errada
      "4k3/8/8/8/8/8/8/4K3 w - - -1 1",                             // relógio negativo
      "4k3/8/8/8/8/8/8/4K3 w - - 0 0",                              // lance zero
  };
  for (const std::string& fen : bad) {
    Position p = from_fen(KIWIPETE);
    std::string error;
    CHECK(!p.set_fen(fen, &error));
    CHECK(!error.empty());
    CHECK_EQ(p.fen(), std::string(KIWIPETE));
  }
}

TEST(zobrist_incremental_and_unmake, "Zobrist incremental e desfazer lances") {
  std::mt19937_64 rng(42);
  int keyErrors = 0, restoreErrors = 0;
  for (int game = 0; game < 300; ++game) {
    Position pos = from_fen(game % 2 ? START_FEN : KIWIPETE);
    const std::string startFen = pos.fen();
    const Key startKey = pos.key();
    std::vector<Move> line;
    for (int ply = 0; ply < 120; ++ply) {
      MoveList list;
      generate_legal(pos, list);
      if (list.size() == 0) break;
      const Move m = list[int(rng() % std::uint64_t(list.size()))];
      pos.make_move(m);
      line.push_back(m);
      if (pos.key() != pos.compute_key()) ++keyErrors;
    }
    for (auto it = line.rbegin(); it != line.rend(); ++it) pos.unmake_move(*it);
    if (pos.fen() != startFen || pos.key() != startKey) ++restoreErrors;
  }
  CHECK_EQ(keyErrors, 0);
  CHECK_EQ(restoreErrors, 0);
}

TEST(zobrist_transposition, "transposições têm a mesma chave Zobrist") {
  Position a, b;
  CHECK(play_line(a, {"Nf3", "Nf6", "Nc3", "Nc6"}));
  CHECK(play_line(b, {"Nc3", "Nc6", "Nf3", "Nf6"}));
  CHECK_EQ(a.key(), b.key());
  CHECK_EQ(a.key(), from_fen("r1bqkb1r/pppppppp/2n2n2/8/8/2N2N2/PPPPPPPP/R1BQKB1R w KQkq - 4 3").key());
  Position c;
  CHECK(play_line(c, {"Nf3", "Nf6", "Nc3", "Nc6", "Rb1"}));
  CHECK(c.key() != a.key());
}

TEST(ep_key_only_when_legal, "en passant só entra na chave quando é legal") {
  // Depois de 1.e4 a FEN registra e3, mas nenhum peão preto pode capturar.
  Position p;
  CHECK(play_line(p, {"e4"}));
  CHECK_EQ(p.fen(), std::string("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
  CHECK_EQ(p.key(), from_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1").key());
  // Captura possível: a chave muda.
  const Position withEp = from_fen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
  const Position without = from_fen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3");
  CHECK(withEp.key() != without.key());
  // Captura ilegal por xeque descoberto na fileira (rei a5, torre h5): não muda a chave
  // e o lance b5xc6 não aparece entre os legais.
  const Position pinned = from_fen("8/8/8/KPp4r/8/8/8/7k w - c6 0 2");
  CHECK_EQ(pinned.key(), from_fen("8/8/8/KPp4r/8/8/8/7k w - - 0 2").key());
  MoveList list;
  generate_legal(pinned, list);
  bool hasEp = false;
  for (Move m : list) hasEp = hasEp || move_flag(m) == FLAG_EP_CAPTURE;
  CHECK(!hasEp);
}

TEST(quick_perft, "perft rápido, inclusive em posições espelhadas") {
  Position p;
  CHECK_EQ(perft(p, 3), std::uint64_t(8902));
  Position k = from_fen(KIWIPETE);
  CHECK_EQ(perft(k, 2), std::uint64_t(2039));
  // A versão espelhada de cada posição precisa dar exatamente o mesmo número.
  Position m = from_fen(mirror_fen(KIWIPETE));
  CHECK_EQ(perft(m, 3), std::uint64_t(97862));
  Position e = from_fen(mirror_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"));
  CHECK_EQ(perft(e, 4), std::uint64_t(43238));
}

TEST(san_basic, "SAN básico em inglês e em português") {
  Position p;
  Move m = parse_uci(p, "g1f3");
  CHECK_EQ(move_to_san(p, m, Notation::English), std::string("Nf3"));
  CHECK_EQ(move_to_san(p, m, Notation::Portuguese), std::string("Cf3"));
  CHECK_EQ(move_to_san(p, parse_uci(p, "e2e4")), std::string("e4"));
  CHECK_EQ(p.fen(), std::string(START_FEN));  // move_to_san não altera a posição
  Position k = from_fen(KIWIPETE);
  CHECK_EQ(move_to_san(k, parse_uci(k, "e1g1")), std::string("O-O"));
  CHECK_EQ(move_to_san(k, parse_uci(k, "e1c1")), std::string("O-O-O"));
  CHECK_EQ(move_to_san(k, parse_uci(k, "e2a6")), std::string("Bxa6"));
  CHECK_EQ(move_to_san(k, parse_uci(k, "e2a6"), Notation::Portuguese), std::string("Bxa6"));
  CHECK_EQ(move_to_san(k, parse_uci(k, "f3f6"), Notation::Portuguese), std::string("Dxf6"));
  CHECK_EQ(move_to_san(k, parse_uci(k, "a1b1"), Notation::Portuguese), std::string("Tb1"));
}

TEST(san_disambiguation, "SAN: desambiguação") {
  Position knights = from_fen("4k3/8/8/8/8/5N2/8/1N2K3 w - - 0 1");
  CHECK_EQ(move_to_san(knights, parse_uci(knights, "b1d2")), std::string("Nbd2"));
  CHECK_EQ(move_to_san(knights, parse_uci(knights, "f3d2")), std::string("Nfd2"));
  CHECK_EQ(move_to_san(knights, parse_uci(knights, "f3d4")), std::string("Nd4"));
  Position rooks = from_fen("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1");
  CHECK_EQ(move_to_san(rooks, parse_uci(rooks, "a1a3")), std::string("R1a3"));
  CHECK_EQ(move_to_san(rooks, parse_uci(rooks, "a5a3")), std::string("R5a3"));
  Position queens = from_fen("4k3/8/8/8/8/Q7/8/Q1Q1K3 w - - 0 1");
  CHECK_EQ(move_to_san(queens, parse_uci(queens, "a1b2")), std::string("Qa1b2"));
  CHECK_EQ(move_to_san(queens, parse_uci(queens, "c1b2")), std::string("Qcb2"));
  CHECK_EQ(move_to_san(queens, parse_uci(queens, "a3b2")), std::string("Q3b2"));
  // Uma peça cravada não conta para a desambiguação: o cavalo de e2 não pode ir a d4.
  Position pinned = from_fen("4k3/4r3/8/8/8/5N2/4N3/4K3 w - - 0 1");
  CHECK_EQ(move_to_san(pinned, parse_uci(pinned, "f3d4")), std::string("Nd4"));
}

TEST(san_special_moves, "SAN: promoção, en passant e mate") {
  Position promo = from_fen("7k/4P3/8/8/8/8/8/4K3 w - - 0 1");
  CHECK_EQ(move_to_san(promo, parse_uci(promo, "e7e8q")), std::string("e8=Q+"));
  CHECK_EQ(move_to_san(promo, parse_uci(promo, "e7e8r")), std::string("e8=R+"));
  CHECK_EQ(move_to_san(promo, parse_uci(promo, "e7e8b")), std::string("e8=B"));
  CHECK_EQ(move_to_san(promo, parse_uci(promo, "e7e8n")), std::string("e8=N"));
  CHECK_EQ(move_to_san(promo, parse_uci(promo, "e7e8q"), Notation::Portuguese), std::string("e8=D+"));
  CHECK_EQ(move_to_san(promo, parse_uci(promo, "e7e8n"), Notation::Portuguese), std::string("e8=C"));
  Position ep = from_fen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
  const Move epMove = parse_uci(ep, "e5f6");
  CHECK_EQ(move_flag(epMove), int(FLAG_EP_CAPTURE));
  CHECK_EQ(move_to_san(ep, epMove), std::string("exf6"));
  Position scholar = from_fen("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4");
  CHECK_EQ(move_to_san(scholar, parse_uci(scholar, "h5f7")), std::string("Qxf7#"));
  CHECK_EQ(move_to_san(scholar, parse_uci(scholar, "h5f7"), Notation::Portuguese), std::string("Dxf7#"));
}

TEST(san_uci_round_trip, "SAN e UCI de ida e volta em todos os lances") {
  std::vector<std::string> fens = random_positions(7, 40, 80);
  for (const char* f : {START_FEN, KIWIPETE, "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1"})
    fens.push_back(f);
  int errors = 0, total = 0;
  for (const std::string& fen : fens) {
    Position pos = from_fen(fen);
    MoveList list;
    generate_legal(pos, list);
    for (Move m : list) {
      ++total;
      if (parse_uci(pos, move_to_uci(m)) != m) ++errors;
      for (Notation n : {Notation::English, Notation::Portuguese}) {
        Move back = MOVE_NONE;
        if (parse_san(pos, move_to_san(pos, m, n), n, back) != SanStatus::Ok || back != m) ++errors;
      }
    }
  }
  CHECK(total > 5000);
  CHECK_EQ(errors, 0);
}

TEST(san_lenient_input, "SAN: entrada tolerante") {
  Position p;
  Move m = MOVE_NONE;
  CHECK(parse_san(p, "Ng1f3", Notation::English, m) == SanStatus::Ok && m == parse_uci(p, "g1f3"));
  CHECK(parse_san(p, "Cf3", Notation::Portuguese, m) == SanStatus::Ok && m == parse_uci(p, "g1f3"));
  CHECK(parse_san(p, "e4!?", Notation::English, m) == SanStatus::Ok && m == parse_uci(p, "e2e4"));
  CHECK(parse_san(p, "e5", Notation::English, m) == SanStatus::NotFound);
  CHECK(parse_san(p, "Cf3", Notation::English, m) == SanStatus::Invalid);
  CHECK(parse_san(p, "xyz", Notation::English, m) == SanStatus::Invalid);
  Position k = from_fen(KIWIPETE);
  CHECK(parse_san(k, "0-0", Notation::English, m) == SanStatus::Ok && move_flag(m) == FLAG_KING_CASTLE);
  CHECK(parse_san(k, "O-O-O+", Notation::English, m) == SanStatus::Ok && move_flag(m) == FLAG_QUEEN_CASTLE);
  CHECK(parse_san(k, "Nd3", Notation::English, m) == SanStatus::Ok);
  Position promo = from_fen("7k/4P3/8/8/8/8/8/4K3 w - - 0 1");
  CHECK(parse_san(promo, "e8Q", Notation::English, m) == SanStatus::Ok && m == parse_uci(promo, "e7e8q"));
  CHECK(parse_san(promo, "e8", Notation::English, m) == SanStatus::Ok && m == parse_uci(promo, "e7e8q"));
  CHECK(parse_san(promo, "e8=T", Notation::Portuguese, m) == SanStatus::Ok && m == parse_uci(promo, "e7e8r"));
  Position knights = from_fen("4k3/8/8/8/8/5N2/8/1N2K3 w - - 0 1");
  CHECK(parse_san(knights, "Nd2", Notation::English, m) == SanStatus::Ambiguous);
  CHECK(parse_san(knights, "Nbd2", Notation::English, m) == SanStatus::Ok && m == parse_uci(knights, "b1d2"));
  CHECK_EQ(parse_uci(p, "e2e5"), MOVE_NONE);
  CHECK_EQ(parse_uci(p, "e7e8q"), MOVE_NONE);
}

TEST(threefold_repetition, "regra da tripla repetição") {
  Position p;
  CHECK(play_line(p, {"Nf3", "Nf6", "Ng1", "Ng8"}));
  CHECK_EQ(p.repetition_count(), 1);
  CHECK(game_result(p) == GameResult::Ongoing);
  CHECK(play_line(p, {"Nf3", "Nf6", "Ng1"}));
  CHECK(game_result(p) == GameResult::Ongoing);
  CHECK(play_line(p, {"Ng8"}));
  CHECK_EQ(p.repetition_count(), 2);
  CHECK(game_result(p) == GameResult::Threefold);
  // Lances de peão são irreversíveis: depois de e3 e6 a contagem recomeça, e a
  // posição nova aparece só duas vezes (uma repetição), não três.
  Position q;
  CHECK(play_line(q, {"Nf3", "Nf6", "Ng1", "Ng8", "e3", "e6", "Nf3", "Nf6", "Ng1", "Ng8"}));
  CHECK_EQ(q.repetition_count(), 1);
  CHECK(game_result(q) == GameResult::Ongoing);
}

TEST(fifty_move_rule, "regra dos 50 lances") {
  Position p = from_fen("4k3/8/8/8/8/8/8/R3K3 w - - 98 80");
  CHECK(play_line(p, {"Ra2"}));
  CHECK(game_result(p) == GameResult::Ongoing);
  CHECK(play_line(p, {"Kd7"}));
  CHECK_EQ(p.halfmove_clock(), 100);
  CHECK(game_result(p) == GameResult::FiftyMoves);
  // Lance de peão zera o contador.
  Position q = from_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 99 80");
  CHECK(play_line(q, {"e4"}));
  CHECK_EQ(q.halfmove_clock(), 0);
  // Mate no centésimo meio lance continua sendo mate.
  Position mate = from_fen("6k1/5ppp/8/8/8/8/8/R5K1 w - - 99 80");
  CHECK(play_line(mate, {"Ra8#"}));
  CHECK_EQ(mate.halfmove_clock(), 100);
  CHECK(game_result(mate) == GameResult::Checkmate);
}

TEST(insufficient_material, "material insuficiente") {
  CHECK(from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1").is_insufficient_material());
  CHECK(from_fen("4k3/8/8/8/8/8/8/4KB2 w - - 0 1").is_insufficient_material());
  CHECK(from_fen("4k3/8/8/8/8/8/8/4KN2 w - - 0 1").is_insufficient_material());
  CHECK(from_fen("4kb2/8/8/8/8/8/8/2B1K3 w - - 0 1").is_insufficient_material());   // os dois bispos em casas escuras
  CHECK(!from_fen("4k1b1/8/8/8/8/8/8/2B1K3 w - - 0 1").is_insufficient_material());  // cores diferentes
  CHECK(!from_fen("4k3/8/8/8/8/8/8/3NKN2 w - - 0 1").is_insufficient_material());    // dois cavalos
  CHECK(!from_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1").is_insufficient_material());
  CHECK(!from_fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1").is_insufficient_material());
  CHECK(game_result(from_fen("4k3/8/8/8/8/8/8/4KB2 w - - 0 1")) == GameResult::InsufficientMaterial);
}

TEST(mate_and_stalemate, "mate e afogamento") {
  Position fool;
  CHECK(play_line(fool, {"f3", "e5", "g4", "Qh4#"}));
  CHECK(game_result(fool) == GameResult::Checkmate);
  CHECK(game_result(from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")) == GameResult::Stalemate);
  CHECK(game_result(Position()) == GameResult::Ongoing);
}

TEST(eval_symmetry, "avaliação simétrica") {
  CHECK_EQ(evaluate(Position()), 10);  // posição simétrica: sobra só o bônus de quem tem a vez
  int asymmetric = 0;
  for (const std::string& fen : random_positions(99, 30, 70)) {
    const Position a = from_fen(fen);
    const Position b = from_fen(mirror_fen(fen));
    if (evaluate(a) != evaluate(b)) ++asymmetric;
  }
  CHECK_EQ(asymmetric, 0);
  // Uma dama a mais precisa valer muito.
  CHECK(evaluate(from_fen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1")) > 800);
  CHECK(evaluate(from_fen("4k3/8/8/8/8/8/8/3QK3 b - - 0 1")) < -800);
  CHECK(contains(eval_report(Position()), "Mobilidade"));
}

TEST(search_mate_in_one, "busca acha mate em 1") {
  const std::vector<std::string> puzzles = {
      "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1",                                      // mate no corredor
      "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4",       // mate do pastor
      "rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq g3 0 2",            // mate do louco
      "k7/7P/1K6/8/8/8/8/8 w - - 0 1",                                             // promoção com mate
  };
  for (const std::string& base : puzzles) {
    for (const std::string& fen : {base, mirror_fen(base)}) {
      Position pos = from_fen(fen);
      const SearchResult r = search_depth(pos, 4);
      CHECK(is_mate_score(r.score));
      CHECK_EQ(mate_in_moves(r.score), 1);
      pos.make_move(r.bestMove);
      CHECK(game_result(pos) == GameResult::Checkmate);
    }
  }
}

TEST(search_mate_in_two, "busca acha mate em 2 (conferido por força bruta)") {
  const std::vector<std::string> puzzles = {
      "r1b2k1r/ppp1bppp/8/1B1Q4/5q2/2P5/PPP2PPP/R3R1K1 w - - 1 1",  // Dd8+ Bxd8 Te8#
      "2r3k1/5ppp/8/8/8/8/4RPPP/4R1K1 w - - 0 1",                   // Te8+ Txe8 Txe8#
      "r6k/6pp/7N/8/8/1Q6/8/6K1 w - - 0 1",                         // Dg8+ Txg8 Cf7# (mate sufocado)
  };
  for (const std::string& base : puzzles) {
    for (const std::string& fen : {base, mirror_fen(base)}) {
      Position pos = from_fen(fen);
      // A força bruta confirma que o problema é mesmo um mate em 2, e não em 1.
      CHECK(!forced_mate(pos, 1));
      CHECK(forced_mate(pos, 2));
      const SearchResult r = search_depth(pos, 6);
      CHECK_EQ(mate_in_moves(r.score), 2);
      // E o lance escolhido pelo motor força o mate contra qualquer defesa.
      pos.make_move(r.bestMove);
      CHECK(every_reply_loses(pos, 2));
    }
  }
}

TEST(search_wins_hanging_queen, "busca captura a dama pendurada") {
  const Position pos = from_fen("rnb1kbnr/pppp1ppp/8/6q1/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3");
  const SearchResult r = search_depth(pos, 5);
  CHECK_EQ(move_to_uci(r.bestMove), std::string("f3g5"));
  CHECK(r.score > 600);
}

TEST(search_limits_and_terminal, "busca respeita limites e posições sem lances") {
  TranspositionTable tt(4);
  Searcher searcher(tt);
  SearchLimits nodes;
  nodes.nodes = 20000;
  const SearchResult byNodes = searcher.search(Position(), nodes);
  CHECK(byNodes.nodes <= 22000);
  CHECK(byNodes.bestMove != MOVE_NONE);

  SearchLimits timed;
  timed.movetime = 150;
  const auto start = std::chrono::steady_clock::now();
  const SearchResult byTime = searcher.search(from_fen(KIWIPETE), timed);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  CHECK(ms < 2000);
  CHECK(byTime.bestMove != MOVE_NONE);

  SearchLimits depth;
  depth.depth = 3;
  CHECK_EQ(searcher.search(Position(), depth).depth, 3);

  // Mate já dado e afogamento: nenhum lance, valores de mate e de empate.
  Position fool;
  CHECK(play_line(fool, {"f3", "e5", "g4", "Qh4#"}));
  const SearchResult mated = searcher.search(fool, depth);
  CHECK_EQ(mated.bestMove, MOVE_NONE);
  CHECK_EQ(mated.score, -VALUE_MATE);
  const SearchResult stalemate = searcher.search(from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"), depth);
  CHECK_EQ(stalemate.bestMove, MOVE_NONE);
  CHECK_EQ(stalemate.score, 0);
  // Rei contra rei e cavalo: empate morto vale zero.
  CHECK_EQ(searcher.search(from_fen("4k3/8/8/8/8/8/8/4KN2 w - - 0 1"), depth).score, 0);
}

TEST(uci_transcript, "transcrição UCI completa") {
  std::ostringstream out;
  UciEngine engine(out);
  CHECK(engine.handle("uci"));
  CHECK(engine.handle("isready"));
  CHECK(engine.handle("setoption name Hash value 8"));
  CHECK(engine.handle("ucinewgame"));
  CHECK(engine.handle("position startpos moves e2e4 e7e5"));
  CHECK_EQ(engine.position().fen(), std::string("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2"));
  CHECK(engine.handle("go depth 5"));
  CHECK(engine.handle("isready"));  // precisa responder mesmo durante a busca
  engine.wait_search();
  std::string text = out.str();
  CHECK(contains(text, "id name Motor de Xadrez"));
  CHECK(contains(text, "id author Michael Douglas"));
  CHECK(contains(text, "option name Hash type spin default 16 min 1 max 1024"));
  CHECK(contains(text, "uciok\n"));
  CHECK(contains(text, "readyok\n"));
  CHECK(contains(text, "info depth 1 "));
  CHECK(contains(text, "info depth 5 "));
  CHECK(contains(text, " score cp "));
  CHECK(contains(text, " nps "));
  CHECK(contains(text, " pv "));
  const std::size_t best = text.rfind("bestmove ");
  CHECK(best != std::string::npos);
  if (best != std::string::npos) {
    const std::string move = text.substr(best + 9, 4);
    CHECK(parse_uci(engine.position(), move) != MOVE_NONE);
  }

  out.str("");
  CHECK(engine.handle("position fen 6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1"));
  CHECK(engine.handle("go depth 4"));
  engine.wait_search();
  text = out.str();
  CHECK(contains(text, "score mate 1"));
  CHECK(contains(text, "bestmove a1a8"));

  out.str("");
  CHECK(engine.handle("position startpos"));
  CHECK(engine.handle("go infinite"));
  CHECK(engine.handle("isready"));
  CHECK(engine.handle("stop"));
  text = out.str();
  CHECK(contains(text, "readyok"));
  CHECK(contains(text, "bestmove "));

  out.str("");
  const auto start = std::chrono::steady_clock::now();
  CHECK(engine.handle("go movetime 100"));
  engine.wait_search();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  CHECK(ms < 2000);
  CHECK(contains(out.str(), "bestmove "));

  out.str("");
  CHECK(engine.handle("go wtime 3000 btime 3000 winc 100 binc 100 movestogo 20"));
  engine.wait_search();
  CHECK(contains(out.str(), "bestmove "));

  out.str("");
  CHECK(engine.handle("go nodes 3000"));
  engine.wait_search();
  CHECK(contains(out.str(), "bestmove "));

  out.str("");
  CHECK(engine.handle("position startpos moves f2f3 e7e5 g2g4 d8h4"));
  CHECK(engine.handle("go depth 3"));
  engine.wait_search();
  CHECK(contains(out.str(), "bestmove (none)"));

  out.str("");
  CHECK(engine.handle("position fen 8/8/8/8 w - - 0 1"));
  CHECK(engine.handle("position startpos moves e2e5"));
  CHECK(engine.handle("xyzzy"));
  CHECK(engine.handle("perft 2"));
  text = out.str();
  CHECK(contains(text, "info string FEN inválida"));
  CHECK(contains(text, "info string lance ilegal ignorado: e2e5"));
  CHECK(contains(text, "info string comando desconhecido: xyzzy"));
  CHECK(contains(text, "Nós: 400"));
  CHECK(!engine.handle("quit"));
}

TEST(terminal_play, "modo jogar no terminal") {
  PlayOptions options;
  options.depth = 2;

  std::istringstream in("e4\nlances\ndica\nxyz\nNd2\ndesfazer\nfen\nsair\n");
  std::ostringstream out;
  CHECK_EQ(play_game(in, out, options), 0);
  const std::string text = out.str();
  CHECK(contains(text, "você joga de brancas"));
  CHECK(contains(text, "Você jogou e4."));
  CHECK(contains(text, "Motor jogou: "));
  CHECK(contains(text, "Lances legais ("));
  CHECK(contains(text, "Dica: "));
  CHECK(contains(text, "Não entendi 'xyz'"));
  CHECK(contains(text, "Lance desfeito."));
  CHECK(contains(text, std::string(START_FEN) + "\n"));
  CHECK(contains(text, "Até a próxima!"));

  // Mate dado pelo jogador, em notação portuguesa.
  options.fen = "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1";
  std::istringstream mateIn("Ta8\nsair\n");
  std::ostringstream mateOut;
  play_game(mateIn, mateOut, options);
  CHECK(contains(mateOut.str(), "Você jogou Ta8#."));
  CHECK(contains(mateOut.str(), "Xeque-mate! Você venceu."));

  // "Re1" é rei em português e torre em inglês: o programa pede para esclarecer.
  options.fen = "4k3/8/8/8/8/8/8/R4K2 w - - 0 1";
  std::istringstream ambiguousIn("Re1\nsair\n");
  std::ostringstream ambiguousOut;
  play_game(ambiguousIn, ambiguousOut, options);
  CHECK(contains(ambiguousOut.str(), "Lance ambíguo: em português R é o rei"));

  // De pretas, o motor abre a partida.
  options.fen.clear();
  options.human = BLACK;
  options.notation = Notation::English;
  std::istringstream blackIn("sair\n");
  std::ostringstream blackOut;
  play_game(blackIn, blackOut, options);
  CHECK(contains(blackOut.str(), "você joga de pretas"));
  CHECK(contains(blackOut.str(), "Motor jogou: "));

  options.fen = "isto não é uma FEN";
  std::istringstream badIn("");
  std::ostringstream badOut;
  CHECK_EQ(play_game(badIn, badOut, options), 2);
  CHECK(contains(badOut.str(), "FEN inválida"));
}

TEST(portuguese_formatting, "formatação em português") {
  CHECK_EQ(format_eval(35, WHITE), std::string("+0,35"));
  CHECK_EQ(format_eval(35, BLACK), std::string("-0,35"));
  CHECK_EQ(format_eval(-120, WHITE), std::string("-1,20"));
  CHECK_EQ(format_eval(0, WHITE), std::string("0,00"));
  CHECK_EQ(format_eval(VALUE_MATE - 3, WHITE), std::string("mate em 2 para as brancas"));
  CHECK_EQ(format_eval(VALUE_MATE - 1, BLACK), std::string("mate em 1 para as pretas"));
  CHECK_EQ(format_thousands(1234567), std::string("1.234.567"));
  CHECK_EQ(format_thousands(999), std::string("999"));
  CHECK_EQ(format_uci_score(-VALUE_MATE + 2), std::string("mate -1"));
  CHECK_EQ(format_uci_score(-35), std::string("cp -35"));
}

int main() {
  init();
  return test::run_all();
}
