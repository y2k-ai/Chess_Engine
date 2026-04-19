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
