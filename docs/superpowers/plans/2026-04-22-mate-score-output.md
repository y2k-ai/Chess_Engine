# Mate Score UCI Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Print `score mate N` instead of `score cp X` in the UCI info line whenever the search score indicates a forced checkmate.

**Architecture:** The `negamax` function already returns mate scores encoded as `±(MATE - ply)` (e.g. MATE=100000, mating in 1 move → score 99999 at root). The fix is purely in the UCI info print loop inside `search()` in `src/search.cpp` — detect the mate score, convert ply distance to move count, and emit the correct UCI token. No other files change.

**Tech Stack:** C++17, existing constants `MATE=100000` and `MAX_PLY=128` from `src/search.h`.

---

## File Map

| File | Action | What changes |
|---|---|---|
| `src/search.cpp` | Modify | Replace `" score cp " << score` with a branch that emits `score mate N` or `score cp X` |

---

## Task 1: Replace score output in UCI info line

**Files:**
- Modify: `src/search.cpp` (the `search()` function, info print block)

### Background: how mate scores work in this engine

`negamax` returns checkmate as `-(MATE - ply)` from the mated side's perspective.  
At the root (ply 0), a score of:
- `+99999` = engine mates opponent in **1 move** (1 ply away)
- `+99997` = engine mates opponent in **2 moves** (3 plies: engine-opp-engine)
- `+99995` = engine mates opponent in **3 moves** (5 plies)
- `-99999` = engine is mated in 1 move
- `-99997` = engine is mated in 2 moves

**Detection threshold:** `abs(score) > MATE - MAX_PLY` → `abs(score) > 99872`.  
No static evaluation can reach this (max material + PST is far below), so this reliably identifies mate scores.

**Move count formula:** `moves = (MATE - abs(score) + 1) / 2`  
Verification:
- abs=99999 → (100000-99999+1)/2 = 1 ✓  
- abs=99997 → (100000-99997+1)/2 = 2 ✓  
- abs=99995 → (100000-99995+1)/2 = 3 ✓

**Sign:** positive if engine is winning (`score > 0`), negative if engine is being mated.

### Steps

- [ ] **Step 1: Read `src/search.cpp` to confirm the exact lines**

Read `src/search.cpp`. Find the block that starts with:

```cpp
        std::cout << "info depth " << d
                  << " score cp " << score
                  << " nodes " << nodes
                  << " time " << ms
```

Note the exact line numbers — you'll edit these lines.

- [ ] **Step 2: Replace the score output block**

Find:

```cpp
        std::cout << "info depth " << d
                  << " score cp " << score
                  << " nodes " << nodes
                  << " time " << ms
```

Replace with:

```cpp
        std::cout << "info depth " << d;
        if (std::abs(score) > MATE - MAX_PLY) {
            int moves = (MATE - std::abs(score) + 1) / 2;
            std::cout << " score mate " << (score > 0 ? moves : -moves);
        } else {
            std::cout << " score cp " << score;
        }
        std::cout << " nodes " << nodes
                  << " time " << ms
```

Note: `std::abs` for `int` requires `<cstdlib>` or `<cmath>` — check the existing includes at the top of `search.cpp`. The file already includes `<algorithm>`, which on MSVC provides `std::abs` for integers. If the build fails with "abs not found", add `#include <cstdlib>` to the includes.

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build, no new errors or warnings.

- [ ] **Step 4: Test — normal position (no mate)**

```bash
printf "uci\nisready\nposition startpos\ngo depth 5\nquit\n" | ./build/Release/chess-engine.exe
```

Expected: info lines say `score cp <number>` (not `score mate`). Bestmove is a legal first move.

- [ ] **Step 5: Test — forced mate position**

Use a known mate-in-1 position. FEN: `4k3/8/8/8/8/8/8/4K2R w K - 0 1`  
White plays Rh8# (rook to h8 = checkmate). Square h8 = h-file (index 7) rank 8 (index 7) = square 63.

```bash
printf "uci\nisready\nposition fen 4k3/8/8/8/8/8/8/4K2R w K - 0 1\ngo depth 3\nquit\n" | ./build/Release/chess-engine.exe
```

Expected output contains: `score mate 1` (at depth 1 or deeper) and `bestmove h1h8`.

- [ ] **Step 6: Test — engine being mated**

FEN: `4k2r/8/8/8/8/8/8/4K3 b k - 0 1`  
Black plays Rh1# (Black has the rook, can mate White). From White's perspective the score should be negative mate.

```bash
printf "uci\nisready\nposition fen 4k2r/8/8/8/8/8/8/4K3 b k - 0 1\ngo depth 3\nquit\n" | ./build/Release/chess-engine.exe
```

Expected output contains: `score mate 1` (Black is winning, from Black's perspective it's a positive mate score). `bestmove h8h1`.

- [ ] **Step 7: Commit**

```bash
git add src/search.cpp
git commit -m "feat: emit 'score mate N' in UCI info when checkmate is detected"
```

---

## Self-Review

**Spec coverage:**
- ✅ Detect mate score: `abs(score) > MATE - MAX_PLY`
- ✅ Convert to moves: `(MATE - abs(score) + 1) / 2`
- ✅ Sign: positive for engine winning, negative for engine losing
- ✅ Falls back to `score cp` for non-mate scores
- ✅ Only touches `search.cpp` info print — no other files

**Placeholder scan:** No TBDs. All code complete, all test commands concrete.

**Type consistency:** `MATE` and `MAX_PLY` are `constexpr int` from `search.h` — used directly in integer arithmetic, no cast needed. `std::abs(score)` where `score` is `int` — unambiguous with `<algorithm>` already included on MSVC.
