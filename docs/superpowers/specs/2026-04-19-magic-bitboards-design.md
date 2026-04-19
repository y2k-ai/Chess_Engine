# Magic Bitboards & Move Generator Design

**Date:** 2026-04-19  
**Status:** Approved  

---

## Overview

Implement sliding piece attack tables using PEXT (BMI2), full pseudo-legal move generation for all pieces, and perft verification. Fills the empty stubs `magic.h/cpp`, `movegen.h/cpp`, and `tests/perft.cpp`.

---

## Section 1 — Attack Tables (`magic.h` / `magic.cpp`)

### Data layout

```cpp
// magic.h
namespace Magic {
    extern Bitboard bishop_masks[64];
    extern Bitboard rook_masks[64];
    extern Bitboard bishop_table[64][512];   // 256 KB
    extern Bitboard rook_table[64][4096];    // 2 MB
    extern Bitboard knight_attacks[64];
    extern Bitboard king_attacks[64];

    void init();

    inline Bitboard bishop_attacks(int sq, Bitboard occ);
    inline Bitboard rook_attacks(int sq, Bitboard occ);
    inline Bitboard queen_attacks(int sq, Bitboard occ);
}
```

### Initialization (`Magic::init()`)

For each square:
1. Build `bishop_masks[sq]` by walking all four diagonals, stopping **one before** the board edge (edge squares never affect blocking).
2. Build `rook_masks[sq]` by walking all four ranks/files, stopping one before the edge.
3. Enumerate every subset of the mask using the carry-rippler trick:
   ```cpp
   Bitboard s = 0;
   do {
       int idx = _pext_u64(s, mask);
       table[sq][idx] = compute_attacks_slow(sq, s, directions);
       s = (s - mask) & mask;
   } while (s);
   ```
4. `compute_attacks_slow` walks each ray direction until it hits an occupied square (inclusive) or the board edge — used only during init.

Knight and king attack tables are pre-computed with bitmask shifts (no PEXT needed).

### Lookup (inlined in header)

```cpp
inline Bitboard bishop_attacks(int sq, Bitboard occ) {
    return bishop_table[sq][_pext_u64(occ, bishop_masks[sq])];
}
inline Bitboard rook_attacks(int sq, Bitboard occ) {
    return rook_table[sq][_pext_u64(occ, rook_masks[sq])];
}
inline Bitboard queen_attacks(int sq, Bitboard occ) {
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}
```

### PEXT requirement

Requires BMI2 (`immintrin.h`, `_pext_u64`). On MSVC: compile with `/arch:AVX2` or the intrinsic is available unconditionally on x64 with `#include <immintrin.h>`. CMakeLists.txt must be updated to add `/arch:AVX2` (MSVC) or `-mbmi2` (GCC/Clang).

`Magic::init()` must be called once at startup, after `Zobrist::init()`.

---

## Section 2 — Move Generator (`movegen.h` / `movegen.cpp`)

### Interface

```cpp
// movegen.h
int generate_moves(const Board& b, Move* list);
int generate_captures(const Board& b, Move* list);
bool is_attacked(int sq, Color by, Bitboard occ, const Board& b);
int  king_sq(const Board& b, Color c);
```

`generate_moves` writes pseudo-legal moves into caller-provided `list` (256 entries always sufficient) and returns count. `generate_captures` writes only capture moves (for future quiescence search).

### Per-piece generation

**Pawns (most complex):**
- Single push: shift ±8, mask by empty squares
- Double push: from rank 2 (white) / rank 7 (black), both target squares empty
- Diagonal captures: two shift patterns, mask by enemy occupancy
- En passant: if `b.ep_sq != NO_SQ`, check pawn can reach it
- Promotions: when reaching rank 8 (white) / rank 1 (black), emit four moves (queen, rook, bishop, knight)

**Knights:** `Magic::knight_attacks[sq] & ~us_occ`

**Bishops:** `Magic::bishop_attacks(sq, occ) & ~us_occ`

**Rooks:** `Magic::rook_attacks(sq, occ) & ~us_occ`

**Queens:** `Magic::queen_attacks(sq, occ) & ~us_occ`

**Kings:** `Magic::king_attacks[sq] & ~us_occ`

**Castling:**
- Check `b.castle_rights` flag for the side
- All squares between king and rook must be empty
- King's current square, transit square, and destination must not be attacked by the opponent
- Uses `is_attacked()` for the three king squares

### Legality model

Output is **pseudo-legal**: friendly-piece captures are filtered, but the king may move into check. The search layer filters fully illegal moves by calling `is_attacked(king_sq, them, occ)` after `make_move` and before counting the node.

### `is_attacked(sq, by, occ, b)`

Reverse-lookup pattern — shoot rays from `sq` as if it were each piece type, check if any piece of color `by` of that type sits on the ray:

```
is_attacked = rook_attacks(sq,occ)   & (rook|queen)[by]
            | bishop_attacks(sq,occ) & (bishop|queen)[by]
            | knight_attacks[sq]     & knight[by]
            | king_attacks[sq]       & king[by]
            | pawn_attacks[us][sq]   & pawn[by]
```

---

## Section 3 — Perft Verification (`tests/perft.cpp`)

### Algorithm

```cpp
uint64_t perft(Board& b, int depth) {
    if (depth == 0) return 1;
    Move list[256];
    int n = generate_moves(b, list);
    uint64_t nodes = 0;
    StateInfo st;
    for (int i = 0; i < n; i++) {
        b.make_move(list[i], st);
        if (!is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b))
            nodes += perft(b, depth - 1);
        b.unmake_move(list[i], st);
    }
    return nodes;
}
```

### Test suite

| Position | Depth | Expected nodes |
|---|---|---|
| Start position | 5 | 4,865,609 |
| Kiwipete (`r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -`) | 4 | 4,085,603 |
| Position 3 (`8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -`) | 6 | 11,030,083 |

The harness prints per-depth node counts and flags any mismatch. A wrong count at depth N narrows the bug to a specific move type.

---

## Build changes

`CMakeLists.txt` needs:
- MSVC: add `/arch:AVX2` to enable `_pext_u64`
- GCC/Clang: add `-mbmi2`

---

## Startup sequence

```cpp
// main.cpp
Zobrist::init();
Magic::init();
uci_loop();
```
