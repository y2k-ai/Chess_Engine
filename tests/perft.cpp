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
