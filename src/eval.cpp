#include "eval.h"
#include "magic.h"

static const int MAT[6] = { 100, 320, 330, 500, 900, 0 };
static_assert(sizeof(MAT)/sizeof(MAT[0]) == NO_PIECE, "MAT size mismatch");

// PST tables — white perspective, index 0=A1, 63=H8 (rank 1 first)
// For black pieces use sq ^ 56 (mirror rank)
static const int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int PST_ROOK[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};
static const int PST_KING[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
};
static const int PST_KING_EG[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-20,-10,-10,-10,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,-10,-10,-10,-20,-30,
    -50,-30,-30,-30,-30,-30,-30,-50,
};

static const int* PST[6] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING
};
static_assert(sizeof(PST)/sizeof(PST[0]) == NO_PIECE, "PST size mismatch");

int evaluate(const Board& b) {
    // Phase: Q=4, R=2, B=1, N=1 per piece, max 24 (full material = middlegame)
    static const int PHASE_W[6] = { 0, 1, 1, 2, 4, 0 };
    int phase = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            phase += PHASE_W[p] * popcount(b.pieces[c][p]);
    if (phase > 24) phase = 24;

    int score = 0;
    for (int c = 0; c < 2; c++) {
        int sign = (c == b.side) ? 1 : -1;
        for (int p = 0; p < 6; p++) {
            Bitboard bb = b.pieces[c][p];
            while (bb) {
                int sq = lsb(bb); pop_lsb(bb);
                int pst_sq = (c == WHITE) ? sq : (sq ^ 56);
                int pst_val = (p == KING)
                    ? (PST_KING[pst_sq] * phase + PST_KING_EG[pst_sq] * (24 - phase)) / 24
                    : PST[p][pst_sq];
                score += sign * (MAT[p] + pst_val);
            }
        }
        if (popcount(b.pieces[c][BISHOP]) >= 2)
            score += sign * 30;
    }

    // Pawn structure evaluation
    {
        static const Bitboard FILE_BB[8] = {
            0x0101010101010101ULL, 0x0202020202020202ULL,
            0x0404040404040404ULL, 0x0808080808080808ULL,
            0x1010101010101010ULL, 0x2020202020202020ULL,
            0x4040404040404040ULL, 0x8080808080808080ULL,
        };
        static const int PASSED_BONUS[8] = { 0, 10, 20, 30, 50, 80, 120, 0 };

        for (int c = 0; c < 2; c++) {
            int sign = (c == b.side) ? 1 : -1;
            Color opp = Color(1 - c);
            Bitboard own_pawns = b.pieces[c][PAWN];

            // Passed and isolated pawn detection (per pawn)
            Bitboard pawns = own_pawns;
            while (pawns) {
                int sq = lsb(pawns); pop_lsb(pawns);
                int f = sq % 8, r = sq / 8;
                int rel_rank = (c == WHITE) ? r : (7 - r);

                Bitboard adj_files = FILE_BB[f];
                if (f > 0) adj_files |= FILE_BB[f - 1];
                if (f < 7) adj_files |= FILE_BB[f + 1];

                // Squares strictly ahead of this pawn (toward enemy back rank)
                Bitboard ahead = (c == WHITE)
                    ? (r < 7 ? (~0ULL << (8 * (r + 1))) : 0ULL)
                    : ((1ULL << (8 * r)) - 1);

                // Passed pawn: no enemy pawns on same+adjacent files ahead
                if (!(b.pieces[opp][PAWN] & adj_files & ahead))
                    score += sign * PASSED_BONUS[rel_rank];

                // Isolated pawn: no friendly pawns on adjacent files (any rank)
                Bitboard adj_only = 0;
                if (f > 0) adj_only |= FILE_BB[f - 1];
                if (f < 7) adj_only |= FILE_BB[f + 1];
                if (!(own_pawns & adj_only))
                    score -= sign * 20;
            }

            // Doubled pawns: more than one pawn on same file
            for (int f2 = 0; f2 < 8; f2++) {
                int cnt = popcount(own_pawns & FILE_BB[f2]);
                if (cnt > 1)
                    score -= sign * 15 * (cnt - 1);
            }
        }
    }

    // Mobility evaluation — bonus per reachable square, own pieces excluded
    {
        static const int MOB_BONUS[6] = { 0, 4, 3, 2, 1, 0 };
        Bitboard occ = b.occupancy[2];
        for (int c = 0; c < 2; c++) {
            int sign = (c == b.side) ? 1 : -1;
            Bitboard own = b.occupancy[c];
            Bitboard bb;

            bb = b.pieces[c][KNIGHT];
            while (bb) { int sq = lsb(bb); pop_lsb(bb);
                score += sign * MOB_BONUS[KNIGHT] * popcount(Magic::knight_attacks[sq] & ~own); }

            bb = b.pieces[c][BISHOP];
            while (bb) { int sq = lsb(bb); pop_lsb(bb);
                score += sign * MOB_BONUS[BISHOP] * popcount(Magic::bishop_attacks(sq, occ) & ~own); }

            bb = b.pieces[c][ROOK];
            while (bb) { int sq = lsb(bb); pop_lsb(bb);
                score += sign * MOB_BONUS[ROOK] * popcount(Magic::rook_attacks(sq, occ) & ~own); }

            bb = b.pieces[c][QUEEN];
            while (bb) { int sq = lsb(bb); pop_lsb(bb);
                score += sign * MOB_BONUS[QUEEN] * popcount(Magic::queen_attacks(sq, occ) & ~own); }
        }
    }

    // King safety — middlegame only
    if (phase >= 8) {
        static const int ATK_PEN[8] = { 0, 0, 20, 40, 70, 110, 160, 200 };
        for (int c = 0; c < 2; c++) {
            int sign = (c == b.side) ? 1 : -1;
            int ksq  = lsb(b.pieces[c][KING]);
            Color opp = Color(1 - c);
            Bitboard occ = b.occupancy[2];
            int penalty = 0;

            // Pawn shield: up to 3 squares one rank in front of king
            int kfile = ksq % 8, krank = ksq / 8;
            int shield_rank = (c == WHITE) ? krank + 1 : krank - 1;
            if (shield_rank >= 0 && shield_rank <= 7) {
                int flo = kfile > 0 ? kfile - 1 : 0;
                int fhi = kfile < 7 ? kfile + 1 : 7;
                for (int f = flo; f <= fhi; f++) {
                    if (!(b.pieces[c][PAWN] & sq_bb(shield_rank * 8 + f)))
                        penalty += 15;
                }
            }

            // Attacker count: opponent pieces hitting the 9-square king zone
            Bitboard zone = Magic::king_attacks[ksq] | sq_bb(ksq);
            int attackers = 0;
            Bitboard bb;
            bb = b.pieces[opp][KNIGHT];
            while (bb) { int sq = lsb(bb); pop_lsb(bb); if (Magic::knight_attacks[sq] & zone) attackers++; }
            bb = b.pieces[opp][BISHOP] | b.pieces[opp][QUEEN];
            while (bb) { int sq = lsb(bb); pop_lsb(bb); if (Magic::bishop_attacks(sq, occ) & zone) attackers++; }
            bb = b.pieces[opp][ROOK] | b.pieces[opp][QUEEN];
            while (bb) { int sq = lsb(bb); pop_lsb(bb); if (Magic::rook_attacks(sq, occ) & zone) attackers++; }
            penalty += ATK_PEN[attackers < 8 ? attackers : 7];

            score -= sign * penalty * phase / 24;
        }
    }

    score += 10;
    return score;
}
