# Magic Bitboards & Move Generator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement PEXT-based sliding piece attack tables, full pseudo-legal move generation for all piece types, and perft verification against known node counts.

**Architecture:** `Magic::init()` builds bishop and rook attack tables using BMI2 `_pext_u64`; lookups are O(1) inlined calls. `generate_moves()` emits pseudo-legal moves (friendly captures filtered; king may walk into check). The search layer filters illegal moves by checking whether the king is in check after `make_move`.

**Tech Stack:** C++17, MSVC with `/arch:AVX2` (BMI2), `_pext_u64` from `<immintrin.h>`.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `CMakeLists.txt` | Modify | Add `/arch:AVX2` (MSVC) / `-mbmi2` (GCC) |
| `src/magic.h` | Fill stub | Table declarations + inline attack lookups |
| `src/magic.cpp` | Fill stub | `Magic::init()` — masks, carry-rippler fill, knight/king/pawn tables |
| `src/movegen.h` | Fill stub | `generate_moves`, `generate_captures`, `is_attacked`, `king_sq` |
| `src/movegen.cpp` | Fill stub | All piece generation logic |
| `tests/perft.cpp` | Fill stub | `perft()` + test harness with 3 known positions |
| `src/main.cpp` | Modify | Add `Magic::init()` call before `uci_loop()` |

---

## Task 1: Add BMI2 build flag

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the compiler flags**

Replace the existing MSVC/else block in `CMakeLists.txt`:

```cmake
if(MSVC)
    add_compile_options(/W4 /arch:AVX2)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /O2")
else()
    add_compile_options(-O3 -mbmi2 -Wall -Wextra)
endif()
```

- [ ] **Step 2: Verify CMake configures without error**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Expected: no errors, Makefile/solution generated.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add BMI2 flag for PEXT support"
```

---

## Task 2: Write `magic.h` — declarations and inline lookups

**Files:**
- Fill: `src/magic.h`

- [ ] **Step 1: Write the header**

Replace the empty `src/magic.h` with:

```cpp
#pragma once
#include "types.h"
#include <immintrin.h>

namespace Magic {
    extern Bitboard bishop_masks[64];
    extern Bitboard rook_masks[64];
    extern Bitboard bishop_table[64][512];
    extern Bitboard rook_table[64][4096];
    extern Bitboard knight_attacks[64];
    extern Bitboard king_attacks[64];
    extern Bitboard pawn_attacks[2][64];

    void init();

    inline Bitboard bishop_attacks(int sq, Bitboard occ) {
        return bishop_table[sq][_pext_u64(occ, bishop_masks[sq])];
    }
    inline Bitboard rook_attacks(int sq, Bitboard occ) {
        return rook_table[sq][_pext_u64(occ, rook_masks[sq])];
    }
    inline Bitboard queen_attacks(int sq, Bitboard occ) {
        return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add src/magic.h
git commit -m "feat: magic.h declarations and inline PEXT lookups"
```

---

## Task 3: Write `magic.cpp` — `Magic::init()`

**Files:**
- Fill: `src/magic.cpp`

- [ ] **Step 1: Write the full init implementation**

Replace the empty `src/magic.cpp` with:

```cpp
#include "magic.h"

namespace Magic {
    Bitboard bishop_masks[64];
    Bitboard rook_masks[64];
    Bitboard bishop_table[64][512];
    Bitboard rook_table[64][4096];
    Bitboard knight_attacks[64];
    Bitboard king_attacks[64];
    Bitboard pawn_attacks[2][64];

    static Bitboard bishop_attacks_slow(int sq, Bitboard occ) {
        Bitboard attacks = 0;
        int r = sq / 8, f = sq % 8;
        int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
        for (auto& d : dirs) {
            int rr = r + d[0], ff = f + d[1];
            while (rr >= 0 && rr <= 7 && ff >= 0 && ff <= 7) {
                Bitboard bit = sq_bb(rr * 8 + ff);
                attacks |= bit;
                if (occ & bit) break;
                rr += d[0]; ff += d[1];
            }
        }
        return attacks;
    }

    static Bitboard rook_attacks_slow(int sq, Bitboard occ) {
        Bitboard attacks = 0;
        int r = sq / 8, f = sq % 8;
        auto ray = [&](int dr, int df) {
            int rr = r + dr, ff = f + df;
            while (rr >= 0 && rr <= 7 && ff >= 0 && ff <= 7) {
                Bitboard bit = sq_bb(rr * 8 + ff);
                attacks |= bit;
                if (occ & bit) break;
                rr += dr; ff += df;
            }
        };
        ray(1,0); ray(-1,0); ray(0,1); ray(0,-1);
        return attacks;
    }

    void init() {
        // Bishop masks (exclude edge squares — they don't affect blocking)
        for (int sq = 0; sq < 64; sq++) {
            Bitboard mask = 0;
            int r = sq / 8, f = sq % 8;
            int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
            for (auto& d : dirs) {
                int rr = r + d[0], ff = f + d[1];
                while (rr >= 1 && rr <= 6 && ff >= 1 && ff <= 6) {
                    mask |= sq_bb(rr * 8 + ff);
                    rr += d[0]; ff += d[1];
                }
            }
            bishop_masks[sq] = mask;
            // Carry-rippler: enumerate all subsets of mask
            Bitboard s = 0;
            do {
                bishop_table[sq][_pext_u64(s, mask)] = bishop_attacks_slow(sq, s);
                s = (s - mask) & mask;
            } while (s);
        }

        // Rook masks (exclude edge squares)
        for (int sq = 0; sq < 64; sq++) {
            int r = sq / 8, f = sq % 8;
            Bitboard mask = 0;
            for (int rr = r + 1; rr <= 6; rr++) mask |= sq_bb(rr * 8 + f);
            for (int rr = r - 1; rr >= 1; rr--) mask |= sq_bb(rr * 8 + f);
            for (int ff = f + 1; ff <= 6; ff++) mask |= sq_bb(r * 8 + ff);
            for (int ff = f - 1; ff >= 1; ff--) mask |= sq_bb(r * 8 + ff);
            rook_masks[sq] = mask;
            Bitboard s = 0;
            do {
                rook_table[sq][_pext_u64(s, mask)] = rook_attacks_slow(sq, s);
                s = (s - mask) & mask;
            } while (s);
        }

        // Knight attacks
        for (int sq = 0; sq < 64; sq++) {
            Bitboard b = sq_bb(sq);
            knight_attacks[sq] =
                ((b << 17) & ~FILE_A) | ((b << 15) & ~FILE_H) |
                ((b << 10) & ~(FILE_A | FILE_A << 1)) | ((b << 6) & ~(FILE_H | FILE_H >> 1)) |
                ((b >> 6)  & ~(FILE_A | FILE_A << 1)) | ((b >> 10) & ~(FILE_H | FILE_H >> 1)) |
                ((b >> 15) & ~FILE_A) | ((b >> 17) & ~FILE_H);
        }

        // King attacks
        for (int sq = 0; sq < 64; sq++) {
            Bitboard b = sq_bb(sq);
            king_attacks[sq] =
                (b << 8) | (b >> 8) |
                ((b << 1) & ~FILE_A) | ((b >> 1) & ~FILE_H) |
                ((b << 9) & ~FILE_A) | ((b << 7) & ~FILE_H) |
                ((b >> 7) & ~FILE_A) | ((b >> 9) & ~FILE_H);
        }

        // Pawn attacks
        for (int sq = 0; sq < 64; sq++) {
            Bitboard b = sq_bb(sq);
            pawn_attacks[WHITE][sq] = ((b << 7) & ~FILE_H) | ((b << 9) & ~FILE_A);
            pawn_attacks[BLACK][sq] = ((b >> 7) & ~FILE_A) | ((b >> 9) & ~FILE_H);
        }
    }
}
```

- [ ] **Step 2: Build to check for compile errors**

```bash
cmake --build build --config Release 2>&1 | head -40
```

Expected: compiles without errors (linker may error on missing movegen stubs — that's OK for now).

- [ ] **Step 3: Commit**

```bash
git add src/magic.cpp
git commit -m "feat: Magic::init() — PEXT bishop/rook tables, knight/king/pawn attacks"
```

---

## Task 4: Write `movegen.h` — declarations

**Files:**
- Fill: `src/movegen.h`

- [ ] **Step 1: Write the header**

Replace the empty `src/movegen.h` with:

```cpp
#pragma once
#include "board.h"

int  generate_moves(const Board& b, Move* list);
int  generate_captures(const Board& b, Move* list);
bool is_attacked(int sq, Color by, Bitboard occ, const Board& b);
int  king_sq(const Board& b, Color c);
```

- [ ] **Step 2: Commit**

```bash
git add src/movegen.h
git commit -m "feat: movegen.h interface"
```

---

## Task 5: Write `tests/perft.cpp` — perft harness (the test)

**Files:**
- Fill: `tests/perft.cpp`

- [ ] **Step 1: Write the perft harness**

Replace `tests/perft.cpp` with:

```cpp
#include <cstdio>
#include <cstdint>
#include "board.h"
#include "magic.h"
#include "movegen.h"

static uint64_t perft(Board& b, int depth) {
    if (depth == 0) return 1;
    Move list[256];
    int n = generate_moves(b, list);
    uint64_t nodes = 0;
    StateInfo st;
    for (int i = 0; i < n; i++) {
        b.make_move(list[i], st);
        int ksq = king_sq(b, Color(1 - b.side));
        if (!is_attacked(ksq, b.side, b.occupancy[2], b))
            nodes += perft(b, depth - 1);
        b.unmake_move(list[i], st);
    }
    return nodes;
}

struct Test { const char* fen; int depth; uint64_t expected; };

static const Test TESTS[] = {
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ULL },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6, 11030083ULL },
};

int main() {
    Zobrist::init();
    Magic::init();

    bool all_pass = true;
    for (const auto& t : TESTS) {
        Board b;
        b.load_fen(t.fen);
        uint64_t result = perft(b, t.depth);
        bool pass = (result == t.expected);
        printf("%s  D%d  got=%llu  exp=%llu  %s\n",
               pass ? "PASS" : "FAIL",
               t.depth,
               (unsigned long long)result,
               (unsigned long long)t.expected,
               t.fen);
        if (!pass) all_pass = false;
    }
    return all_pass ? 0 : 1;
}
```

- [ ] **Step 2: Build — expect compile success but wrong counts**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: build succeeds (movegen stubs exist). `perft.exe` would print FAIL since `generate_moves` returns 0. Do not run yet — implement movegen first.

- [ ] **Step 3: Commit**

```bash
git add tests/perft.cpp
git commit -m "test: perft harness with 3 known positions"
```

---

## Task 6: Write `movegen.cpp` — helpers and non-pawn pieces

**Files:**
- Fill: `src/movegen.cpp`

- [ ] **Step 1: Write helpers + knight/king/slider generation**

Create `src/movegen.cpp`:

```cpp
#include "movegen.h"
#include "magic.h"

int king_sq(const Board& b, Color c) {
    return lsb(b.pieces[c][KING]);
}

bool is_attacked(int sq, Color by, Bitboard occ, const Board& b) {
    Bitboard bb = sq_bb(sq);
    Bitboard pawn_atk = (by == WHITE)
        ? ((bb >> 9) & ~FILE_H) | ((bb >> 7) & ~FILE_A)
        : ((bb << 7) & ~FILE_H) | ((bb << 9) & ~FILE_A);
    return (pawn_atk                              & b.pieces[by][PAWN])   ||
           (Magic::knight_attacks[sq]             & b.pieces[by][KNIGHT]) ||
           (Magic::king_attacks[sq]               & b.pieces[by][KING])   ||
           (Magic::bishop_attacks(sq, occ)        & (b.pieces[by][BISHOP] | b.pieces[by][QUEEN])) ||
           (Magic::rook_attacks(sq, occ)          & (b.pieces[by][ROOK]   | b.pieces[by][QUEEN]));
}

static void add_promotions(Move*& list, int from, int to, int cap, int flags) {
    for (int p : {QUEEN, ROOK, BISHOP, KNIGHT})
        *list++ = make_move(from, to, PAWN, cap, p, flags | FLAG_PROMO);
}

int generate_moves(const Board& b, Move* list) {
    Move* start = list;
    Color us   = b.side;
    Color them = Color(1 - us);
    Bitboard occ    = b.occupancy[2];
    Bitboard us_occ = b.occupancy[us];
    Bitboard enemy  = b.occupancy[them];
    Bitboard empty  = ~occ;

    // Knights
    Bitboard knights = b.pieces[us][KNIGHT];
    while (knights) {
        int from = lsb(knights); pop_lsb(knights);
        Bitboard atk = Magic::knight_attacks[from] & ~us_occ;
        while (atk) {
            int to = lsb(atk); pop_lsb(atk);
            int cap = b.piece_on(to);
            int flags = (cap != NO_PIECE) ? FLAG_CAPTURE : FLAG_NONE;
            *list++ = make_move(from, to, KNIGHT, cap, NO_PIECE, flags);
        }
    }

    // Bishops
    Bitboard bishops = b.pieces[us][BISHOP];
    while (bishops) {
        int from = lsb(bishops); pop_lsb(bishops);
        Bitboard atk = Magic::bishop_attacks(from, occ) & ~us_occ;
        while (atk) {
            int to = lsb(atk); pop_lsb(atk);
            int cap = b.piece_on(to);
            int flags = (cap != NO_PIECE) ? FLAG_CAPTURE : FLAG_NONE;
            *list++ = make_move(from, to, BISHOP, cap, NO_PIECE, flags);
        }
    }

    // Rooks
    Bitboard rooks = b.pieces[us][ROOK];
    while (rooks) {
        int from = lsb(rooks); pop_lsb(rooks);
        Bitboard atk = Magic::rook_attacks(from, occ) & ~us_occ;
        while (atk) {
            int to = lsb(atk); pop_lsb(atk);
            int cap = b.piece_on(to);
            int flags = (cap != NO_PIECE) ? FLAG_CAPTURE : FLAG_NONE;
            *list++ = make_move(from, to, ROOK, cap, NO_PIECE, flags);
        }
    }

    // Queens
    Bitboard queens = b.pieces[us][QUEEN];
    while (queens) {
        int from = lsb(queens); pop_lsb(queens);
        Bitboard atk = Magic::queen_attacks(from, occ) & ~us_occ;
        while (atk) {
            int to = lsb(atk); pop_lsb(atk);
            int cap = b.piece_on(to);
            int flags = (cap != NO_PIECE) ? FLAG_CAPTURE : FLAG_NONE;
            *list++ = make_move(from, to, QUEEN, cap, NO_PIECE, flags);
        }
    }

    // King
    {
        int from = king_sq(b, us);
        Bitboard atk = Magic::king_attacks[from] & ~us_occ;
        while (atk) {
            int to = lsb(atk); pop_lsb(atk);
            int cap = b.piece_on(to);
            int flags = (cap != NO_PIECE) ? FLAG_CAPTURE : FLAG_NONE;
            *list++ = make_move(from, to, KING, cap, NO_PIECE, flags);
        }
    }

    // Pawns
    Bitboard pawns = b.pieces[us][PAWN];
    if (us == WHITE) {
        Bitboard single  = (pawns << 8) & empty;
        Bitboard dbl     = ((single & RANK_3) << 8) & empty;
        Bitboard cap_l   = (pawns << 7) & ~FILE_H & enemy;
        Bitboard cap_r   = (pawns << 9) & ~FILE_A & enemy;

        // Single push non-promo
        Bitboard np = single & ~RANK_8;
        while (np) { int to = lsb(np); pop_lsb(np); *list++ = make_move(to-8, to, PAWN); }
        // Double push
        while (dbl) { int to = lsb(dbl); pop_lsb(dbl); *list++ = make_move(to-16, to, PAWN); }
        // Single push promotions
        Bitboard pr = single & RANK_8;
        while (pr) { int to = lsb(pr); pop_lsb(pr); add_promotions(list, to-8, to, NO_PIECE, FLAG_NONE); }
        // Captures non-promo
        Bitboard cl_np = cap_l & ~RANK_8;
        while (cl_np) { int to = lsb(cl_np); pop_lsb(cl_np); *list++ = make_move(to-7, to, PAWN, b.piece_on(to), NO_PIECE, FLAG_CAPTURE); }
        Bitboard cr_np = cap_r & ~RANK_8;
        while (cr_np) { int to = lsb(cr_np); pop_lsb(cr_np); *list++ = make_move(to-9, to, PAWN, b.piece_on(to), NO_PIECE, FLAG_CAPTURE); }
        // Capture promotions
        Bitboard cl_pr = cap_l & RANK_8;
        while (cl_pr) { int to = lsb(cl_pr); pop_lsb(cl_pr); add_promotions(list, to-7, to, b.piece_on(to), FLAG_CAPTURE); }
        Bitboard cr_pr = cap_r & RANK_8;
        while (cr_pr) { int to = lsb(cr_pr); pop_lsb(cr_pr); add_promotions(list, to-9, to, b.piece_on(to), FLAG_CAPTURE); }
        // En passant
        if (b.ep_sq != NO_SQ) {
            Bitboard ep = sq_bb(b.ep_sq);
            Bitboard ep_pawns = pawns & (((ep >> 9) & ~FILE_H) | ((ep >> 7) & ~FILE_A));
            while (ep_pawns) {
                int from = lsb(ep_pawns); pop_lsb(ep_pawns);
                *list++ = make_move(from, b.ep_sq, PAWN, PAWN, NO_PIECE, FLAG_EP | FLAG_CAPTURE);
            }
        }
    } else {
        Bitboard single  = (pawns >> 8) & empty;
        Bitboard dbl     = ((single & RANK_6) >> 8) & empty;
        Bitboard cap_l   = (pawns >> 9) & ~FILE_H & enemy;
        Bitboard cap_r   = (pawns >> 7) & ~FILE_A & enemy;

        Bitboard np = single & ~RANK_1;
        while (np) { int to = lsb(np); pop_lsb(np); *list++ = make_move(to+8, to, PAWN); }
        while (dbl) { int to = lsb(dbl); pop_lsb(dbl); *list++ = make_move(to+16, to, PAWN); }
        Bitboard pr = single & RANK_1;
        while (pr) { int to = lsb(pr); pop_lsb(pr); add_promotions(list, to+8, to, NO_PIECE, FLAG_NONE); }
        Bitboard cl_np = cap_l & ~RANK_1;
        while (cl_np) { int to = lsb(cl_np); pop_lsb(cl_np); *list++ = make_move(to+9, to, PAWN, b.piece_on(to), NO_PIECE, FLAG_CAPTURE); }
        Bitboard cr_np = cap_r & ~RANK_1;
        while (cr_np) { int to = lsb(cr_np); pop_lsb(cr_np); *list++ = make_move(to+7, to, PAWN, b.piece_on(to), NO_PIECE, FLAG_CAPTURE); }
        Bitboard cl_pr = cap_l & RANK_1;
        while (cl_pr) { int to = lsb(cl_pr); pop_lsb(cl_pr); add_promotions(list, to+9, to, b.piece_on(to), FLAG_CAPTURE); }
        Bitboard cr_pr = cap_r & RANK_1;
        while (cr_pr) { int to = lsb(cr_pr); pop_lsb(cr_pr); add_promotions(list, to+7, to, b.piece_on(to), FLAG_CAPTURE); }
        if (b.ep_sq != NO_SQ) {
            Bitboard ep = sq_bb(b.ep_sq);
            Bitboard ep_pawns = pawns & (((ep << 9) & ~FILE_A) | ((ep << 7) & ~FILE_H));
            while (ep_pawns) {
                int from = lsb(ep_pawns); pop_lsb(ep_pawns);
                *list++ = make_move(from, b.ep_sq, PAWN, PAWN, NO_PIECE, FLAG_EP | FLAG_CAPTURE);
            }
        }
    }

    // Castling
    if (us == WHITE) {
        if ((b.castle_rights & CASTLE_WK) &&
            !(occ & (sq_bb(F1) | sq_bb(G1))) &&
            !is_attacked(E1, BLACK, occ, b) &&
            !is_attacked(F1, BLACK, occ, b) &&
            !is_attacked(G1, BLACK, occ, b))
            *list++ = make_move(E1, G1, KING, NO_PIECE, NO_PIECE, FLAG_CASTLE);

        if ((b.castle_rights & CASTLE_WQ) &&
            !(occ & (sq_bb(D1) | sq_bb(C1) | sq_bb(B1))) &&
            !is_attacked(E1, BLACK, occ, b) &&
            !is_attacked(D1, BLACK, occ, b) &&
            !is_attacked(C1, BLACK, occ, b))
            *list++ = make_move(E1, C1, KING, NO_PIECE, NO_PIECE, FLAG_CASTLE);
    } else {
        if ((b.castle_rights & CASTLE_BK) &&
            !(occ & (sq_bb(F8) | sq_bb(G8))) &&
            !is_attacked(E8, WHITE, occ, b) &&
            !is_attacked(F8, WHITE, occ, b) &&
            !is_attacked(G8, WHITE, occ, b))
            *list++ = make_move(E8, G8, KING, NO_PIECE, NO_PIECE, FLAG_CASTLE);

        if ((b.castle_rights & CASTLE_BQ) &&
            !(occ & (sq_bb(D8) | sq_bb(C8) | sq_bb(B8))) &&
            !is_attacked(E8, WHITE, occ, b) &&
            !is_attacked(D8, WHITE, occ, b) &&
            !is_attacked(C8, WHITE, occ, b))
            *list++ = make_move(E8, C8, KING, NO_PIECE, NO_PIECE, FLAG_CASTLE);
    }

    return (int)(list - start);
}

int generate_captures(const Board& b, Move* list) {
    Move all[256];
    int n = generate_moves(b, all);
    Move* start = list;
    for (int i = 0; i < n; i++)
        if (move_flags(all[i]) & FLAG_CAPTURE)
            *list++ = all[i];
    return (int)(list - start);
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -10
```

Expected: both `chess-engine` and `perft` build without errors.

- [ ] **Step 3: Commit**

```bash
git add src/movegen.cpp
git commit -m "feat: full pseudo-legal move generator"
```

---

## Task 7: Wire `Magic::init()` into `main.cpp`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add Magic::init() call**

Replace `src/main.cpp` with:

```cpp
#include "uci.h"
#include "board.h"
#include "magic.h"

int main() {
    Zobrist::init();
    Magic::init();
    uci_loop();
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: call Magic::init() at startup"
```

---

## Task 8: Add missing RANK_3/RANK_6 constants

**Files:**
- Modify: `src/types.h`

The movegen uses `RANK_3` and `RANK_6` for double-push detection. Check `types.h` — it currently only defines `RANK_1`, `RANK_2`, `RANK_7`, `RANK_8`.

- [ ] **Step 1: Add missing rank constants**

In `src/types.h`, after the existing rank constants, add:

```cpp
constexpr Bitboard RANK_3 = 0x0000000000FF0000ULL;
constexpr Bitboard RANK_4 = 0x00000000FF000000ULL;
constexpr Bitboard RANK_5 = 0x000000FF00000000ULL;
constexpr Bitboard RANK_6 = 0x0000FF0000000000ULL;
```

- [ ] **Step 2: Build to confirm no errors**

```bash
cmake --build build --config Release 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/types.h
git commit -m "feat: add RANK_3..RANK_6 bitboard constants"
```

---

## Task 9: Run perft and verify

**Files:** none (read-only verification)

- [ ] **Step 1: Run the perft binary**

```bash
./build/Release/perft.exe
```

On non-MSVC:
```bash
./build/perft
```

Expected output:
```
PASS  D5  got=4865609   exp=4865609   rnbqkbnr/pppppppp/...
PASS  D4  got=4085603   exp=4085603   r3k2r/p1ppqpb1/...
PASS  D6  got=11030083  exp=11030083  8/2p5/3p4/...
```

- [ ] **Step 2: If a test FAILs — split-perft debugging**

Add a depth-1 print loop to isolate which move is wrong:

```cpp
// Temporary debug snippet — add to perft() when depth==1 just before returning
if (depth == 1) {
    char from_sq[3] = { (char)('a' + move_from(list[i]) % 8),
                        (char)('1' + move_from(list[i]) / 8), 0 };
    char to_sq[3]   = { (char)('a' + move_to(list[i]) % 8),
                        (char)('1' + move_to(list[i]) / 8), 0 };
    printf("%s%s: 1\n", from_sq, to_sq);
}
```

Compare the printed move list against a reference engine (e.g., stockfish `go perft 1`) to find the discrepancy. Then look at the move type (castle, EP, promotion) indicated by the wrong move and fix the corresponding code in `movegen.cpp`.

- [ ] **Step 3: Commit after all tests pass**

```bash
git add -A
git commit -m "feat: move generator verified via perft (startpos D5, kiwipete D4, pos3 D6)"
```

---

## Self-Review Checklist

- [x] **Spec coverage:** BMI2 tables ✓, bishop/rook/queen lookups ✓, knight/king pre-computed ✓, pawn attacks ✓, full movegen ✓, castling ✓, EP ✓, promotions ✓, is_attacked ✓, perft harness ✓, 3 test positions ✓
- [x] **No placeholders:** All steps include complete code
- [x] **Type consistency:** `king_sq`, `is_attacked`, `generate_moves` signatures match across movegen.h and movegen.cpp; `Magic::` namespace consistent throughout
- [x] **Missing constant caught:** `RANK_3`/`RANK_6` not in `types.h` — Task 8 adds them
