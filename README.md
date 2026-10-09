# Motor de Xadrez

Um engine de xadrez completo em C++17, escrito do zero e sem dependências: tabuleiro em bitboards com magic bitboards, geração de lances legais conferida por perft, busca alfa-beta com PVS, quiescência e tabela de transposição, e o protocolo UCI para jogar dentro do Arena ou do Cute Chess. Também dá para jogar contra ele no terminal, em português.

A ideia é ser um engine de verdade e, ao mesmo tempo, um material de estudo: o código é curto, comentado em português, e a seção [Como funciona](#como-funciona) explica cada peça do quebra-cabeça.

## Recursos

- **Tabuleiro em bitboards** (um inteiro de 64 bits por tipo de peça e por cor), com tabelas de ataque pré-calculadas para cavalo, rei e peão.
- **Magic bitboards** para bispo, torre e dama. Os números mágicos são encontrados na inicialização por busca aleatória com sementes fixas (cerca de 30 ms) e o resultado é conferido nos testes contra uma versão lenta, com laços por raio.
- **Geração de lances 100% legal**, sem "faz o lance e vê se o rei ficou em xeque": xeque simples e duplo, peças cravadas, roque sem passar por casa atacada, todas as promoções e o caso traiçoeiro do en passant que descobre xeque na fileira.
- **Hash Zobrist** incremental. O en passant só entra na chave quando a captura é realmente legal, então a detecção de repetição não se engana.
- **FEN** de entrada e saída, com validação (reis, peões na última fileira, roque sem torre, en passant impossível, rei de quem não joga em xeque) e mensagens de erro claras.
- **Fazer e desfazer** lances com todo o estado: direitos de roque, en passant, relógio dos 50 lances, chave e peça capturada.
- **Empates**: regra dos 50 lances, tripla repetição e material insuficiente (o mate no centésimo meio lance continua valendo).
- **Notação** UCI (`e2e4`) e algébrica (SAN) em inglês (`Nf3`) e em português (`Cf3`), com desambiguação mínima (`Nbd2`, `R1a3`, `Qa1b2`), xeque e mate.
- **perft e divide** conferidos com os valores publicados: a posição inicial na profundidade 6 (119.060.324 posições) leva menos de meio segundo.
- **Busca**: aprofundamento iterativo, PVS (alfa-beta com janela nula), janelas de aspiração, quiescência com MVV-LVA, tabela de transposição, lance nulo, redução de lances tardios (LMR), killers, histórico, extensão de xeque e pontuação de mate.
- **Gestão de tempo**: `wtime`, `btime`, `winc`, `binc`, `movestogo`, `movetime`, `depth`, `nodes`, `infinite` e `stop`.
- **Avaliação "tapered"** (interpola meio-jogo e final): tabelas de casas, mobilidade, segurança do rei, peões dobrados, isolados e passados, par de bispos e torres em colunas abertas.
- **UCI** com `uci`, `isready`, `ucinewgame`, `position startpos|fen ... moves ...`, `go`, `stop`, `quit` e `setoption name Hash`, e linhas `info` com depth, seldepth, score cp ou mate, nodes, nps, hashfull, time e pv. Extras para depuração: `d`, `eval` e `perft`.
- **Modo `xadrez jogar`** no terminal, em português: tabuleiro, lances em SAN (português ou inglês) ou UCI, `desfazer`, `dica`, `lances`, `fen` e `sair`.

## Como usar

Depois de compilar (veja [Como compilar](#como-compilar)), o executável fica em `build/xadrez`.

```
xadrez                      modo UCI, para Arena, Cute Chess e outras interfaces
xadrez jogar [opções]       joga contra o motor no terminal
xadrez perft <prof> [FEN]   conta as posições a uma profundidade fixa
xadrez divide <prof> [FEN]  perft separado por lance da raiz
xadrez bench [prof]         mede a velocidade da busca em posições fixas
xadrez ajuda                mostra a ajuda
```

### Numa interface gráfica

O motor fala UCI, o protocolo que praticamente todas as interfaces usam. Basta cadastrar o executável `build/xadrez` como motor UCI:

- **Arena**: menu *Engines*, *Install New Engine*, escolha o arquivo e confirme o tipo UCI.
- **Cute Chess**: *Tools*, *Settings*, aba *Engines*, botão de adicionar; em *Command* coloque o caminho do executável e em *Protocol* escolha UCI.

A única opção é `Hash` (tamanho da tabela de transposição, de 1 a 1024 MB; o padrão é 16), mais o botão `Clear Hash`.

### Conversando com o motor pelo protocolo

Dá para digitar os comandos UCI direto no terminal, o que é ótimo para entender o protocolo. As linhas que começam com `info` mostram cada iteração da busca:

```console
$ ./build/xadrez
Motor de Xadrez 1.0 (protocolo UCI). Digite 'uci' para começar ou 'quit' para sair.
uci
id name Motor de Xadrez 1.0
id author Michael Douglas
option name Hash type spin default 16 min 1 max 1024
option name Clear Hash type button
uciok
isready
readyok
position startpos moves e2e4 e7e5
go depth 8
info depth 1 seldepth 3 score cp 54 nodes 36 nps 36000 hashfull 0 time 0 pv b1c3
info depth 2 seldepth 6 score cp 10 nodes 163 nps 163000 hashfull 0 time 0 pv b1c3 b8c6
info depth 3 seldepth 9 score cp 43 nodes 401 nps 401000 hashfull 0 time 0 pv b1c3 b8c6 g1f3
info depth 4 seldepth 13 score cp 10 nodes 2549 nps 2549000 hashfull 1 time 0 pv b1c3 b8c6 g1f3 g8f6
info depth 5 seldepth 11 score cp 44 nodes 5497 nps 5497000 hashfull 2 time 1 pv b1c3 b8c6 g1f3 g8f6 d2d4
info depth 6 seldepth 18 score cp 19 nodes 13854 nps 6927000 hashfull 2 time 2 pv b1c3 b8c6 g1f3 f8c5 f1b5 g8e7
info depth 7 seldepth 20 score cp 26 nodes 27740 nps 6935000 hashfull 5 time 4 pv b1c3 b8c6 g1f3 f8c5 f1c4 g8f6 e1g1
info depth 8 seldepth 21 score cp 35 nodes 75224 nps 6268666 hashfull 14 time 12 pv b1c3 b8c6 g1f3 g8f6 d2d4 f8b4 c1g5 e5d4
bestmove b1c3 ponder b8c6
quit
```

Um mate sufocado em 2 lances (dama sacrificada em g8, torre recaptura e o cavalo dá mate em f7):

```console
position fen r6k/6pp/7N/8/8/1Q6/8/6K1 w - - 0 1
go depth 5
info depth 1 seldepth 2 score cp 502 nodes 45 nps 45000 hashfull 0 time 0 pv h6f5
info depth 2 seldepth 6 score mate 2 nodes 204 nps 204000 hashfull 1 time 0 pv b3g8 a8g8 h6f7
...
bestmove b3g8 ponder a8g8
```

Comandos extras, que não fazem parte do UCI mas ajudam a depurar: `d` desenha o tabuleiro e mostra FEN e chave Zobrist, `eval` detalha a avaliação termo a termo e `perft <n>` faz o divide da posição atual.

### Jogando no terminal

```console
$ ./build/xadrez jogar --profundidade 8
Motor de Xadrez 1.0: você joga de brancas. Digite 'ajuda' para ver os comandos.
As avaliações são em peões, do ponto de vista das brancas (positivo é bom para as brancas).

  8  t c b d r b c t
  7  p p p p p p p p
  6  . . . . . . . .
  5  . . . . . . . .
  4  . . . . . . . .
  3  . . . . . . . .
  2  P P P P P P P P
  1  T C B D R B C T
     a b c d e f g h

Seu lance: e4
Você jogou e4.
...
O motor está pensando...
Motor jogou: d5 (avaliação +0,19, profundidade 8, 159.699 nós em 0,0 s)
```

Os lances podem ser digitados em SAN português (`Cf3`, `Bb5`, `Txe8`, `Dd1`, `O-O`, `e8=D`), em SAN inglês (`Nf3`, `Qd1`) ou no formato UCI (`g1f3`, `e7e8q`). O único conflito entre as duas línguas é a letra R (rei em português, torre em inglês): quando os dois sentidos são possíveis o programa pergunta qual você quis dizer.

| Comando | O que faz |
|---|---|
| `desfazer` | volta o seu último lance e a resposta do motor |
| `dica` | o motor sugere um lance para você |
| `lances` | lista os lances legais |
| `tabuleiro` | desenha o tabuleiro de novo |
| `fen` | mostra a posição em FEN |
| `ajuda` | lista os comandos |
| `sair` | encerra a partida |

Opções: `--pretas` (o motor começa), `--tempo <ms>` (tempo por lance, padrão 1000), `--profundidade <n>` (profundidade fixa), `--fen "<FEN>"` (posição inicial), `--notacao pt|en` (letras das peças no tabuleiro e nos lances do motor) e `--unicode` (desenha as peças com símbolos).

### perft e divide

```console
$ ./build/xadrez perft 6
perft(6) = 119060324 em 0,277 s (428,4 milhões de posições por segundo)
$ ./build/xadrez divide 3
a2a3: 380
b2b3: 420
...
h2h4: 420

Lances: 20
Total: 8902
```

O `divide` mostra quantas posições existem abaixo de cada lance da raiz. É a ferramenta para caçar bugs no gerador: compare com outro motor, desça no lance em que os números divergem e repita até achar a posição problemática.

## Como compilar

Precisa de um compilador C++17 (clang ou gcc). Só a biblioteca padrão é usada, inclusive `std::thread` para a busca rodar enquanto o motor continua ouvindo comandos.

```bash
make            # gera build/xadrez
make test       # testes de unidade, perft rápido e testes de ponta a ponta
make clean
```

Ou com CMake:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Testado no macOS com clang. O código é C++17 padrão, sem APIs de sistema operacional; os testes de ponta a ponta usam `sh`.

## Testes

```bash
make test    # unidade + perft até 5 milhões de posições + transcrição UCI pelo executável
make perft   # suíte perft completa: 52 contagens em 21 posições, cerca de 820 milhões de posições
make asan    # tudo de novo com AddressSanitizer e UndefinedBehaviorSanitizer
make tsan    # tudo de novo com ThreadSanitizer (a busca roda em outra thread)
```

O que é verificado:

- **perft** (`tests/perft.epd`): a posição inicial até a profundidade 6, Kiwipete até 5, as posições 3, 4 (e a versão espelhada), 5 e 6 da wiki de programação de xadrez, e 14 posições armadilha (en passant ilegal por xeque descoberto, roque que dá xeque, roque impedido, promoção para sair do xeque, subpromoção com xeque, afogamento e mate). O `make test` roda todas as contagens até 5 milhões, o que inclui todas as profundidades pedidas na suíte clássica (inicial até 5, Kiwipete até 4, posição 3 até 5, posição 4 até 4, posição 5 até 3).
- **Magic bitboards** contra o laço por raios em 19.200 ocupações aleatórias.
- **FEN** de ida e volta e 15 FENs inválidas recusadas sem estragar a posição.
- **Zobrist**: em 300 partidas aleatórias a chave incremental bate com a calculada do zero a cada lance, e desfazer tudo devolve a FEN e a chave originais; transposições dão a mesma chave.
- **SAN e UCI** de ida e volta em mais de 5 mil lances, nas duas notações, mais desambiguação, promoção, en passant, roque e mate.
- **Regras**: tripla repetição, 50 lances (e mate no centésimo meio lance), material insuficiente, mate e afogamento.
- **Avaliação simétrica**: a posição espelhada (cores trocadas) tem exatamente a mesma avaliação.
- **Mates em 1 e em 2**, nas versões normal e espelhada. Os de 2 lances são conferidos por uma força bruta independente da busca: ela confirma que não existe mate em 1, que existe mate em 2 e que o lance escolhido pelo motor força o mate contra qualquer defesa.
- **Busca**: captura da dama pendurada, limites de nós, tempo e profundidade, posições sem lances.
- **UCI**: transcrição completa em processo (com `isready` durante a busca, `go infinite` e `stop`, relógio, `nodes`, FEN inválida, lance ilegal) e outra pelo executável de verdade, lendo e escrevendo na entrada e saída padrão.
- **Modo jogar**: sessão completa com lances, `lances`, `dica`, `desfazer`, mate dado pelo jogador e o conflito da letra R.

## Como funciona

### 1. Bitboards

Um tabuleiro tem 64 casas e um inteiro sem sinal tem 64 bits, então um conjunto de casas cabe em um `uint64_t`: o bit 0 é a1, o bit 7 é h1, o bit 56 é a8 e o bit 63 é h8 (casa = 8 × fileira + coluna). O motor guarda um bitboard por tipo de peça e um por cor; "cavalos brancos" é só `pieces(WHITE) & pieces(KNIGHT)`.

```
fileira 8   56 57 58 59 60 61 62 63
fileira 7   48 49 50 51 52 53 54 55
...
fileira 2    8  9 10 11 12 13 14 15
fileira 1    0  1  2  3  4  5  6  7
             a  b  c  d  e  f  g  h
```

A graça é que operações sobre o tabuleiro inteiro viram poucas instruções: `ataques & ~minhasPecas` são os destinos possíveis, `b & (b - 1)` apaga o bit mais baixo, e contar ou achar bits ligados é feito com truques clássicos (SWAR para contar e sequência de De Bruijn para achar o bit mais baixo, ambos em `bitboard.h`), sem depender de extensões do compilador.

### 2. Tabelas de ataque

Cavalo, rei e peão sempre atacam as mesmas casas a partir de uma casa, não importa o que esteja no tabuleiro. Então os ataques são calculados uma vez só, na inicialização: `Knight[64]`, `King[64]` e `Pawn[cor][64]`. Gerar os lances de um cavalo em d4 é ler `Knight[d4] & ~minhasPecas`.

### 3. Peças deslizantes e magic bitboards

Bispo, torre e dama são diferentes: os ataques dependem das peças que bloqueiam o caminho. A solução simples é andar raio a raio até bater em alguém (é o que `sliding_slow` faz). Funciona, mas é lento para um gerador que roda centenas de milhões de vezes por segundo.

Os **magic bitboards** transformam isso numa consulta de tabela. Para uma torre em d4:

1. Só importam as casas que podem bloquear: a coluna e a fileira da torre, **sem** as bordas (a peça na borda nunca bloqueia nada depois dela). Essa é a **máscara**, com no máximo 12 casas para a torre e 9 para o bispo.
2. Existem 2^bits ocupações diferentes da máscara. A ideia é achar um número **mágico** `M` tal que `((ocupação & máscara) × M) >> (64 - bits)` dê um índice diferente para ocupações com ataques diferentes (colisões só são permitidas quando os ataques são iguais).
3. Com esse número, os ataques ficam pré-calculados numa tabela, e em tempo de jogo o custo é um AND, uma multiplicação, um deslocamento e uma leitura de memória.

Não existe fórmula para os números mágicos: eles são achados por tentativa e erro. `init_magics` enumera todas as ocupações de cada máscara (com o truque `b = (b - máscara) & máscara`, que percorre todos os subconjuntos), sorteia candidatos com poucos bits ligados e testa cada um até achar um sem colisões destrutivas. As sementes são fixas, então os números saem sempre iguais, em cerca de 30 ms. Os testes conferem a versão mágica contra a versão lenta em milhares de ocupações aleatórias.

Por que magic e não laços por raio? Desempenho: o perft da posição inicial na profundidade 6 passa de 400 milhões de posições por segundo num Mac com Apple Silicon. A versão com laços continua no código como referência e para montar as tabelas.

### 4. Zobrist hashing

Para reconhecer uma posição já vista (tabela de transposição e repetição) cada combinação peça e casa recebe um número aleatório de 64 bits, assim como cada estado de roque, cada coluna de en passant e o lado da vez. A chave da posição é o XOR de tudo o que está presente. Como XOR é a própria inversa, fazer um lance só precisa "tirar" a peça da origem e "pôr" no destino:

```
chave ^= Z[cavalo branco][g1] ^ Z[cavalo branco][f3] ^ Z[vez]
```

Um detalhe que muita gente erra: a FEN registra a casa de en passant depois de qualquer avanço duplo, mas duas posições só são iguais (para a regra da repetição) se as capturas possíveis forem as mesmas. Por isso a coluna de en passant só entra na chave quando existe uma captura en passant **legal**.

### 5. Gerando só lances legais

O jeito ingênuo é gerar lances "pseudolegais", fazer cada um e descartar os que deixam o rei em xeque. Este motor gera direto os legais, o que é mais rápido e permite a contagem em bloco no perft:

1. **Rei**: vai para casas vizinhas não atacadas. O ataque é calculado **sem o próprio rei** no tabuleiro; senão, um rei em xeque de torre acharia que pode fugir "para trás", na mesma linha, escondido atrás de si mesmo.
2. **Xeque duplo**: só o rei pode se mover.
3. **Xeque simples**: as outras peças só podem capturar o atacante ou se colocar entre ele e o rei. Isso vira uma máscara: `Between[rei][atacante] | atacante`.
4. **Cravadas**: para cada torre, bispo ou dama inimigos alinhados com o rei (olhando o tabuleiro vazio), se entre eles houver exatamente uma peça e ela for nossa, essa peça está cravada e só pode andar sobre a linha `Line[rei][peça]`. Cavalo cravado não anda nunca.
5. **Roque**: o rei não pode estar em xeque, as casas entre rei e torre precisam estar vazias e o rei não pode passar nem chegar em casa atacada.
6. **En passant**: é testado simulando o tabuleiro sem os dois peões. O caso clássico:

```
  8  . . . . . . . .
  7  . . . . . . . .
  6  . . . . . . . .      as pretas jogaram c7-c5;
  5  K P p . . . . r      bxc6 en passant tira b5 E c5 da fileira 5
  4  . . . . . . . .      e deixa o rei de a5 em xeque da torre de h5
```

Nenhuma máscara de cravada pega isso, porque duas peças saem da linha ao mesmo tempo. A posição 3 da suíte perft (`8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1`) existe justamente para pegar esse bug.

### 6. Perft: como saber que o gerador está certo

Perft (*performance test*) conta quantas posições existem a N meios lances de distância, gerando e fazendo todos os lances. Os números certos para várias posições são conhecidos, então qualquer diferença indica bug: um roque esquecido, uma promoção faltando, um en passant ilegal.

| Posição | Profundidade | Posições |
|---|---|---|
| Inicial | 5 | 4.865.609 |
| Inicial | 6 | 119.060.324 |
| Kiwipete | 4 | 4.085.603 |
| Kiwipete | 5 | 193.690.690 |
| Posição 3 | 7 | 178.633.661 |
| Posição 4 | 5 | 15.833.292 |
| Posição 5 | 5 | 89.941.194 |

Na última camada não é preciso fazer os lances: basta contar quantos lances legais existem (contagem em bloco). É por isso que gerar só lances legais compensa.

### 7. Busca: do minimax ao alfa-beta

**Minimax**: eu escolho o lance que maximiza a avaliação, supondo que o adversário sempre responde com o lance que a minimiza. Escrito do ponto de vista de quem joga, vira o **negamax**: o valor de uma posição é o máximo de `-valor(filho)`.

**Alfa-beta** poda galhos que não podem mudar a decisão. A janela `[alfa, beta]` diz: "já tenho garantido alfa; o adversário já tem garantido que eu não passo de beta". Se um lance meu chega em beta, o adversário nunca vai deixar esta posição acontecer, e o resto dos lances nem precisa ser olhado (corte beta). Com a melhor ordem possível, o alfa-beta analisa algo perto da raiz quadrada dos nós do minimax, o que na prática dobra a profundidade alcançada.

Tudo o que vem depois serve para ordenar melhor os lances ou para gastar menos tempo com galhos ruins:

- **Aprofundamento iterativo**: busca com profundidade 1, depois 2, 3... Parece desperdício, mas cada iteração deixa a tabela de transposição e os killers prontos para ordenar a próxima, e sempre existe um lance pronto quando o tempo acaba.
- **PVS** (*principal variation search*): o primeiro lance é buscado com a janela inteira; os outros com uma janela nula `[alfa, alfa + 1]`, que só responde "é melhor ou não". Se algum surpreender, é buscado de novo com a janela inteira.
- **Janelas de aspiração**: a partir da profundidade 5, a raiz começa com uma janela estreita em volta do valor anterior, que alarga se o resultado cair fora dela.
- **Tabela de transposição**: guarda, para cada chave Zobrist, o melhor lance, a profundidade e o valor, junto com o tipo de limite (exato, inferior depois de um corte beta, superior quando nenhum lance superou alfa). Posições repetidas por outra ordem de lances são resolvidas na hora, e o lance guardado é o primeiro a ser tentado.
- **Ordenação**: lance da tabela, capturas por **MVV-LVA** (vítima mais valiosa, atacante menos valioso: PxD antes de DxP), promoção a dama, dois **killers** por altura (lances quietos que causaram corte em posições irmãs) e o **histórico** (quantas vezes cada origem e destino causaram corte).
- **Lance nulo**: se mesmo passando a vez a posição continua acima de beta numa busca mais rasa, ela é boa demais e o galho é cortado. Não é usado em xeque nem sem peças (zugzwang nos finais de peões).
- **LMR**: lances quietos que aparecem tarde na ordenação são buscados com profundidade menor; se surpreenderem, são buscados de novo normalmente.
- **Extensão de xeque**: em xeque a profundidade aumenta um nível, para nunca parar a análise no meio de uma sequência forçada.
- **Mate**: a pontuação de mate é `32000 - meios lances até o mate`, então mate mais curto vale mais. Na tabela de transposição o valor é ajustado pela altura do nó, para continuar certo quando a mesma posição aparecer em outra profundidade. O UCI mostra `score mate N` em lances.

### 8. Quiescência

Parar a busca no meio de uma troca dá resultados absurdos: "capturei a dama com a torre" vale +9 se a análise acabar ali, mesmo que o peão recapture a torre no lance seguinte (o **efeito horizonte**). A quiescência continua depois da profundidade nominal só com capturas e promoções, até a posição ficar quieta. Em cada nó o lado da vez pode "ficar parado" (*stand pat*) com a avaliação estática, porque normalmente existe algum lance quieto pelo menos tão bom. Em xeque não existe ficar parado: todas as respostas são analisadas, o que também acha mates na quiescência. Capturas que nem ganhando a peça chegariam em alfa são puladas (poda delta).

### 9. Avaliação

A avaliação conta centipeões (100 = um peão) e tem dois valores para cada termo: meio-jogo e final. A **fase** vai de 24 (todas as peças) a 0 (só reis e peões) e o valor final é a média ponderada: `(mg × fase + eg × (24 - fase)) / 24`. Assim o rei quer ficar escondido no meio-jogo e ir para o centro no final, sem um "pulo" no meio do caminho.

- **Material e tabelas de casas**: cavalo no centro vale mais que na borda, peão avançado vale mais no final (os valores seguem a ideia da *Simplified Evaluation Function* de Tomasz Michniewski).
- **Mobilidade**: casas alcançáveis por cavalos, bispos, torres e damas, sem contar as atacadas por peões inimigos.
- **Segurança do rei**: escudo de peões na frente do rei e pressão das peças inimigas sobre as casas em volta dele (cresce com o quadrado do número de ataques).
- **Estrutura de peões**: dobrados e isolados perdem pontos; passados ganham mais quanto mais avançados.
- **Par de bispos** e **torres em colunas abertas ou semiabertas**.

O comando `eval` mostra cada termo para a posição atual. Um teste garante que a avaliação é simétrica: trocar as cores e virar o tabuleiro dá exatamente o mesmo valor.

### 10. Empates

- **50 lances**: o relógio de meios lances zera em toda captura ou lance de peão; ao chegar em 100 é empate (a não ser que o último lance tenha dado mate).
- **Repetição**: cada lance empilha a chave Zobrist. Só é preciso olhar para trás de dois em dois (mesmo lado na vez) e só até o último lance irreversível. Na busca uma única repetição já conta como empate, porque se a posição se repetiu uma vez ela pode se repetir de novo; na partida vale a regra oficial de três vezes.
- **Material insuficiente**: rei contra rei, rei e uma peça menor contra rei, ou só bispos, todos na mesma cor de casa. Dois cavalos não entram: o mate é possível se o adversário colaborar.

### 11. Gestão de tempo e UCI

A busca roda numa thread separada; a thread principal continua lendo comandos, então `isready` é respondido na hora e `stop` interrompe a busca. A parada é cooperativa: uma flag atômica é consultada a cada nó e o relógio a cada 1024 nós. A primeira iteração sempre termina, para existir um lance legal para responder.

Com relógio (`wtime`, `btime`, incrementos e `movestogo`), o motor calcula um limite suave (tempo restante dividido pelos lances que faltam, mais 3/4 do incremento) e um rígido (até 4 vezes o suave, sem passar de 90% do relógio). Depois de cada iteração ele só começa outra se ainda não usou 60% do limite suave, porque cada iteração custa várias vezes a anterior. Com `movetime` usa o tempo inteiro; com `go infinite` só responde depois do `stop`, como manda o protocolo.

### 12. Estrutura do código

| Arquivo | Conteúdo |
|---|---|
| `src/types.h` | cores, peças, casas, codificação do lance em 16 bits |
| `src/bitboard.*` | contagem de bits, tabelas de ataque e magic bitboards |
| `src/position.*` | tabuleiro, FEN, fazer e desfazer lances, Zobrist, repetição |
| `src/movegen.*` | gerador legal, perft e resultado da partida |
| `src/notation.*` | UCI, SAN em inglês e português, diagrama do tabuleiro |
| `src/eval.*` | avaliação tapered |
| `src/tt.*` | tabela de transposição |
| `src/search.*` | aprofundamento iterativo, PVS, quiescência, tempo |
| `src/uci.*` | protocolo UCI com a busca em outra thread |
| `src/play.*` | modo `xadrez jogar` |

## Limitações

- Xadrez clássico apenas (sem Chess960) e uma thread de busca.
- Sem ponder, livro de aberturas ou tablebases de finais.
- A força não foi medida em Elo. Ele ganha de jogadores casuais e acha táticas curtas com facilidade, mas está longe dos motores de ponta.

## Contribuindo

Issues e pull requests são bem-vindos. Antes de mandar uma mudança no gerador de lances, rode `make perft`: uma única contagem errada indica bug.

## Licença

MIT. Veja [LICENSE](LICENSE).
