# Tapered Eval for All Piece Types — Design

**Date:** 2026-04-26  
**Status:** Approved  

---

## Goal

Extend the existing king PST interpolation to all piece types (pawn, knight, bishop, rook, queen) and also taper material values between middlegame and endgame. This gives the engine phase-aware positional and material evaluation across the full game.

---

## Background

Phase is already computed at the top of `evaluate()`:

```
phase = sum of (PHASE_W[p] * piece_count), clamped to [0, 24]
  Q=4, R=2, B=1, N=1
  Full material = 24 (middlegame)
  No material   =  0 (endgame)
```

King PST is already interpolated:
```cpp
(PST_KING[sq] * phase + PST_KING_EG[sq] * (24 - phase)) / 24
```

All other pieces currently use a single MG table and a single material value.

---

## Architecture

Only `src/eval.cpp` changes.

### New constants (file scope)

**Endgame material values** — CPW standard:
```
MAT_EG[6] = { 94, 281, 297, 512, 936, 0 }
             P    N    B    R    Q    K
```
Compare to MG: `MAT[6] = { 100, 320, 330, 500, 900, 0 }`

Notable: rooks and knights gain value as material thins; queens and pawns/bishops slightly less dominant in endgame.

**Endgame PST tables** — CPW standard, same index convention (A1=0, white perspective):
- `PST_PAWN_EG[64]` — rewards advanced pawns heavily; central pawns +10 to +20
- `PST_KNIGHT_EG[64]` — similar to MG but slightly more central; corner penalty less severe
- `PST_BISHOP_EG[64]` — long diagonals rewarded; center slightly better
- `PST_ROOK_EG[64]` — 7th rank bonus; open files less critical than mobility already covers
- `PST_QUEEN_EG[64]` — slightly centralized; similar to MG
- `PST_KING_EG[64]` — already exists, unchanged

**Pointer array:**
```cpp
static const int* PST_EG[6] = {
    PST_PAWN_EG, PST_KNIGHT_EG, PST_BISHOP_EG,
    PST_ROOK_EG, PST_QUEEN_EG, PST_KING_EG
};
```

`PST[5]` already points to `PST_KING`; `PST_EG[5]` points to `PST_KING_EG`. No change to king logic — it now falls into the general path.

### Loop change

Replace the king-special-cased interpolation with a uniform interpolation for all pieces:

```cpp
// Before:
int pst_val = (p == KING)
    ? (PST_KING[pst_sq] * phase + PST_KING_EG[pst_sq] * (24 - phase)) / 24
    : PST[p][pst_sq];
score += sign * (MAT[p] + pst_val);

// After:
int pst_val = (PST[p][pst_sq] * phase + PST_EG[p][pst_sq] * (24 - phase)) / 24;
int mat_val = (MAT[p]         * phase + MAT_EG[p]          * (24 - phase)) / 24;
score += sign * (mat_val + pst_val);
```

### What does NOT change

- Phase computation
- Pawn structure block
- Mobility block
- Rook open/semi-open file block
- King safety block
- All other files

---

## Files Changed

| File | Action |
|---|---|
| `src/eval.cpp` | Add `MAT_EG`, 5 EG PST tables, `PST_EG` pointer array; generalize loop |

---

## Testing

1. Build — clean, no errors
2. Perft regression — all 3 positions unchanged (eval changes cannot affect move generation)
3. Sanity — endgame king centralizes, rook more valuable with less material
4. Regression — mate score still detected (k7/2R5/1K6 depth 5)

---

## Expected Elo gain

+15–30 Elo based on typical results from adding tapered eval to engines at this level.
