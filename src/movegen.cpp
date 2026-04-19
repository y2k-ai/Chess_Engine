#include "movegen.h"
#include "magic.h"

int king_sq(const Board& b, Color c) {
    return lsb(b.pieces[c][KING]);
}

bool is_attacked(int sq, Color by, Bitboard occ, const Board& b) {
    return (Magic::pawn_attacks[by][sq]           & b.pieces[by][PAWN])   ||
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

        Bitboard np = single & ~RANK_8;
        while (np) { int to = lsb(np); pop_lsb(np); *list++ = make_move(to-8, to, PAWN); }
        while (dbl) { int to = lsb(dbl); pop_lsb(dbl); *list++ = make_move(to-16, to, PAWN); }
        Bitboard pr = single & RANK_8;
        while (pr) { int to = lsb(pr); pop_lsb(pr); add_promotions(list, to-8, to, NO_PIECE, FLAG_NONE); }
        Bitboard cl_np = cap_l & ~RANK_8;
        while (cl_np) { int to = lsb(cl_np); pop_lsb(cl_np); *list++ = make_move(to-7, to, PAWN, b.piece_on(to), NO_PIECE, FLAG_CAPTURE); }
        Bitboard cr_np = cap_r & ~RANK_8;
        while (cr_np) { int to = lsb(cr_np); pop_lsb(cr_np); *list++ = make_move(to-9, to, PAWN, b.piece_on(to), NO_PIECE, FLAG_CAPTURE); }
        Bitboard cl_pr = cap_l & RANK_8;
        while (cl_pr) { int to = lsb(cl_pr); pop_lsb(cl_pr); add_promotions(list, to-7, to, b.piece_on(to), FLAG_CAPTURE); }
        Bitboard cr_pr = cap_r & RANK_8;
        while (cr_pr) { int to = lsb(cr_pr); pop_lsb(cr_pr); add_promotions(list, to-9, to, b.piece_on(to), FLAG_CAPTURE); }
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
    Move all[320];
    int n = generate_moves(b, all);
    Move* start = list;
    for (int i = 0; i < n; i++)
        if (move_flags(all[i]) & FLAG_CAPTURE)
            *list++ = all[i];
    return (int)(list - start);
}
