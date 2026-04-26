# Tapered Eval for All Piece Types — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend phase-based PST interpolation and material values to all piece types (pawn, knight, bishop, rook, queen), so the engine uses different positional and material values in the middlegame vs endgame.

**Architecture:** All changes are in `src/eval.cpp`. Add `MAT_EG[6]`, five EG PST tables, and a `PST_EG[6]` pointer array at file scope. Replace the king-special-cased interpolation in the material loop with a uniform interpolation for all pieces. `PST_KING_EG` already exists and slots into `PST_EG[5]` — no change to king values.

**Tech Stack:** C++17, `src/eval.cpp` only. Build: `cmake --build build --config Release`. Binaries: `build/Release/chess-engine.exe`, `build/Release/perft.exe`.

---

## File Map

| File | Action | What changes |
|---|---|---|
| `src/eval.cpp` | Modify | Add `MAT_EG`, 5 EG PST tables, `PST_EG` pointer array; generalize loop interpolation |

---

## Task 1: Add EG constants to `src/eval.cpp`

**Files:**
- Modify: `src/eval.cpp` (after line 83 — after the `PST[6]` static_assert)

### Steps

- [ ] **Step 1: Read `src/eval.cpp` lines 1–84 to confirm the insertion point**

Read `src/eval.cpp`. Confirm:
1. `PST_KING_EG[64]` exists at lines ~69–78.
2. `PST[6]` pointer array ends at line 82.
3. The `static_assert` is at line 83.
4. `MAT[6] = { 100, 320, 330, 500, 900, 0 }` is at line 4.

The new constants are inserted immediately after line 83 (the `static_assert`).

- [ ] **Step 2: Insert `MAT_EG`, five EG PST tables, and `PST_EG` pointer array**

Find:
```cpp
static const int* PST[6] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING
};
static_assert(sizeof(PST)/sizeof(PST[0]) == NO_PIECE, "PST size mismatch");
```

Replace with:
```cpp
static const int* PST[6] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING
};
static_assert(sizeof(PST)/sizeof(PST[0]) == NO_PIECE, "PST size mismatch");

// Endgame material values (CPW tapered)
static const int MAT_EG[6] = { 94, 281, 297, 512, 936, 0 };

// Endgame PST tables — same index convention as MG tables (A1=0, white perspective)
static const int PST_PAWN_EG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    -6, -4,  1,  1,  1,  1, -4, -6,
    -6, -4,  1,  5,  5,  1, -4, -6,
    -6, -4,  5, 10, 10,  5, -4, -6,
    -3,  2, 12, 16, 16, 12,  2, -3,
     8, 14, 20, 32, 32, 20, 14,  8,
    20, 25, 35, 45, 45, 35, 25, 20,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PST_KNIGHT_EG[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int PST_BISHOP_EG[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int PST_ROOK_EG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0,
};
static const int PST_QUEEN_EG[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};

static const int* PST_EG[6] = {
    PST_PAWN_EG, PST_KNIGHT_EG, PST_BISHOP_EG,
    PST_ROOK_EG, PST_QUEEN_EG, PST_KING_EG
};
static_assert(sizeof(PST_EG)/sizeof(PST_EG[0]) == NO_PIECE, "PST_EG size mismatch");
```

- [ ] **Step 3: Build to confirm constants compile cleanly**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build, no errors or warnings.

---

## Task 2: Generalize the material+PST loop interpolation

**Files:**
- Modify: `src/eval.cpp` (lines ~102–106 inside `evaluate()`)

### Steps

- [ ] **Step 1: Read `src/eval.cpp` lines 95–110 to confirm the exact block**

Read `src/eval.cpp`. Find this block inside `evaluate()`:

```cpp
                int pst_val = (p == KING)
                    ? (PST_KING[pst_sq] * phase + PST_KING_EG[pst_sq] * (24 - phase)) / 24
                    : PST[p][pst_sq];
                score += sign * (MAT[p] + pst_val);
```

Note the exact line numbers.

- [ ] **Step 2: Replace the king-special-cased block with uniform interpolation**

Find:
```cpp
                int pst_val = (p == KING)
                    ? (PST_KING[pst_sq] * phase + PST_KING_EG[pst_sq] * (24 - phase)) / 24
                    : PST[p][pst_sq];
                score += sign * (MAT[p] + pst_val);
```

Replace with:
```cpp
                int pst_val = (PST[p][pst_sq] * phase + PST_EG[p][pst_sq] * (24 - phase)) / 24;
                int mat_val = (MAT[p]         * phase + MAT_EG[p]          * (24 - phase)) / 24;
                score += sign * (mat_val + pst_val);
```

Note: `PST[5]` = `PST_KING` and `PST_EG[5]` = `PST_KING_EG`, so king interpolation is unchanged in behaviour — it just uses the general path now.

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build, no errors.

- [ ] **Step 4: Perft regression**

```bash
./build/Release/perft.exe
```

Expected: all three positions PASS — eval changes cannot affect move generation:
- startpos depth 5 = 4865609
- kiwipete depth 4 = 4085603
- pos3 depth 6 = 11030083

- [ ] **Step 5: Sanity — rook more valuable in endgame**

Compare rook evaluation at full material (MG) vs no material (EG). Both positions are white to move, white has one rook, black has matching material.

```bash
# Endgame: white rook vs black rook, no other pieces (phase ≈ 4, two rooks)
printf "uci\nisready\nposition fen 4k3/8/8/8/8/8/8/4KR2 w - - 0 1\ngo depth 1\nquit\n" | ./build/Release/chess-engine.exe
```

Expected: score near 0 (symmetric) but no crash. The important thing is it completes and outputs `bestmove`.

- [ ] **Step 6: Sanity — advanced pawn worth more in endgame**

```bash
# Endgame pawn on e5 (rank 5, rel_rank=4 for white) — only kings and this pawn
printf "uci\nisready\nposition fen 4k3/8/8/4P3/8/8/8/4K3 w - - 0 1\ngo depth 1\nquit\n" | ./build/Release/chess-engine.exe

# Same but pawn on e2 (rank 2, rel_rank=1)
printf "uci\nisready\nposition fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1\ngo depth 1\nquit\n" | ./build/Release/chess-engine.exe
```

Expected: e5 pawn scores noticeably higher. With EG at phase≈0, pawn EG PST applies: rank-4 square gets +12 bonus vs rank-1 square getting +1. Combined with MAT_EG=94 vs MAT=100 blend and PASSED_BONUS already applied in pawn structure block, the e5 position should score at least 20 cp higher.

- [ ] **Step 7: Regression — mate score unaffected**

```bash
printf "uci\nisready\nposition fen k7/2R5/1K6/8/8/8/8/8 w - - 0 1\ngo depth 5\nquit\n" | ./build/Release/chess-engine.exe
```

Expected: all depth lines show `score mate 1` and `pv c7c8`. No crash.

- [ ] **Step 8: Commit**

```bash
git add src/eval.cpp
git commit -m "feat: tapered eval — interpolate PST and material for all piece types"
```

---

## Self-Review

**Spec coverage:**
- ✅ `MAT_EG[6] = { 94, 281, 297, 512, 936, 0 }` — CPW standard endgame material
- ✅ `PST_PAWN_EG`, `PST_KNIGHT_EG`, `PST_BISHOP_EG`, `PST_ROOK_EG`, `PST_QUEEN_EG` — CPW-style EG tables
- ✅ `PST_EG[6]` pointer array with `static_assert` for size safety
- ✅ General interpolation: `(MG * phase + EG * (24 - phase)) / 24` for both PST and material
- ✅ King still interpolates correctly via `PST[5]`/`PST_EG[5]` — no behavioural change
- ✅ Phase already computed at top of `evaluate()` — no duplication
- ✅ Only `src/eval.cpp` changes

**Placeholder scan:** No TBDs. All code complete, all commands concrete.

**Type consistency:**
- `MAT_EG` is `int[6]` — same type as `MAT[6]`. Index `p` is 0–5 (PAWN to KING), all valid.
- `PST_EG` is `const int*[6]` — same type as `PST[6]`. Both indexed by `p` and `pst_sq`.
- `pst_val` and `mat_val` are `int` — same as before. Integer division `/24` is intentional (at most 1 cp lost per piece per call).
- `PST_EG[5]` = `PST_KING_EG` — array declared at file scope before `PST_EG` is defined. Valid C++.
- `static_assert` on `PST_EG` uses `NO_PIECE` (= 6) — same as `PST` assert. Both arrays have 6 entries.
