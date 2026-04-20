#include "search.h"
#include "eval.h"
#include "movegen.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

// ── Stop flag ──────────────────────────────────────────────────────────────
std::atomic<bool> stop_search{false};

// ── Transposition table ───────────────────────────────────────────────────
static TTEntry tt[TT_SIZE];

void tt_clear() { memset(tt, 0, sizeof(tt)); }

static TTEntry* tt_probe(uint64_t hash) {
    return &tt[hash % TT_SIZE];
}

static void tt_store(uint64_t hash, int depth, int score, TTFlag flag, Move move) {
    TTEntry* e = &tt[hash % TT_SIZE];
    if (e->flag != TT_NONE && e->depth > depth && e->hash == hash) return;
    e->hash  = hash;
    e->depth = (uint8_t)depth;
    e->score = (int16_t)score;
    e->flag  = flag;
    e->move  = move;
}

// ── Timing ────────────────────────────────────────────────────────────────
static std::chrono::steady_clock::time_point search_start;
static int  time_limit_ms = 0;
static bool time_out      = false;

static int elapsed_ms() {
    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - search_start).count();
}

static bool check_time() {
    if (stop_search.load(std::memory_order_relaxed)) return true;
    if (time_limit_ms > 0 && elapsed_ms() >= time_limit_ms) return true;
    return false;
}

// ── Search state ──────────────────────────────────────────────────────────
static uint64_t nodes;
static Move     killers[2][MAX_PLY];
static int      history[2][6][64];
static Move     pv[MAX_PLY][MAX_PLY];
static int      pv_len[MAX_PLY];

// ── Helpers ───────────────────────────────────────────────────────────────
static inline bool in_check(const Board& b) {
    return is_attacked(king_sq(b, b.side), Color(1 - b.side), b.occupancy[2], b);
}

static void score_moves(const Move* list, int* scores, int n,
                        Move tt_move, int ply, Color us) {
    for (int i = 0; i < n; i++) {
        Move m = list[i];
        int f = move_flags(m);
        if (m == tt_move)
            scores[i] = 2000000;
        else if (f & FLAG_CAPTURE)
            scores[i] = 1000000 + move_captured(m) * 100 - move_piece(m);
        else if (m == killers[0][ply])
            scores[i] = 900000;
        else if (m == killers[1][ply])
            scores[i] = 800000;
        else
            scores[i] = history[us][move_piece(m)][move_to(m)];
    }
}

static inline void pick_best(Move* list, int* scores, int idx, int n) {
    int best = idx;
    for (int i = idx + 1; i < n; i++)
        if (scores[i] > scores[best]) best = i;
    if (best != idx) {
        std::swap(list[idx], list[best]);
        std::swap(scores[idx], scores[best]);
    }
}

// ── Quiescence search ─────────────────────────────────────────────────────
static int quiescence(Board& b, int alpha, int beta, int ply) {
    if (ply >= MAX_PLY) return evaluate(b);

    nodes++;
    if ((nodes & 2047) == 0 && check_time()) { time_out = true; return 0; }

    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    pv_len[ply] = ply;

    Move list[320];
    int n = generate_captures(b, list);

    int scores[320];
    score_moves(list, scores, n, 0, ply, b.side);

    for (int i = 0; i < n; i++) {
        pick_best(list, scores, i, n);
        Move m = list[i];

        // Delta pruning
        if (stand_pat + 900 + 200 < alpha && !(move_flags(m) & FLAG_PROMO))
            continue;

        StateInfo st;
        b.make_move(m, st);
        if (is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b)) {
            b.unmake_move(m, st);
            continue;
        }
        int score = -quiescence(b, -beta, -alpha, ply + 1);
        b.unmake_move(m, st);

        if (time_out) return 0;
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}
