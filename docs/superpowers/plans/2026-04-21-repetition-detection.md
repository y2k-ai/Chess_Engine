# Repetition Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Return score 0 (draw) whenever the current position has appeared before in the game or search history, preventing the engine from walking into or ignoring 2-fold repetitions.

**Architecture:** A `hash_history` array in `search.cpp` stores Zobrist hashes of every position seen since the last irreversible move. `parse_position` in `uci.cpp` seeds it with the game history before search starts; `negamax` pushes/pops hashes around each recursive call and checks for repetition at entry. Two helper functions (`rep_push`, `rep_clear`) are exported so `uci.cpp` can populate the array without touching the internals.

**Tech Stack:** C++17, existing Zobrist hash infrastructure (`Board::hash`, `b.halfmove`).

---

## File Map

| File | Action | What changes |
|---|---|---|
| `src/search.h` | Modify | Add `rep_push` and `rep_clear` declarations |
| `src/search.cpp` | Modify | Add `hash_history` globals; implement `rep_push`/`rep_clear`; add push/pop + repetition check in `negamax` |
| `src/uci.cpp` | Modify | Seed `hash_history` from `parse_position`; clear on `ucinewgame` |

---

## Task 1: `search.h` + `search.cpp` — hash history globals and helpers

**Files:**
- Modify: `src/search.h`
- Modify: `src/search.cpp`

- [ ] **Step 1: Add declarations to `src/search.h`**

After the existing `void tt_clear();` line, add:

```cpp
void rep_push(uint64_t hash);
void rep_clear();
```

The full tail of `search.h` should now look like:

```cpp
extern std::atomic<bool> stop_search;

SearchResult search(Board& b, const SearchLimits& limits);
void         tt_clear();
void         rep_push(uint64_t hash);
void         rep_clear();
```

- [ ] **Step 2: Add `hash_history` globals to `src/search.cpp`**

In the `// ── Search state ──` section (currently lines 47–52), append two lines so the block reads:

```cpp
// ── Search state ──────────────────────────────────────────────────────────
static uint64_t nodes;
static Move     killers[2][MAX_PLY];
static int      history[2][6][64];
static Move     pv[MAX_PLY][MAX_PLY];
static int      pv_len[MAX_PLY];
static uint64_t hash_history[1024];
static int      hash_history_len = 0;
```

- [ ] **Step 3: Implement `rep_push` and `rep_clear` in `src/search.cpp`**

Append immediately after the `pick_best` function (before the `// ── Quiescence search` comment):

```cpp
// ── Repetition history ────────────────────────────────────────────────────
void rep_push(uint64_t hash) {
    if (hash_history_len < 1024) hash_history[hash_history_len++] = hash;
}

void rep_clear() { hash_history_len = 0; }
```

- [ ] **Step 4: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/search.h src/search.cpp
git commit -m "feat: hash_history globals and rep_push/rep_clear helpers"
```

---

## Task 2: `search.cpp` — repetition check + push/pop in negamax

**Files:**
- Modify: `src/search.cpp`

The `negamax` function currently starts at line 131. Two changes are needed:
1. **Repetition check** — at entry, before the TT probe, detect if `b.hash` appears earlier in the history.
2. **Push/pop** — around the recursive `negamax` call, push the child position's hash and pop it afterward.

- [ ] **Step 1: Add the repetition check inside `negamax`**

Find this block near the top of `negamax` (after `if (b.halfmove >= 100) return 0;`):

```cpp
    if (b.halfmove >= 100) return 0;

    bool pv_node = (beta - alpha > 1);
```

Replace it with:

```cpp
    if (b.halfmove >= 100) return 0;

    // Repetition detection: scan back same-side positions bounded by halfmove clock
    {
        int start = hash_history_len - 3;
        int bound = hash_history_len - 1 - b.halfmove;
        for (int i = start; i >= bound && i >= 0; i -= 2)
            if (hash_history[i] == b.hash) return 0;
    }

    bool pv_node = (beta - alpha > 1);
```

**Why `len - 3`:** `hash_history[len-1]` is the current position (pushed by the caller); `len-2` is the opponent's last position; `len-3` is the first candidate for same-side repetition.

**Why `i -= 2`:** Only same-side positions share the same Zobrist side key; odd-offset positions have the opponent to move and can never match.

**Why `bound = len - 1 - halfmove`:** A capture or pawn push resets the halfmove clock and makes earlier positions unreachable by repetition.

- [ ] **Step 2: Add push/pop around the recursive call**

Find the recursive call inside the move loop:

```cpp
        int score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, true);
        b.unmake_move(m, st);
```

Replace with:

```cpp
        hash_history[hash_history_len++] = b.hash;
        int score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, true);
        hash_history_len--;
        b.unmake_move(m, st);
```

Note: `b.hash` here is the hash **after** `make_move` (the child position), which is what the child's repetition check will compare against.

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/search.cpp
git commit -m "feat: repetition detection in negamax — push/pop + 2-fold draw check"
```

---

## Task 3: `uci.cpp` — seed history from game moves

**Files:**
- Modify: `src/uci.cpp`

`parse_position` must push hashes for every position in the game history before search starts. `ucinewgame` must clear the history. Without this, the engine only detects repetitions created inside the current search, not repetitions of positions already on the board.

- [ ] **Step 1: Update `parse_position` to seed `hash_history`**

Find the full `parse_position` function:

```cpp
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
```

Replace it with:

```cpp
static void parse_position(const std::string& line, Board& b) {
    rep_clear();
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
    rep_push(b.hash);  // push the root position
    size_t mv_pos = line.find(" moves ");
    if (mv_pos == std::string::npos) return;
    std::istringstream ss(line.substr(mv_pos + 7));
    std::string tok;
    while (ss >> tok) {
        Move m = str_to_move(tok, b);
        if (m) {
            StateInfo st;
            b.make_move(m, st);
            rep_push(b.hash);  // push each position after a game move
        }
    }
}
```

- [ ] **Step 2: Add `rep_clear()` to `ucinewgame` handling**

Find:

```cpp
        } else if (line == "ucinewgame") {
            b.load_fen(START_FEN);
            tt_clear();
```

Replace with:

```cpp
        } else if (line == "ucinewgame") {
            b.load_fen(START_FEN);
            tt_clear();
            rep_clear();
```

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build, both targets link.

- [ ] **Step 4: Commit**

```bash
git add src/uci.cpp
git commit -m "feat: seed repetition history from game moves in parse_position"
```

---

## Task 4: Verify

**Files:** none (read-only verification)

- [ ] **Step 1: Perft still passes**

```bash
./build/Release/perft.exe
```

Expected: all 3 PASS — move generator is unaffected.

- [ ] **Step 2: Basic UCI search still works**

```bash
printf "uci\nisready\nposition startpos\ngo depth 5\nquit\n" | ./build/Release/chess-engine.exe
```

Expected: `info depth 1..5` lines followed by `bestmove <legal move>`.

- [ ] **Step 3: Repetition draw is detected**

This sequence replays 4 moves that bring the position back to its start (2-fold repetition), then searches. A position where the engine has already been should score 0 (draw) at the root, and the engine should choose a different path.

```bash
printf "uci\nisready\nposition startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8\ngo depth 4\nquit\n" | ./build/Release/chess-engine.exe
```

After `e2e4 e7e5 e1e2 e7e7 e2e1 e7e8` the kings are back to their starting squares but pawns have moved — so this is NOT a full repetition. Use a king-only endgame instead:

```bash
printf "uci\nisready\nposition fen 8/8/8/8/8/8/8/4K2k w - - 0 1 moves e1f1 h1g1 f1e1 g1h1\ngo depth 4\nquit\n" | ./build/Release/chess-engine.exe
```

After `e1f1 h1g1 f1e1 g1h1` the position is identical to the FEN root (hash_history has 5 entries: root + 4 moves). The engine is searching from a position that has appeared before — it should NOT repeat again (bestmove should not be `e1f1` which would head toward another repetition).

Expected: `bestmove` is any legal king move; no crash; search completes.

- [ ] **Step 4: Final commit if any loose files**

```bash
git status
```

If clean: no commit needed. If anything unstaged:

```bash
git add -A
git commit -m "chore: post-repetition-detection cleanup"
```

---

## Self-Review

**Spec coverage:**
- ✅ `hash_history` array in `search.cpp` globals
- ✅ `rep_push` / `rep_clear` exported from `search.h`
- ✅ Push/pop around recursive `negamax` call
- ✅ Repetition check at entry to `negamax` (step-2, halfmove-bounded)
- ✅ `parse_position` seeds history (root + each game move)
- ✅ `ucinewgame` clears history

**Placeholder scan:** No TBDs, all code is complete.

**Type consistency:** `uint64_t hash` throughout; `hash_history_len` is `int` used consistently in signed comparisons; `rep_push`/`rep_clear` signatures match between `search.h` and `search.cpp`.

**Edge cases handled:**
- Array bounds: `rep_push` guards against overflow (`< 1024`)
- `hash_history_len - 3 < 0`: inner loop exits immediately when `start < 0 || start < bound`
- Empty move list in `parse_position`: `rep_push` for root still called; loop simply doesn't execute
