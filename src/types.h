#pragma once
#include <cstdint>
#include <algorithm>

using Bitboard = uint64_t;
using Move     = uint32_t;

enum Color  : int { WHITE = 0, BLACK = 1 };
enum Piece  : int { PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5, NO_PIECE = 6 };
enum Square : int {
    A1=0,B1,C1,D1,E1,F1,G1,H1,
    A2,B2,C2,D2,E2,F2,G2,H2,
    A3,B3,C3,D3,E3,F3,G3,H3,
    A4,B4,C4,D4,E4,F4,G4,H4,
    A5,B5,C5,D5,E5,F5,G5,H5,
    A6,B6,C6,D6,E6,F6,G6,H6,
    A7,B7,C7,D7,E7,F7,G7,H7,
    A8,B8,C8,D8,E8,F8,G8,H8,
    NO_SQ = 64
};

constexpr int CASTLE_WK = 1;
constexpr int CASTLE_WQ = 2;
constexpr int CASTLE_BK = 4;
constexpr int CASTLE_BQ = 8;

enum MoveFlag : uint32_t {
    FLAG_NONE    = 0,
    FLAG_CASTLE  = 1,
    FLAG_EP      = 2,
    FLAG_PROMO   = 4,
    FLAG_CAPTURE = 8,
};

inline Move make_move(int from, int to, int piece, int captured = NO_PIECE, int promo = NO_PIECE, int flags = FLAG_NONE) {
    return (uint32_t)from | ((uint32_t)to << 6) | ((uint32_t)piece << 12) | ((uint32_t)captured << 16) | ((uint32_t)promo << 20) | ((uint32_t)flags << 24);
}
inline int move_from(Move m)     { return m & 0x3F; }
inline int move_to(Move m)       { return (m >> 6) & 0x3F; }
inline int move_piece(Move m)    { return (m >> 12) & 0xF; }
inline int move_captured(Move m) { return (m >> 16) & 0xF; }
inline int move_promo(Move m)    { return (m >> 20) & 0xF; }
inline int move_flags(Move m)    { return (m >> 24) & 0xF; }

inline Bitboard sq_bb(int sq)        { return 1ULL << sq; }

#ifdef _MSC_VER
    #include <intrin.h>
    inline int lsb(Bitboard b) {
        unsigned long index;
        _BitScanForward64(&index, b);
        return (int)index;
    }
    inline int popcount(Bitboard b) { return (int)__popcnt64(b); }
#else
    inline int lsb(Bitboard b)      { return __builtin_ctzll(b); }
    inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
#endif

inline Bitboard pop_lsb(Bitboard& b) { Bitboard bit = b & -b; b &= b - 1; return bit; }

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = 0x8080808080808080ULL;
constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_2 = 0x000000000000FF00ULL;
constexpr Bitboard RANK_7 = 0x00FF000000000000ULL;
constexpr Bitboard RANK_8 = 0xFF00000000000000ULL;
