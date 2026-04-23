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

static const int* PST[6] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING
};
static_assert(sizeof(PST)/sizeof(PST[0]) == NO_PIECE, "PST size mismatch");

int evaluate(const Board& b) {
    int score = 0;
    for (int c = 0; c < 2; c++) {
        int sign = (c == b.side) ? 1 : -1;
        for (int p = 0; p < 6; p++) {
            Bitboard bb = b.pieces[c][p];
            while (bb) {
                int sq = lsb(bb); pop_lsb(bb);
                int pst_sq = (c == WHITE) ? sq : (sq ^ 56);
                score += sign * (MAT[p] + PST[p][pst_sq]);
            }
        }
        if (popcount(b.pieces[c][BISHOP]) >= 2)
            score += sign * 30;
    }
    // King safety — middlegame only
    // Phase: Q=4, R=2, B=1, N=1 per piece, max 24 (full material)
    static const int PHASE_W[6] = { 0, 1, 1, 2, 4, 0 };
    int phase = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            phase += PHASE_W[p] * popcount(b.pieces[c][p]);
    if (phase > 24) phase = 24;

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
