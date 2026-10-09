# Motor de Xadrez
#   make            compila build/xadrez
#   make test       testes de unidade, perft rápido e transcrição UCI
#   make perft      suíte perft completa (todas as profundidades do tests/perft.epd)
#   make asan       testes com AddressSanitizer e UndefinedBehaviorSanitizer
#   make tsan       testes com ThreadSanitizer
#   make clean      apaga a pasta build/

CXXFLAGS ?= -O2
LDFLAGS ?=
BUILD ?= build

WARNINGS = -std=c++17 -Wall -Wextra -Wpedantic
THREADS = -pthread

LIB_SRC = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIB_OBJ = $(LIB_SRC:src/%.cpp=$(BUILD)/obj/%.o)
HEADERS = $(wildcard src/*.h)

all: $(BUILD)/xadrez

$(BUILD)/obj:
	mkdir -p $@

$(BUILD)/obj/%.o: src/%.cpp $(HEADERS) | $(BUILD)/obj
	$(CXX) $(WARNINGS) $(CXXFLAGS) $(THREADS) -c -o $@ $<

$(BUILD)/xadrez: $(LIB_OBJ) $(BUILD)/obj/main.o
	$(CXX) $(CXXFLAGS) $(THREADS) $(LDFLAGS) -o $@ $^

$(BUILD)/testes: tests/testes.cpp tests/teste.h $(LIB_OBJ)
	$(CXX) $(WARNINGS) $(CXXFLAGS) $(THREADS) -Isrc $(LDFLAGS) -o $@ tests/testes.cpp $(LIB_OBJ)

$(BUILD)/testes_perft: tests/perft.cpp $(LIB_OBJ)
	$(CXX) $(WARNINGS) $(CXXFLAGS) $(THREADS) -Isrc $(LDFLAGS) -o $@ tests/perft.cpp $(LIB_OBJ)

test: $(BUILD)/xadrez $(BUILD)/testes $(BUILD)/testes_perft
	./$(BUILD)/testes
	./$(BUILD)/testes_perft tests/perft.epd --limite 5000000
	sh tests/uci.sh ./$(BUILD)/xadrez

perft: $(BUILD)/testes_perft
	./$(BUILD)/testes_perft tests/perft.epd

asan:
	$(MAKE) BUILD=build/asan CXXFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" test

tsan:
	$(MAKE) BUILD=build/tsan CXXFLAGS="-O1 -g -fsanitize=thread" test

clean:
	rm -rf build

.PHONY: all test perft asan tsan clean
