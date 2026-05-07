#include "search.h"
#include "book.h"
#include "magic.h"
#include "eval.h"
#include "movegen.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

// ── Stop flag ──────────────────────────────────────────────────────────────
std::atomic<bool> stop_search{false};

// ── LMR reduction table ────────────────────────────────────────────────────
// LMR reduction table: LMR_TABLE[depth][legal] = round(log(depth)*log(legal)/2)
static int LMR_TABLE[MAX_PLY][MAX_PLY];
static const bool lmr_table_init = []() {
    for (int d = 0; d < MAX_PLY; d++)
        for (int l = 0; l < MAX_PLY; l++)
            LMR_TABLE[d][l] = (d > 0 && l > 0)
                ? std::max(0, (int)std::round(std::log((double)d) * std::log((double)l) / 2.0))
                : 0;
    return true;
}();

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
static Move     countermoves[6][64];
static Move     pv[MAX_PLY][MAX_PLY];
static int      pv_len[MAX_PLY];
static uint64_t hash_history[1024];
static int      hash_history_len = 0;

// ── Helpers ───────────────────────────────────────────────────────────────
static inline bool in_check(const Board& b) {
    return is_attacked(king_sq(b, b.side), Color(1 - b.side), b.occupancy[2], b);
}

static int see(const Board& b, Move m); // forward declaration

static void score_moves(const Board& b, const Move* list, int* scores, int n,
                        Move tt_move, int ply, Color us, Move prev_move) {
    Move cm = (prev_move) ? countermoves[move_piece(prev_move)][move_to(prev_move)] : 0;
    for (int i = 0; i < n; i++) {
        Move m = list[i];
        int f = move_flags(m);
        if (m == tt_move)
            scores[i] = 2000000;
        else if (f & FLAG_CAPTURE)
            scores[i] = 1000000 + see(b, m);
        else if (m == killers[0][ply])
            scores[i] = 900000;
        else if (m == killers[1][ply])
            scores[i] = 800000;
        else {
            int h = history[us][move_piece(m)][move_to(m)];
            int bonus = 0;
            int to = move_to(m);
            int piece = move_piece(m);

            if (piece == QUEEN) bonus += 500;
            if (piece == ROOK) bonus += 300;
            if (piece == BISHOP) bonus += 100;
            if (piece == KNIGHT) bonus += 100;
            if (piece == KING) bonus += 200;

            int center_dist = std::max(std::abs((to % 8) - 3), std::abs((to / 8) - 3));
            bonus += (7 - center_dist) * 50;

            scores[i] = h + bonus;
        }
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

// ── Repetition history ────────────────────────────────────────────────────
void rep_push(uint64_t hash) {
    if (hash_history_len < 1024) hash_history[hash_history_len++] = hash;
}

void rep_clear() { hash_history_len = 0; }

// ── Static Exchange Evaluation ────────────────────────────────────────────
static const int SEE_VAL[7] = { 100, 320, 330, 500, 900, 20000, 0 };

static Bitboard all_attackers(const Board& b, int sq, Bitboard occ) {
    return (Magic::pawn_attacks[BLACK][sq] & b.pieces[WHITE][PAWN])
         | (Magic::pawn_attacks[WHITE][sq] & b.pieces[BLACK][PAWN])
         | (Magic::knight_attacks[sq] & (b.pieces[WHITE][KNIGHT] | b.pieces[BLACK][KNIGHT]))
         | (Magic::bishop_attacks(sq, occ) & (b.pieces[WHITE][BISHOP] | b.pieces[BLACK][BISHOP]
                                            | b.pieces[WHITE][QUEEN]  | b.pieces[BLACK][QUEEN]))
         | (Magic::rook_attacks(sq, occ)   & (b.pieces[WHITE][ROOK]   | b.pieces[BLACK][ROOK]
                                            | b.pieces[WHITE][QUEEN]  | b.pieces[BLACK][QUEEN]))
         | (Magic::king_attacks[sq] & (b.pieces[WHITE][KING] | b.pieces[BLACK][KING]));
}

static int see(const Board& b, Move m) {
    int to     = move_to(m);
    int from   = move_from(m);
    int target = move_captured(m);
    if (target == NO_PIECE) return 0;

    int      gain[32], d = 0;
    int      atkr_piece = move_piece(m);
    Bitboard occ = b.occupancy[2] ^ sq_bb(from);

    gain[d] = SEE_VAL[target];
    Bitboard attadef = all_attackers(b, to, occ) & occ;
    Color    side    = Color(1 - b.side);

    while (true) {
        d++;
        gain[d] = SEE_VAL[atkr_piece] - gain[d - 1];
        if (std::max(-gain[d - 1], gain[d]) < 0) break;
        Bitboard lva = 0; atkr_piece = NO_PIECE;
        for (int p = PAWN; p <= KING; p++) {
            Bitboard s = attadef & b.pieces[side][p];
            if (s) { lva = s & -s; atkr_piece = p; break; }
        }
        if (!lva) break;
        occ     ^= lva;
        attadef  = all_attackers(b, to, occ) & occ;
        side     = Color(1 - side);
    }
    while (--d) gain[d - 1] = std::max(-gain[d], gain[d - 1]);
    return gain[0];
}

// ── Quiescence search ─────────────────────────────────────────────────────
static int quiescence(Board& b, int alpha, int beta, int ply, Move prev_move = 0) {
    if (ply >= MAX_PLY) return evaluate(b);

    nodes++;
    if ((nodes & 255) == 0 && check_time()) { time_out = true; return 0; }

    pv_len[ply] = ply;

    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    Move list[320];
    int n = generate_captures(b, list);

    int scores[320];
    score_moves(b, list, scores, n, 0, ply, b.side, 0);

    for (int i = 0; i < n; i++) {
        pick_best(list, scores, i, n);
        Move m = list[i];

        if (!(move_flags(m) & FLAG_PROMO) && scores[i] < 1000000)
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

// ── Negamax alpha-beta ────────────────────────────────────────────────────
static int negamax(Board& b, int depth, int alpha, int beta, int ply, bool null_ok, Move prev_move = 0) {
    if (ply >= MAX_PLY) return evaluate(b);

    nodes++;
    if ((nodes & 255) == 0 && check_time()) { time_out = true; return 0; }

    pv_len[ply] = ply;

    if (b.halfmove >= 100) return 0;

    // Repetition detection: scan back same-side positions bounded by halfmove clock
    {
        int start = hash_history_len - 3;
        int bound = hash_history_len - 1 - b.halfmove;
        for (int i = start; i >= bound && i >= 0; i -= 2)
            if (hash_history[i] == b.hash) return 0;
    }

    bool pv_node = (beta - alpha > 1);

    // TT probe
    TTEntry* tte = tt_probe(b.hash);
    Move tt_move = 0;
    if (tte->hash == b.hash) {
        tt_move = tte->move;
        if (!pv_node && tte->depth >= depth) {
            int s = tte->score;
            if (tte->flag == TT_EXACT) return s;
            if (tte->flag == TT_LOWER && s >= beta)  return s;
            if (tte->flag == TT_UPPER && s <= alpha) return s;
        }
    }

    bool in_chk = in_check(b);
    if (in_chk) depth++;

    if (depth <= 0) return quiescence(b, alpha, beta, ply, prev_move);

    // Null-move pruning
    if (null_ok && !in_chk && !pv_node && depth >= 3) {
        Bitboard majors = b.pieces[b.side][KNIGHT] | b.pieces[b.side][BISHOP]
                        | b.pieces[b.side][ROOK]   | b.pieces[b.side][QUEEN];
        if (majors) {
            int R = (depth >= 6) ? 4 : 3;
            StateInfo st;
            b.make_null_move(st);
            int null_score = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
            b.unmake_null_move(st);
            if (!time_out && null_score >= beta) return beta;
        }
    }

    // Probcut: if a capture is good enough at reduced depth, prune
    if (!pv_node && !in_chk && depth >= 5 && abs(beta) < MATE - 100) {
        int pc_beta   = beta + 200;
        int pc_static = evaluate(b);
        Move cap_list[256];
        int  cap_n = generate_captures(b, cap_list);
        int  cap_scores[256];
        score_moves(b, cap_list, cap_scores, cap_n, tt_move, ply, b.side, prev_move);
        for (int ci = 0; ci < cap_n; ci++) {
            pick_best(cap_list, cap_scores, ci, cap_n);
            Move m = cap_list[ci];
            if (see(b, m) < pc_beta - pc_static) continue;
            StateInfo st;
            b.make_move(m, st);
            if (is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b)) {
                b.unmake_move(m, st);
                continue;
            }
            int pc_score = -negamax(b, depth - 4, -pc_beta, -pc_beta + 1, ply + 1, false, m);
            b.unmake_move(m, st);
            if (!time_out && pc_score >= pc_beta) return pc_score;
        }
    }

    int static_eval = (!in_chk && !pv_node && depth <= 7) ? evaluate(b) : -INF;

    // Reverse futility pruning: if static eval is well above beta, prune early
    if (static_eval != -INF && depth >= 1 && depth <= 7) {
        if (static_eval - depth * 70 >= beta)
            return static_eval;
    }

    Move list[320];
    int  scores[320];
    int  n = generate_moves(b, list);
    score_moves(b, list, scores, n, tt_move, ply, b.side, prev_move);

    int  best_score = -INF;
    Move best_move  = 0;
    int  legal      = 0;
    TTFlag flag     = TT_UPPER;

    for (int i = 0; i < n; i++) {
        pick_best(list, scores, i, n);
        Move m = list[i];

        StateInfo st;
        b.make_move(m, st);
        if (is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b)) {
            b.unmake_move(m, st);
            continue;
        }
        legal++;

        // Futility pruning
        static const int FP_MARGIN[3] = { 0, 100, 300 };
        if (static_eval != -INF && depth <= 2
            && !(move_flags(m) & (FLAG_CAPTURE | FLAG_PROMO))
            && m != killers[0][ply] && m != killers[1][ply]
            && static_eval + FP_MARGIN[depth] <= alpha) {
            b.unmake_move(m, st);
            continue;
        }

        hash_history[hash_history_len++] = b.hash;
        int new_depth = depth - 1;
        bool quiet = !(move_flags(m) & (FLAG_CAPTURE | FLAG_PROMO));
        bool reduced = !in_chk && legal >= 3 && depth >= 3 && quiet
                       && m != killers[0][ply] && m != killers[1][ply];
        if (reduced) {
            int r = LMR_TABLE[std::min(depth, MAX_PLY - 1)][std::min(legal, MAX_PLY - 1)];
            new_depth = std::max(1, new_depth - r);
        }
        int score;
        if (legal == 1) {
            score = -negamax(b, new_depth, -beta, -alpha, ply + 1, true, m);
        } else {
            score = -negamax(b, new_depth, -alpha - 1, -alpha, ply + 1, true, m);
            if (!time_out && score > alpha && score < beta)
                score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, true, m);
        }
        hash_history_len--;
        b.unmake_move(m, st);

        if (time_out) return 0;

        if (score > best_score) {
            best_score = score;
            best_move  = m;
            pv[ply][ply] = m;
            int copy_len = pv_len[ply + 1] - ply - 1;
            if (copy_len > 0)
                memcpy(pv[ply] + ply + 1, pv[ply + 1] + ply + 1, copy_len * sizeof(Move));
            pv_len[ply] = pv_len[ply + 1];

            if (score > alpha) {
                alpha = score;
                flag  = TT_EXACT;
                if (score >= beta) {
                    if (!(move_flags(m) & FLAG_CAPTURE)) {
                        killers[1][ply] = killers[0][ply];
                        killers[0][ply] = m;
                        history[b.side][move_piece(m)][move_to(m)] += depth * depth;
                        if (prev_move) countermoves[move_piece(prev_move)][move_to(prev_move)] = m;
                    }
                    tt_store(b.hash, depth, score, TT_LOWER, m);
                    return score;
                }
            }
        }
    }

    if (legal == 0) return in_chk ? -(MATE - ply) : 0;

    tt_store(b.hash, depth, best_score, flag, best_move);
    return best_score;
}

// ── Iterative deepening entry point ──────────────────────────────────────
SearchResult search(Board& b, const SearchLimits& lim) {
    search_start  = std::chrono::steady_clock::now();
    time_out      = false;
    stop_search.store(false, std::memory_order_relaxed);
    nodes         = 0;

    if (lim.movetime > 0) {
        time_limit_ms = lim.movetime;
    } else if (lim.wtime > 0 || lim.btime > 0) {
        int my_time = (b.side == WHITE) ? lim.wtime : lim.btime;
        int my_inc  = (b.side == WHITE) ? lim.winc  : lim.binc;
        time_limit_ms = my_time / 20 + my_inc / 2;
    } else {
        time_limit_ms = 0;
    }

    int max_depth = (lim.depth > 0) ? lim.depth
                  : (lim.infinite)  ? 24
                  : 24;

    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));
    memset(countermoves, 0, sizeof(countermoves));

    SearchResult result;
    int prev_score = 0;

    // Check opening book first
    Move book_move = book_probe(b);
    if (book_move != 0) {
        result.best_move = book_move;
        result.score = 0;
        return result;
    }

    for (int d = 1; d <= max_depth; d++) {
        int alpha = -INF, beta = INF;
        if (d > 1) { alpha = prev_score - 50; beta = prev_score + 50; }

        int score;
        while (true) {
            score = negamax(b, d, alpha, beta, 0, true);
            if (time_out) break;
            if      (score <= alpha) { alpha -= 100; if (alpha < -INF/2) alpha = -INF; }
            else if (score >= beta)  { beta  += 100; if (beta  >  INF/2) beta  =  INF; }
            else break;
        }

        if (time_out && d > 1) break;

        prev_score       = score;
        result.score     = score;
        result.depth     = d;
        result.nodes     = nodes;
        if (pv_len[0] > 0) result.best_move = pv[0][0];

        int ms = elapsed_ms();
        std::cout << "info depth " << d;
        if (std::abs(score) > MATE - MAX_PLY) {
            int moves = (MATE - std::abs(score) + 1) / 2;
            std::cout << " score mate " << (score > 0 ? moves : -moves);
        } else {
            std::cout << " score cp " << score;
        }
        std::cout << " nodes " << nodes
                  << " time " << ms
                  << " pv";
        for (int k = 0; k < pv_len[0]; k++) {
            int from = move_from(pv[0][k]), to = move_to(pv[0][k]);
            char mv[6];
            mv[0] = (char)('a' + from % 8); mv[1] = (char)('1' + from / 8);
            mv[2] = (char)('a' + to   % 8); mv[3] = (char)('1' + to   / 8);
            int promo = move_promo(pv[0][k]);
            if (move_flags(pv[0][k]) & FLAG_PROMO) {
                mv[4] = "pnbrqk"[promo]; mv[5] = 0;
            } else { mv[4] = 0; }
            std::cout << " " << mv;
        }
        std::cout << "\n";
        std::cout.flush();

        if (time_limit_ms > 0 && elapsed_ms() >= time_limit_ms / 2) break;
    }

    // Fallback: if time ran out before any PV was established, pick first legal move
    if (result.best_move == 0) {
        Move list[320];
        int n = generate_moves(b, list);
        for (int i = 0; i < n; i++) {
            StateInfo st;
            b.make_move(list[i], st);
            bool legal = !is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b);
            b.unmake_move(list[i], st);
            if (legal) { result.best_move = list[i]; break; }
        }
    }

    return result;
}
