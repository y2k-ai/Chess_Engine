cmake_minimum_required(VERSION 3.20)
project(chess-engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /W4)
else()
    add_compile_options(-O3 -Wall -Wextra)
endif()

set(ENGINE_SOURCES
    src/board.cpp
    src/magic.cpp
    src/movegen.cpp
    src/search.cpp
    src/eval.cpp
    src/uci.cpp
)

add_executable(chess-engine src/main.cpp ${ENGINE_SOURCES})
target_include_directories(chess-engine PRIVATE src)

add_executable(perft tests/perft.cpp ${ENGINE_SOURCES})
target_include_directories(perft PRIVATE src)
