#include "board.h"
#include <sstream>
#include <random>
#include <cstring>
#include <cstdlib>

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

namespace Zobrist {
    uint64_t piece_keys[2][6][64];
    uint64_t side_key;
    uint64_t castle_keys[16];
    uint64_t ep_keys[8];

    void init() {
        std::mt19937_64 rng(0xDEADBEEF12345678ULL);
        for (auto& c : piece_keys)
            for (auto& p : c)
                for (auto& sq : p) sq = rng();
        side_key = rng();
        for (auto& k : castle_keys) k = rng();
        for (auto& k : ep_keys)     k = rng();
    }
}

void Board::reset() {
    memset(pieces, 0, sizeof(pieces));
    memset(occupancy, 0, sizeof(occupancy));
    side = WHITE; castle_rights = 0; ep_sq = NO_SQ;
    halfmove = 0; fullmove = 1; hash = 0;
}

void Board::update_occupancy() {
    occupancy[WHITE] = occupancy[BLACK] = 0;
    for (int p = 0; p < 6; p++) {
        occupancy[WHITE] |= pieces[WHITE][p];
        occupancy[BLACK] |= pieces[BLACK][p];
    }
    occupancy[2] = occupancy[WHITE] | occupancy[BLACK];
}

int Board::piece_on(int sq) const {
    Bitboard bb = sq_bb(sq);
    for (int p = 0; p < 6; p++)
        if ((pieces[WHITE][p] | pieces[BLACK][p]) & bb) return p;
    return NO_PIECE;
}

int Board::color_on(int sq) const {
    Bitboard bb = sq_bb(sq);
    if (occupancy[WHITE] & bb) return WHITE;
    if (occupancy[BLACK] & bb) return BLACK;
    return -1;
}

bool Board::load_fen(const std::string& fen) {
    reset();
    std::istringstream ss(fen);
    std::string board_str, side_str, castle_str, ep_str;
    ss >> board_str >> side_str >> castle_str >> ep_str >> halfmove >> fullmove;

    int sq = A8;
    for (char c : board_str) {
        if (c == '/') { sq -= 16; continue; }
        if (c >= '1' && c <= '8') { sq += c - '0'; continue; }
        static const char piece_chars[] = "PNBRQKpnbrqk";
        static const int  piece_types[] = {PAWN,KNIGHT,BISHOP,ROOK,QUEEN,KING,PAWN,KNIGHT,BISHOP,ROOK,QUEEN,KING};
        static const int  piece_colors[]= {WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,BLACK,BLACK,BLACK,BLACK,BLACK,BLACK};
        const char* pos = strchr(piece_chars, c);
        if (pos) {
            int idx = (int)(pos - piece_chars);
            pieces[piece_colors[idx]][piece_types[idx]] |= sq_bb(sq++);
        }
    }

    side = (side_str == "b") ? BLACK : WHITE;

    castle_rights = 0;
    for (char c : castle_str) {
        if (c == 'K') castle_rights |= CASTLE_WK;
        if (c == 'Q') castle_rights |= CASTLE_WQ;
        if (c == 'k') castle_rights |= CASTLE_BK;
        if (c == 'q') castle_rights |= CASTLE_BQ;
    }

    ep_sq = NO_SQ;
    if (ep_str != "-") {
        int file = ep_str[0] - 'a';
        int rank = ep_str[1] - '1';
        ep_sq = rank * 8 + file;
    }

    update_occupancy();

    hash = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) {
            Bitboard bb = pieces[c][p];
            while (bb) {
                int s = lsb(bb);
                bb &= bb - 1;
                hash ^= Zobrist::piece_keys[c][p][s];
            }
        }
    if (side == BLACK) hash ^= Zobrist::side_key;
    hash ^= Zobrist::castle_keys[castle_rights];
    if (ep_sq != NO_SQ) hash ^= Zobrist::ep_keys[ep_sq % 8];

    return true;
}

void Board::make_move(Move m, StateInfo& st) {
    st.castle_rights = castle_rights;
    st.ep_sq         = ep_sq;
    st.halfmove      = halfmove;
    st.hash          = hash;

    int from  = move_from(m);
    int to    = move_to(m);
    int piece = move_piece(m);
    int cap   = move_captured(m);
    int promo = move_promo(m);
    int flags = move_flags(m);
    Color us = side, them = Color(1 - us);

    hash ^= Zobrist::castle_keys[castle_rights];
    if (ep_sq != NO_SQ) hash ^= Zobrist::ep_keys[ep_sq % 8];

    pieces[us][piece] &= ~sq_bb(from);
    hash ^= Zobrist::piece_keys[us][piece][from];

    if (flags & FLAG_CAPTURE) {
        if (flags & FLAG_EP) {
            int cap_sq = (us == WHITE) ? to - 8 : to + 8;
            pieces[them][PAWN] &= ~sq_bb(cap_sq);
            hash ^= Zobrist::piece_keys[them][PAWN][cap_sq];
        } else {
            pieces[them][cap] &= ~sq_bb(to);
            hash ^= Zobrist::piece_keys[them][cap][to];
        }
    }

    int placed = (flags & FLAG_PROMO) ? promo : piece;
    pieces[us][placed] |= sq_bb(to);
    hash ^= Zobrist::piece_keys[us][placed][to];

    if (flags & FLAG_CASTLE) {
        auto move_rook = [&](int rfrom, int rto, Color c) {
            pieces[c][ROOK] &= ~sq_bb(rfrom);
            pieces[c][ROOK] |= sq_bb(rto);
            hash ^= Zobrist::piece_keys[c][ROOK][rfrom] ^ Zobrist::piece_keys[c][ROOK][rto];
        };
        if (to == G1) move_rook(H1, F1, us);
        if (to == C1) move_rook(A1, D1, us);
        if (to == G8) move_rook(H8, F8, us);
        if (to == C8) move_rook(A8, D8, us);
    }

    static const int castle_mask[64] = {
        ~CASTLE_WQ,15,15,15,~(CASTLE_WK|CASTLE_WQ),15,15,~CASTLE_WK,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        ~CASTLE_BQ,15,15,15,~(CASTLE_BK|CASTLE_BQ),15,15,~CASTLE_BK,
    };
    castle_rights &= castle_mask[from] & castle_mask[to];
    hash ^= Zobrist::castle_keys[castle_rights];

    ep_sq = NO_SQ;
    if (piece == PAWN && abs(to - from) == 16) {
        ep_sq = (from + to) / 2;
        hash ^= Zobrist::ep_keys[ep_sq % 8];
    }

    halfmove = (piece == PAWN || (flags & FLAG_CAPTURE)) ? 0 : halfmove + 1;
    if (us == BLACK) fullmove++;

    side = them;
    hash ^= Zobrist::side_key;
    update_occupancy();
}

void Board::unmake_move(Move m, const StateInfo& st) {
    side = Color(1 - side);
    Color us = side, them = Color(1 - us);

    int from  = move_from(m);
    int to    = move_to(m);
    int piece = move_piece(m);
    int cap   = move_captured(m);
    int promo = move_promo(m);
    int flags = move_flags(m);

    int placed = (flags & FLAG_PROMO) ? promo : piece;
    pieces[us][placed] &= ~sq_bb(to);

    if (flags & FLAG_CAPTURE) {
        if (flags & FLAG_EP) {
            int cap_sq = (us == WHITE) ? to - 8 : to + 8;
            pieces[them][PAWN] |= sq_bb(cap_sq);
        } else {
            pieces[them][cap] |= sq_bb(to);
        }
    }

    if (flags & FLAG_CASTLE) {
        auto restore_rook = [&](int rfrom, int rto, Color c) {
            pieces[c][ROOK] |= sq_bb(rfrom);
            pieces[c][ROOK] &= ~sq_bb(rto);
        };
        if (to == G1) restore_rook(H1, F1, us);
        if (to == C1) restore_rook(A1, D1, us);
        if (to == G8) restore_rook(H8, F8, us);
        if (to == C8) restore_rook(A8, D8, us);
    }

    pieces[us][piece] |= sq_bb(from);

    castle_rights = st.castle_rights;
    ep_sq         = st.ep_sq;
    halfmove      = st.halfmove;
    hash          = st.hash;
    if (us == BLACK) fullmove--;

    update_occupancy();
}

void Board::make_null_move(StateInfo& st) {
    st.ep_sq    = ep_sq;
    st.halfmove = halfmove;
    st.hash     = hash;
    if (ep_sq != NO_SQ) hash ^= Zobrist::ep_keys[ep_sq % 8];
    ep_sq = NO_SQ;
    halfmove++;
    side = Color(1 - side);
    hash ^= Zobrist::side_key;
}

void Board::unmake_null_move(const StateInfo& st) {
    side     = Color(1 - side);
    ep_sq    = st.ep_sq;
    halfmove = st.halfmove;
    hash     = st.hash;
}
