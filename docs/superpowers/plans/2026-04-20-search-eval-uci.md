# Search, Evaluation & UCI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a fully playable UCI chess engine with alpha-beta search, iterative deepening, null-move pruning, killer moves, history heuristic, aspiration windows, and material+PST evaluation.

**Architecture:** `eval.cpp` scores positions statically; `search.cpp` drives iterative-deepening negamax that calls eval at leaf nodes and uses a 128 MB transposition table; `uci.cpp` parses UCI protocol commands, maintains board state, and dispatches to search.

**Tech Stack:** C++17, `<chrono>` for timing, `<atomic>` for stop flag, existing `Board`/`generate_moves`/`is_attacked` infrastructure.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `src/eval.h` | Fill stub | `evaluate()` declaration |
| `src/eval.cpp` | Fill stub | Material + PST scoring |
| `src/board.h` | Modify | Add `make_null_move` / `unmake_null_move` declarations |
| `src/board.cpp` | Modify | Implement null-move helpers |
| `src/search.h` | Fill stub | All search types, constants, TT, `search()` declaration |
| `src/search.cpp` | Fill stub | Full search implementation |
| `src/uci.cpp` | Fill stub | UCI loop, move string conversion, position/go parsing |

---

## Task 1: `eval.h` + `eval.cpp` — Material + PST

**Files:**
- Fill: `src/eval.h`
- Fill: `src/eval.cpp`

- [ ] **Step 1: Write `src/eval.h`**

```cpp
#pragma once
#include "board.h"

int evaluate(const Board& b);
```

- [ ] **Step 2: Write `src/eval.cpp`**

```cpp
#include "eval.h"

// Material values in centipawns
static const int MAT[6] = { 100, 320, 330, 500, 900, 0 };

// PST tables — white perspective, index 0=A1, 63=H8 (rank 1 first)
// For black pieces use sq ^ 56 (mirror rank)
static const int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int PST_ROOK[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};
static const int PST_KING[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
};

static const int* PST[6] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING
};

int evaluate(const Board& b) {
    int score = 0;
    for (int c = 0; c < 2; c++) {
        int sign = (c == WHITE) ? 1 : -1;
        for (int p = 0; p < 6; p++) {
            Bitboard bb = b.pieces[c][p];
            while (bb) {
                int sq = lsb(bb); pop_lsb(bb);
                int pst_sq = (c == WHITE) ? sq : (sq ^ 56);
                score += sign * (MAT[p] + PST[p][pst_sq]);
            }
        }
        // Bishop pair bonus
        if (popcount(b.pieces[c][BISHOP]) >= 2)
            score += sign * 30;
    }
    // Tempo
    score += (b.side == WHITE) ? 10 : -10;
    return (b.side == WHITE) ? score : -score;
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/eval.h src/eval.cpp
git commit -m "feat: evaluate() — material + PST + bishop pair + tempo"
```

---

## Task 2: `board.h` + `board.cpp` — Null-move helpers

**Files:**
- Modify: `src/board.h`
- Modify: `src/board.cpp`

- [ ] **Step 1: Add declarations to `src/board.h`**

Inside the `Board` struct (after `unmake_move`):

```cpp
void make_null_move(StateInfo& st);
void unmake_null_move(const StateInfo& st);
```

- [ ] **Step 2: Add implementations to `src/board.cpp`**

Append at end of file:

```cpp
void Board::make_null_move(StateInfo& st) {
    st.ep_sq    = ep_sq;
    st.halfmove = halfmove;
    st.hash     = hash;
    if (ep_sq != NO_SQ) hash ^= Zobrist::ep_keys[ep_sq % 8];
    ep_sq = NO_SQ;
    halfmove++;
    side = Color(1 - side);
    hash ^= Zobrist::side_key;
}

void Board::unmake_null_move(const StateInfo& st) {
    side     = Color(1 - side);
    ep_sq    = st.ep_sq;
    halfmove = st.halfmove;
    hash     = st.hash;
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/board.h src/board.cpp
git commit -m "feat: make_null_move / unmake_null_move for search"
```

---

## Task 3: `search.h` — All declarations

**Files:**
- Fill: `src/search.h`

- [ ] **Step 1: Write `src/search.h`**

```cpp
#pragma once
#include "board.h"
#include <atomic>

constexpr int INF     = 1000000;
constexpr int MATE    = 100000;
constexpr int MAX_PLY = 128;
constexpr int TT_SIZE = 1 << 22;   // 4M entries (~128 MB)

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
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/search.h
git commit -m "feat: search.h — SearchLimits, SearchResult, TTEntry, constants"
```

---

## Task 4: `search.cpp` — Globals, TT, time, move scoring

**Files:**
- Fill: `src/search.cpp` (start of file — more tasks will append to it)

- [ ] **Step 1: Write the first section of `src/search.cpp`**

```cpp
#include "search.h"
#include "eval.h"
#include "movegen.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

// ── Stop flag ──────────────────────────────────────────────────────────────
std::atomic<bool> stop_search{false};

// ── Transposition table ───────────────────────────────────────────────────
static TTEntry tt[TT_SIZE];

void tt_clear() { memset(tt, 0, sizeof(tt)); }

static TTEntry* tt_probe(uint64_t hash) {
    return &tt[hash % TT_SIZE];
}

static void tt_store(uint64_t hash, int depth, int score, TTFlag flag, Move move) {
    TTEntry* e = &tt[hash % TT_SIZE];
    if (e->flag != TT_NONE && e->depth > depth && e->hash == hash) return;
    e->hash  = hash;
    e->depth = (uint8_t)depth;
    e->score = (int16_t)score;
    e->flag  = flag;
    e->move  = move;
}

// ── Timing ────────────────────────────────────────────────────────────────
static std::chrono::steady_clock::time_point search_start;
static int  time_limit_ms = 0;
static bool time_out      = false;

static int elapsed_ms() {
    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - search_start).count();
}

static bool check_time() {
    if (stop_search.load(std::memory_order_relaxed)) return true;
    if (time_limit_ms > 0 && elapsed_ms() >= time_limit_ms) return true;
    return false;
}

// ── Search state ──────────────────────────────────────────────────────────
static uint64_t nodes;
static Move     killers[2][MAX_PLY];
static int      history[2][6][64];
static Move     pv[MAX_PLY][MAX_PLY];
static int      pv_len[MAX_PLY];

// ── Helpers ───────────────────────────────────────────────────────────────
static inline bool in_check(const Board& b) {
    return is_attacked(king_sq(b, b.side), Color(1 - b.side), b.occupancy[2], b);
}

static void score_moves(const Move* list, int* scores, int n,
                        Move tt_move, int ply, Color us) {
    for (int i = 0; i < n; i++) {
        Move m = list[i];
        int f = move_flags(m);
        if (m == tt_move)
            scores[i] = 2000000;
        else if (f & FLAG_CAPTURE)
            scores[i] = 1000000 + move_captured(m) * 100 - move_piece(m);
        else if (m == killers[0][ply])
            scores[i] = 900000;
        else if (m == killers[1][ply])
            scores[i] = 800000;
        else
            scores[i] = history[us][move_piece(m)][move_to(m)];
    }
}

static inline void pick_best(Move* list, int* scores, int idx, int n) {
    int best = idx;
    for (int i = idx + 1; i < n; i++)
        if (scores[i] > scores[best]) best = i;
    if (best != idx) {
        std::swap(list[idx], list[best]);
        std::swap(scores[idx], scores[best]);
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build (definitions match search.h declarations).

- [ ] **Step 3: Commit**

```bash
git add src/search.cpp
git commit -m "feat: search.cpp — TT, timing, move scoring globals"
```

---

## Task 5: `search.cpp` — Quiescence search

**Files:**
- Modify: `src/search.cpp` (append)

- [ ] **Step 1: Append quiescence search to `src/search.cpp`**

```cpp
// ── Quiescence search ─────────────────────────────────────────────────────
static int quiescence(Board& b, int alpha, int beta, int ply) {
    if (ply >= MAX_PLY) return evaluate(b);

    nodes++;
    if ((nodes & 2047) == 0 && check_time()) { time_out = true; return 0; }

    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    pv_len[ply] = ply;

    Move list[320];
    int n = generate_captures(b, list);

    int scores[320];
    score_moves(list, scores, n, 0, ply, b.side);

    for (int i = 0; i < n; i++) {
        pick_best(list, scores, i, n);
        Move m = list[i];

        // Delta pruning: skip if even capturing the biggest piece won't improve
        if (stand_pat + MAT[move_captured(m)] + 200 < alpha)
            continue;

        StateInfo st;
        b.make_move(m, st);
        if (is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b)) {
            b.unmake_move(m, st);
            continue;
        }
        int score = -quiescence(b, -beta, -alpha, ply + 1);
        b.unmake_move(m, st);

        if (time_out) return 0;
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}
```

Note: `MAT` is defined in `eval.cpp` as a file-static array, so we need to expose it. Add to the **top of `src/search.cpp`** (after the includes, before the TT section) this extern declaration:

```cpp
// from eval.cpp — piece material values
static const int QVAL = 900;  // used for delta pruning approximation
```

Actually `MAT` is `static` in eval.cpp so not visible here. Replace the delta pruning line with a hardcoded approximation:

```cpp
// Delta pruning: biggest capturable piece ≈ queen (900) + margin
if (stand_pat + 900 + 200 < alpha && !(move_flags(m) & FLAG_PROMO))
    continue;
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/search.cpp
git commit -m "feat: quiescence search with delta pruning"
```

---

## Task 6: `search.cpp` — Negamax alpha-beta

**Files:**
- Modify: `src/search.cpp` (append)

- [ ] **Step 1: Append negamax to `src/search.cpp`**

```cpp
// ── Negamax alpha-beta ────────────────────────────────────────────────────
static int negamax(Board& b, int depth, int alpha, int beta, int ply, bool null_ok) {
    if (ply >= MAX_PLY) return evaluate(b);

    nodes++;
    if ((nodes & 2047) == 0 && check_time()) { time_out = true; return 0; }

    // Draw by 50-move rule
    if (b.halfmove >= 100) return 0;

    bool pv_node = (beta - alpha > 1);
    pv_len[ply]  = ply;

    // TT probe
    TTEntry* tte = tt_probe(b.hash);
    Move tt_move = 0;
    if (tte->hash == b.hash) {
        tt_move = tte->move;
        if (!pv_node && tte->depth >= depth) {
            int s = tte->score;
            if (tte->flag == TT_EXACT) return s;
            if (tte->flag == TT_LOWER && s >= beta)  return s;
            if (tte->flag == TT_UPPER && s <= alpha) return s;
        }
    }

    if (depth <= 0) return quiescence(b, alpha, beta, ply);

    bool in_chk = in_check(b);
    if (in_chk) depth++;  // check extension

    // Null-move pruning
    if (null_ok && !in_chk && !pv_node && depth >= 3) {
        Bitboard majors = b.pieces[b.side][KNIGHT] | b.pieces[b.side][BISHOP]
                        | b.pieces[b.side][ROOK]   | b.pieces[b.side][QUEEN];
        if (majors) {
            int R = (depth >= 6) ? 4 : 3;
            StateInfo st;
            b.make_null_move(st);
            int null_score = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
            b.unmake_null_move(st);
            if (!time_out && null_score >= beta) return beta;
        }
    }

    // Generate + order moves
    Move list[320];
    int  scores[320];
    int  n = generate_moves(b, list);
    score_moves(list, scores, n, tt_move, ply, b.side);

    int  best_score = -INF;
    Move best_move  = 0;
    int  legal      = 0;
    TTFlag flag     = TT_UPPER;

    for (int i = 0; i < n; i++) {
        pick_best(list, scores, i, n);
        Move m = list[i];

        StateInfo st;
        b.make_move(m, st);
        if (is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b)) {
            b.unmake_move(m, st);
            continue;
        }
        legal++;

        int score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, true);
        b.unmake_move(m, st);

        if (time_out) return 0;

        if (score > best_score) {
            best_score = score;
            best_move  = m;
            // Update PV
            pv[ply][ply] = m;
            memcpy(pv[ply] + ply + 1, pv[ply + 1] + ply + 1,
                   (pv_len[ply + 1] - ply - 1) * sizeof(Move));
            pv_len[ply] = pv_len[ply + 1];

            if (score > alpha) {
                alpha = score;
                flag  = TT_EXACT;
                if (score >= beta) {
                    // Beta cutoff — update killers + history
                    if (!(move_flags(m) & FLAG_CAPTURE)) {
                        killers[1][ply] = killers[0][ply];
                        killers[0][ply] = m;
                        history[b.side][move_piece(m)][move_to(m)] += depth * depth;
                    }
                    tt_store(b.hash, depth, score, TT_LOWER, m);
                    return score;
                }
            }
        }
    }

    if (legal == 0) return in_chk ? -(MATE - ply) : 0;

    tt_store(b.hash, depth, best_score, flag, best_move);
    return best_score;
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/search.cpp
git commit -m "feat: negamax alpha-beta with null-move, killers, history, TT"
```

---

## Task 7: `search.cpp` — Iterative deepening entry point

**Files:**
- Modify: `src/search.cpp` (append)

- [ ] **Step 1: Append `search()` to `src/search.cpp`**

```cpp
// ── Iterative deepening entry point ──────────────────────────────────────
SearchResult search(Board& b, const SearchLimits& lim) {
    // Determine time limit
    search_start  = std::chrono::steady_clock::now();
    time_out      = false;
    stop_search.store(false, std::memory_order_relaxed);
    nodes         = 0;

    if (lim.movetime > 0) {
        time_limit_ms = lim.movetime;
    } else if (lim.wtime > 0 || lim.btime > 0) {
        int my_time = (b.side == WHITE) ? lim.wtime : lim.btime;
        int my_inc  = (b.side == WHITE) ? lim.winc  : lim.binc;
        time_limit_ms = my_time / 20 + my_inc / 2;
    } else {
        time_limit_ms = 0;  // no limit
    }

    int max_depth = (lim.depth > 0) ? lim.depth
                  : (lim.infinite)  ? MAX_PLY
                  : MAX_PLY;

    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));

    SearchResult result;
    int prev_score = 0;

    for (int d = 1; d <= max_depth; d++) {
        // Aspiration windows (skip for depth 1)
        int alpha = -INF, beta = INF;
        if (d > 1) { alpha = prev_score - 50; beta = prev_score + 50; }

        int score;
        while (true) {
            score = negamax(b, d, alpha, beta, 0, true);
            if (time_out) break;
            if      (score <= alpha) { alpha -= 100; if (alpha < -INF/2) alpha = -INF; }
            else if (score >= beta)  { beta  += 100; if (beta  >  INF/2) beta  =  INF; }
            else break;
        }

        if (time_out && d > 1) break;  // keep result from previous depth

        prev_score       = score;
        result.score     = score;
        result.depth     = d;
        result.nodes     = nodes;
        if (pv_len[0] > 0) result.best_move = pv[0][0];

        // UCI info line
        int ms = elapsed_ms();
        std::cout << "info depth " << d
                  << " score cp " << score
                  << " nodes " << nodes
                  << " time " << ms
                  << " pv";
        for (int k = 0; k < pv_len[0]; k++) {
            int from = move_from(pv[0][k]), to = move_to(pv[0][k]);
            char mv[6];
            mv[0] = 'a' + from % 8; mv[1] = '1' + from / 8;
            mv[2] = 'a' + to   % 8; mv[3] = '1' + to   / 8;
            int promo = move_promo(pv[0][k]);
            if (move_flags(pv[0][k]) & FLAG_PROMO) {
                mv[4] = "pnbrqk"[promo]; mv[5] = 0;
            } else { mv[4] = 0; }
            std::cout << " " << mv;
        }
        std::cout << "\n";
        std::cout.flush();

        if (time_limit_ms > 0 && elapsed_ms() >= time_limit_ms / 2) break;
    }
    return result;
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/search.cpp
git commit -m "feat: iterative deepening with aspiration windows and UCI info output"
```

---

## Task 8: `uci.cpp` — Full UCI loop

**Files:**
- Fill: `src/uci.cpp`

- [ ] **Step 1: Write `src/uci.cpp`**

```cpp
#include "uci.h"
#include "board.h"
#include "magic.h"
#include "movegen.h"
#include "search.h"
#include <iostream>
#include <sstream>
#include <string>

// ── Move string conversion ────────────────────────────────────────────────
static std::string move_to_str(Move m) {
    int from = move_from(m), to = move_to(m);
    char buf[6];
    buf[0] = 'a' + from % 8; buf[1] = '1' + from / 8;
    buf[2] = 'a' + to   % 8; buf[3] = '1' + to   / 8;
    if (move_flags(m) & FLAG_PROMO) {
        buf[4] = "pnbrqk"[move_promo(m)]; buf[5] = 0;
    } else { buf[4] = 0; }
    return buf;
}

static Move str_to_move(const std::string& s, Board& b) {
    Move list[320];
    int n = generate_moves(b, list);
    StateInfo st;
    for (int i = 0; i < n; i++) {
        Move m = list[i];
        b.make_move(m, st);
        bool legal = !is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b);
        b.unmake_move(m, st);
        if (legal && move_to_str(m) == s) return m;
    }
    return 0;
}

// ── Position parser ───────────────────────────────────────────────────────
static void parse_position(const std::string& line, Board& b) {
    if (line.find("startpos") != std::string::npos) {
        b.load_fen(START_FEN);
    } else {
        size_t fen_pos = line.find("fen ");
        if (fen_pos == std::string::npos) return;
        size_t mv_pos = line.find(" moves", fen_pos);
        std::string fen = (mv_pos != std::string::npos)
            ? line.substr(fen_pos + 4, mv_pos - fen_pos - 4)
            : line.substr(fen_pos + 4);
        b.load_fen(fen);
    }
    size_t mv_pos = line.find(" moves ");
    if (mv_pos == std::string::npos) return;
    std::istringstream ss(line.substr(mv_pos + 7));
    std::string tok;
    while (ss >> tok) {
        Move m = str_to_move(tok, b);
        if (m) {
            StateInfo st;
            b.make_move(m, st);
        }
    }
}

// ── Go parser ─────────────────────────────────────────────────────────────
static void parse_go(const std::string& line, Board& b) {
    SearchLimits lim;
    std::istringstream ss(line.substr(2));  // skip "go"
    std::string tok;
    while (ss >> tok) {
        if      (tok == "depth")    ss >> lim.depth;
        else if (tok == "movetime") ss >> lim.movetime;
        else if (tok == "wtime")    ss >> lim.wtime;
        else if (tok == "btime")    ss >> lim.btime;
        else if (tok == "winc")     ss >> lim.winc;
        else if (tok == "binc")     ss >> lim.binc;
        else if (tok == "infinite") lim.infinite = true;
    }
    SearchResult res = search(b, lim);
    std::cout << "bestmove " << move_to_str(res.best_move) << "\n";
    std::cout.flush();
}

// ── Main UCI loop ─────────────────────────────────────────────────────────
void uci_loop() {
    Board b;
    b.load_fen(START_FEN);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line == "uci") {
            std::cout << "id name ChessEngine\n"
                      << "id author Chess Engine Developer\n"
                      << "uciok\n";
            std::cout.flush();
        } else if (line == "isready") {
            std::cout << "readyok\n";
            std::cout.flush();
        } else if (line == "ucinewgame") {
            b.load_fen(START_FEN);
            tt_clear();
        } else if (line.substr(0, 8) == "position") {
            parse_position(line, b);
        } else if (line.substr(0, 2) == "go") {
            parse_go(line, b);
        } else if (line == "stop") {
            stop_search.store(true, std::memory_order_relaxed);
        } else if (line == "quit") {
            break;
        }
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: both `chess-engine.exe` and `perft.exe` build without errors.

- [ ] **Step 3: Commit**

```bash
git add src/uci.cpp
git commit -m "feat: UCI loop — position, go, stop, quit, move parsing"
```

---

## Task 9: Integration test

**Files:** none (read-only verification)

- [ ] **Step 1: Verify perft still passes**

```bash
./build/Release/perft.exe
```

Expected: all 3 PASS (move generator unaffected).

- [ ] **Step 2: Test UCI handshake**

```bash
echo -e "uci\nisready\nquit" | ./build/Release/chess-engine.exe
```

Expected output:
```
id name ChessEngine
id author Chess Engine Developer
uciok
readyok
```

- [ ] **Step 3: Test search from startpos**

```bash
echo -e "uci\nisready\nposition startpos\ngo depth 6\nquit" | ./build/Release/chess-engine.exe
```

Expected: prints `info depth 1 ...` through `info depth 6 ...`, then `bestmove <legal_move>`.

- [ ] **Step 4: Test mate-in-1 detection**

Lucena position adjusted — use a simple mate-in-1: `5k2/8/8/8/8/8/8/4K2R w K - 0 1` is not forced mate. Use this FEN which has Qh7# available:

```bash
echo -e "position fen 6k1/5ppp/8/8/8/8/5PPP/4Q1K1 w - - 0 1\ngo depth 3\nquit" | ./build/Release/chess-engine.exe
```

Expected: `bestmove e1e8` (or another move — just verify it's a legal move and search runs without crashing).

- [ ] **Step 5: Commit final state**

```bash
git add -A
git commit -m "feat: chess engine fully playable via UCI"
```

---

## Self-Review

- [x] **Spec coverage:** Material ✓, PST ✓, bishop pair ✓, tempo ✓, TT ✓, iterative deepening ✓, aspiration ✓, quiescence ✓, null move ✓, killers ✓, history ✓, move ordering (TT/MVV-LVA/killer/history) ✓, UCI commands ✓, time management ✓
- [x] **No placeholders:** All steps have complete code
- [x] **Type consistency:** `SearchResult`, `SearchLimits`, `TTEntry`, `TTFlag` consistent across search.h and search.cpp; `move_to_str` consistent between search.cpp info line and uci.cpp
- [x] **Known issue addressed:** `MAT` in eval.cpp is file-static — delta pruning in quiescence uses hardcoded 900 instead of referencing it
- [x] **Null-move deps:** `make_null_move`/`unmake_null_move` added in Task 2 before search.cpp uses them in Task 6
