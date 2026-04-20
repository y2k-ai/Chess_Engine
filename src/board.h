#pragma once
#include "types.h"
#include <string>

struct StateInfo {
    int      castle_rights;
    int      ep_sq;
    int      halfmove;
    uint64_t hash;
};

struct Board {
    Bitboard pieces[2][6];   // [color][piece]
    Bitboard occupancy[3];   // [WHITE, BLACK, BOTH]
    Color    side;
    int      castle_rights;
    int      ep_sq;
    int      halfmove;
    int      fullmove;
    uint64_t hash;

    void reset();
    bool load_fen(const std::string& fen);
    void update_occupancy();
    int  piece_on(int sq) const;
    int  color_on(int sq) const;
    void make_move(Move m, StateInfo& st);
    void unmake_move(Move m, const StateInfo& st);
    void make_null_move(StateInfo& st);
    void unmake_null_move(const StateInfo& st);
};

namespace Zobrist {
    extern uint64_t piece_keys[2][6][64];
    extern uint64_t side_key;
    extern uint64_t castle_keys[16];
    extern uint64_t ep_keys[8];
    void init();
}

extern const std::string START_FEN;
