#!/bin/sh
# Testes de ponta a ponta: conversa com o executável pela entrada e saída padrão,
# do jeito que uma interface gráfica faz.
# Uso: sh tests/uci.sh ./build/xadrez
set -u
BIN="$1"
failures=0

expect() {
  if printf '%s\n' "$OUTPUT" | grep -q -- "$1"; then
    echo "  ok     $2"
  else
    echo "  FALHOU $2 (faltou: $1)"
    printf '%s\n' "$OUTPUT" | sed 's/^/         | /'
    failures=$((failures + 1))
  fi
}

OUTPUT=$(printf 'uci\nisready\nsetoption name Hash value 8\nucinewgame\nposition startpos moves e2e4 e7e5 g1f3\ngo depth 6\nisready\n' | "$BIN")
expect '^id name Motor de Xadrez' 'identificação'
expect '^option name Hash type spin' 'opção Hash'
expect '^uciok$' 'uciok'
expect '^readyok$' 'readyok'
expect '^info depth 6 seldepth [0-9]* score cp -*[0-9]* nodes [0-9]* nps [0-9]* hashfull [0-9]* time [0-9]* pv ' 'linha info completa'
expect '^bestmove [a-h][1-8][a-h][1-8]' 'bestmove depois da busca por profundidade'

OUTPUT=$(printf 'position fen 6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1\ngo depth 4\n' | "$BIN")
expect 'score mate 1 ' 'mate em 1 anunciado'
expect '^bestmove a1a8' 'mate em 1 jogado'

OUTPUT=$(printf 'position startpos\ngo infinite\nisready\nstop\nquit\n' | "$BIN")
expect '^readyok$' 'isready durante go infinite'
expect '^bestmove ' 'stop encerra go infinite'

OUTPUT=$(printf 'position startpos moves e2e4\ngo wtime 2000 btime 2000 winc 0 binc 0\n' | "$BIN")
expect '^bestmove ' 'busca com relógio'

OUTPUT=$(printf 'position startpos\nperft 3\n' | "$BIN")
expect 'Nós: 8902' 'perft pelo protocolo'

OUTPUT=$("$BIN" perft 4)
expect 'perft(4) = 197281' 'comando perft'

OUTPUT=$("$BIN" divide 1 "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1")
expect 'Total: 14' 'comando divide'

OUTPUT=$(printf 'e4\ndica\nsair\n' | "$BIN" jogar --profundidade 2)
expect 'Você jogou e4.' 'jogar: lance do jogador'
expect 'Motor jogou: ' 'jogar: resposta do motor'
expect 'Dica: ' 'jogar: dica'

OUTPUT=$("$BIN" ajuda)
expect 'xadrez jogar' 'ajuda'

if "$BIN" perft 0 >/dev/null 2>&1; then
  echo "  FALHOU perft 0 deveria sair com erro"
  failures=$((failures + 1))
else
  echo "  ok     perft 0 recusado"
fi
if "$BIN" jogar --fen "nada" </dev/null >/dev/null 2>&1; then
  echo "  FALHOU FEN inválida deveria sair com erro"
  failures=$((failures + 1))
else
  echo "  ok     FEN inválida recusada"
fi

if [ "$failures" -ne 0 ]; then
  echo "uci.sh: $failures falhas"
  exit 1
fi
echo "uci.sh: todos os testes de ponta a ponta passaram"
