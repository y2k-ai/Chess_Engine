cmake_minimum_required(VERSION 3.20)
project(chess-engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /arch:AVX2)
    # Use configuration-specific optimization flags to avoid /RTC1 conflicts
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /O2")
else()
    add_compile_options(-O3 -mbmi2 -Wall -Wextra)
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
