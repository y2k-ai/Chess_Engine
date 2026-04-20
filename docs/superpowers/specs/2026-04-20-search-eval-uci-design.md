# Search, Evaluation & UCI Design

**Date:** 2026-04-20  
**Status:** Approved

---

## Overview

Implement a complete, playable UCI chess engine by filling `search.h/cpp`, `eval.h/cpp`, and `uci.h/cpp`. The result is an engine that speaks the UCI protocol, searches with alpha-beta + standard enhancements, and evaluates positions with material + piece-square tables.

---

## Section 1 — Evaluation (`eval.h` / `eval.cpp`)

### Interface
```cpp
int evaluate(const Board& b);  // centipawns, from side-to-move perspective
```

### Material values
```
Pawn=100, Knight=320, Bishop=330, Rook=500, Queen=900, King=0
```

### Piece-square tables (PST)
One 64-entry table per piece type (from white's perspective; mirror by rank for black).  
Standard Chessprogramming Wiki midgame values covering pawn structure incentives (center pawns, advanced pawns), knight centralization, bishop diagonals, rook open-file readiness, queen development, king safety (corners preferred).

### Scoring
```
score = Σ(white material + white PST) − Σ(black material + black PST)
score += tempo_bonus (+10 for side to move)
score += bishop_pair_bonus (+30 if side has 2+ bishops)
return (b.side == WHITE) ? score : -score
```

---

## Section 2 — Transposition Table (`search.h`)

```cpp
enum TTFlag : uint8_t { TT_EXACT, TT_LOWER, TT_UPPER };

struct TTEntry {
    uint64_t hash;
    int16_t  score;
    Move     move;
    uint8_t  depth;
    TTFlag   flag;
};
```

- Size: 1 << 22 entries (≈ 128 MB). Index = `hash % size`.
- `tt_probe(hash, depth, alpha, beta)` → score or `TT_MISS`
- `tt_store(hash, depth, score, flag, move)`
- Cleared on `ucinewgame`.

---

## Section 3 — Search (`search.h` / `search.cpp`)

### Interface
```cpp
struct SearchLimits {
    int  depth;       // max depth (0 = unlimited)
    int  movetime;    // ms per move (0 = not set)
    int  wtime, btime, winc, binc;  // clock-based time
};

struct SearchResult {
    Move     best_move;
    int      score;
    int      depth;
    uint64_t nodes;
};

SearchResult search(Board& b, const SearchLimits& limits);
```

### Algorithm: Iterative Deepening + Alpha-Beta

```
for d = 1 .. max_depth:
    score = aspiration_search(b, d, prev_score)
    if time_up: break
    best_move = pv_move[0]
emit "bestmove"
```

**Negamax alpha-beta (fail-soft):**
```
negamax(b, depth, alpha, beta):
    probe TT → return if hit
    if depth == 0: return quiescence(b, alpha, beta)
    try null move (R=3) if depth >= 3 and not in check
    generate + order moves (TT move, MVV-LVA captures, killers, history)
    for each move:
        make_move; score = -negamax(b, depth-1, -beta, -alpha)
        unmake_move
        update alpha/beta/TT
    store TT; return best_score
```

**Quiescence search:** only captures + promotions; uses `generate_captures`. Delta pruning applied.

**Aspiration windows:** first search full-window; subsequent iterations use [prev±50]; widen to full on fail-low/fail-high.

**Move ordering:**
1. TT/hash move
2. Captures ordered by MVV-LVA (`victim_value - attacker_value / 10`)
3. Killer moves (2 per ply)
4. History heuristic (`history[piece][to]`)

**Null move pruning:** R=3 (adaptive: R=4 if depth >= 6). Skip if in check or no major pieces.

**Killer moves:** `killers[2][MAX_PLY]` — updated on non-capture beta cutoffs.

**History heuristic:** `history[2][6][64]` (color, piece, to) — bonus += depth² on beta cutoff, decayed by dividing by 2 every 1000 nodes.

### Time management
```
if movetime set: use movetime
else: time_per_move = (mytime / 20) + increment / 2
stop searching when elapsed > time_per_move
```

---

## Section 4 — UCI Loop (`uci.h` / `uci.cpp`)

### Commands handled

| Command | Action |
|---|---|
| `uci` | Print `id name ChessEngine`, `id author ...`, `uciok` |
| `isready` | Print `readyok` |
| `ucinewgame` | Clear TT, reset history/killers |
| `position startpos [moves ...]` | Load start FEN, apply moves |
| `position fen <fen> [moves ...]` | Load FEN, apply moves |
| `go [depth N] [movetime N] [wtime N btime N winc N binc N] [infinite]` | Run search, print `bestmove` |
| `stop` | Stop search (set flag) |
| `quit` | Exit |

### Info output (per iteration)
```
info depth N score cp S nodes N time T pv <move1> <move2> ...
```

Moves in pure algebraic notation: `e2e4`, `e7e8q` (promotion).

### Move encoding helper
`move_to_str(Move m)` → `"e2e4"` or `"e7e8q"`  
`str_to_move(const string&, Board&)` → `Move` (scan legal moves for match)

---

## File Map

| File | Role |
|---|---|
| `src/eval.h` | `evaluate()` declaration |
| `src/eval.cpp` | Material + PST implementation |
| `src/search.h` | `SearchLimits`, `SearchResult`, `TTEntry`, `search()` |
| `src/search.cpp` | Iterative deepening, alpha-beta, quiescence, TT, killers, history |
| `src/uci.h` | `uci_loop()` declaration (already exists) |
| `src/uci.cpp` | UCI protocol parser + search integration |

---

## Constants

```cpp
constexpr int INF       = 1000000;
constexpr int MATE      = 100000;   // score for checkmate
constexpr int MAX_PLY   = 128;
constexpr int TT_SIZE   = 1 << 22; // ~128 MB
```

---

## Startup sequence (already in `main.cpp`)
```cpp
Zobrist::init();
Magic::init();
uci_loop();   // TT allocated on first use or at startup
```
