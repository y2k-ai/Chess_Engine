# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Two targets are produced:
- `chess-engine` — the UCI engine (`src/main.cpp` → `uci_loop()`)
- `perft` — standalone perft test runner (`tests/perft.cpp`)

Debug build: replace `Release` with `Debug`. On MSVC, `/RTC1` is stripped and `/O2` is applied to both configs to avoid runtime conflicts.

## Running

```bash
./build/chess-engine   # starts UCI loop
./build/perft          # runs perft tests
```

On Windows/MSVC the binaries land under `build/Debug/` or `build/Release/` (e.g. `build/Release/chess-engine.exe`).

## Architecture

**Representation layer** (`types.h`, `board.h/cpp`)
- Bitboard-per-piece: `Board::pieces[color][piece]` + `occupancy[3]` (WHITE, BLACK, BOTH).
- `Move` is a packed `uint32_t`: from(6) | to(6) | piece(4) | captured(4) | promo(4) | flags(4). Use `make_move()` and `move_*()` accessors exclusively.
- `StateInfo` holds the fields that `unmake_move` cannot recover arithmetically (castle rights, ep square, halfmove clock, hash). Callers must allocate one per ply on the stack and pass it to `make_move`; `unmake_move` restores from it.
- Zobrist hash is maintained incrementally inside `make_move`/`unmake_move` and seeded with a fixed 64-bit key at `Zobrist::init()`.

**Move generation** (`magic.h/cpp`, `movegen.h/cpp`)
- Sliding piece attacks (bishop, rook, queen) will use magic bitboard lookup tables initialized in `magic.cpp`.
- `movegen.cpp` populates a move list for a given `Board` position.
- Both `.h` and `.cpp` files are currently empty stubs — implementation pending.

**Search / Eval / UCI** (`search.h/cpp`, `eval.h/cpp`, `uci.h/cpp`)
- `search.h/cpp` and `eval.h/cpp` are empty stubs; `uci_loop()` is the engine's entry point and drives everything.

## Key conventions

- Squares are indexed A1=0 … H8=63, rank-major (A1, B1, … H1, A2 …).
- `lsb()` / `popcount()` use MSVC intrinsics (`_BitScanForward64`, `__popcnt64`) on Windows and `__builtin_*` elsewhere — always go through those wrappers.
- Standard bitboard iteration: `while (bb) { int sq = lsb(bb); pop_lsb(bb); /* use sq */ }` — `pop_lsb()` clears the LSB in-place and returns the bit.
- `Board::piece_on(sq)` / `Board::color_on(sq)` query which piece/color occupies a square (return `NO_PIECE` / undefined if empty).
- `Board::load_fen()` sets up a position; `START_FEN` (extern from `board.h`) holds the standard starting position.
- `Zobrist::init()` must be called before any `Board` is used.
