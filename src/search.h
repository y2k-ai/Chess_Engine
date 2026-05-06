#pragma once
#include "board.h"
#include <atomic>

constexpr int INF     = 1000000;
constexpr int MATE    = 100000;
constexpr int MAX_PLY = 128;
constexpr int TT_SIZE = 1 << 23;

enum TTFlag : uint8_t { TT_NONE = 0, TT_EXACT, TT_LOWER, TT_UPPER };

struct TTEntry {
    uint64_t hash  = 0;
    Move     move  = 0;
    int16_t  score = 0;
    uint8_t  depth = 0;
    TTFlag   flag  = TT_NONE;
};

struct SearchLimits {
    int  depth    = 0;
    int  movetime = 0;
    int  wtime    = 0, btime = 0;
    int  winc     = 0, binc  = 0;
    bool infinite = false;
};

struct SearchResult {
    Move     best_move = 0;
    int      score     = 0;
    int      depth     = 0;
    uint64_t nodes     = 0;
};

extern std::atomic<bool> stop_search;

SearchResult search(Board& b, const SearchLimits& limits);
void         tt_clear();
void         rep_push(uint64_t hash);
void         rep_clear();
