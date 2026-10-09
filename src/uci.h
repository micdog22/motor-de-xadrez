// Protocolo UCI: a conversa entre o motor e interfaces como Arena e Cute Chess.
#ifndef XADREZ_UCI_H
#define XADREZ_UCI_H

#include <iosfwd>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "search.h"

namespace xadrez {

extern const char* const ENGINE_NAME;
extern const char* const ENGINE_AUTHOR;

std::string format_uci_score(int score);

class UciEngine {
 public:
  explicit UciEngine(std::ostream& out);
  ~UciEngine();
  UciEngine(const UciEngine&) = delete;
  UciEngine& operator=(const UciEngine&) = delete;

  // Lê comandos até "quit" ou até a entrada acabar.
  void loop(std::istream& in);
  // Executa um comando. Devolve false quando o comando é "quit".
  bool handle(const std::string& line);
  // Espera a busca atual terminar sozinha (útil em testes e no fim da entrada).
  void wait_search();
  // Pede para a busca parar e espera a resposta "bestmove".
  void stop_search();
  const Position& position() const { return pos_; }

 private:
  void send(const std::string& line);
  void cmd_uci();
  void cmd_position(std::istringstream& is);
  void cmd_go(std::istringstream& is);
  void cmd_setoption(std::istringstream& is);
  void cmd_display();
  void cmd_perft(int depth);

  std::ostream& out_;
  std::mutex outMutex_;
  Position pos_;
  TranspositionTable tt_;
  std::unique_ptr<Searcher> searcher_;
  std::thread worker_;
  bool infiniteSearch_ = false;
};

}  // namespace xadrez

#endif
