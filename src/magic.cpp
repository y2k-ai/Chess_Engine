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
