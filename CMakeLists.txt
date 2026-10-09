cmake_minimum_required(VERSION 3.16)
project(motor_de_xadrez LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_library(xadrez_core STATIC
  src/bitboard.cpp
  src/eval.cpp
  src/movegen.cpp
  src/notation.cpp
  src/play.cpp
  src/position.cpp
  src/search.cpp
  src/tt.cpp
  src/uci.cpp)
target_include_directories(xadrez_core PUBLIC src)
target_link_libraries(xadrez_core PUBLIC Threads::Threads)

add_executable(xadrez src/main.cpp)
add_executable(testes tests/testes.cpp)
add_executable(testes_perft tests/perft.cpp)

foreach(alvo xadrez_core xadrez testes testes_perft)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${alvo} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
foreach(alvo xadrez testes testes_perft)
  target_link_libraries(${alvo} PRIVATE xadrez_core)
endforeach()

enable_testing()
add_test(NAME unidade COMMAND testes)
add_test(NAME perft COMMAND testes_perft ${CMAKE_CURRENT_SOURCE_DIR}/tests/perft.epd --limite 5000000)
add_test(NAME ponta_a_ponta COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/uci.sh $<TARGET_FILE:xadrez>)
