#include "uci.h"
#include "board.h"
#include "magic.h"
#include "movegen.h"
#include "search.h"
#include <iostream>
#include <sstream>
#include <string>

// ── Move string conversion ────────────────────────────────────────────────
static std::string move_to_str(Move m) {
    int from = move_from(m), to = move_to(m);
    char buf[6];
    buf[0] = 'a' + from % 8; buf[1] = '1' + from / 8;
    buf[2] = 'a' + to   % 8; buf[3] = '1' + to   / 8;
    if (move_flags(m) & FLAG_PROMO) {
        buf[4] = "pnbrqk"[move_promo(m)]; buf[5] = 0;
    } else { buf[4] = 0; }
    return buf;
}

static Move str_to_move(const std::string& s, Board& b) {
    Move list[320];
    int n = generate_moves(b, list);
    StateInfo st;
    for (int i = 0; i < n; i++) {
        Move m = list[i];
        b.make_move(m, st);
        bool legal = !is_attacked(king_sq(b, Color(1 - b.side)), b.side, b.occupancy[2], b);
        b.unmake_move(m, st);
        if (legal && move_to_str(m) == s) return m;
    }
    return 0;
}

// ── Position parser ───────────────────────────────────────────────────────
static void parse_position(const std::string& line, Board& b) {
    if (line.find("startpos") != std::string::npos) {
        b.load_fen(START_FEN);
    } else {
        size_t fen_pos = line.find("fen ");
        if (fen_pos == std::string::npos) return;
        size_t mv_pos = line.find(" moves", fen_pos);
        std::string fen = (mv_pos != std::string::npos)
            ? line.substr(fen_pos + 4, mv_pos - fen_pos - 4)
            : line.substr(fen_pos + 4);
        b.load_fen(fen);
    }
    size_t mv_pos = line.find(" moves ");
    if (mv_pos == std::string::npos) return;
    std::istringstream ss(line.substr(mv_pos + 7));
    std::string tok;
    while (ss >> tok) {
        Move m = str_to_move(tok, b);
        if (m) {
            StateInfo st;
            b.make_move(m, st);
        }
    }
}

// ── Go parser ─────────────────────────────────────────────────────────────
static void parse_go(const std::string& line, Board& b) {
    SearchLimits lim;
    std::istringstream ss(line.substr(2));
    std::string tok;
    while (ss >> tok) {
        if      (tok == "depth")    ss >> lim.depth;
        else if (tok == "movetime") ss >> lim.movetime;
        else if (tok == "wtime")    ss >> lim.wtime;
        else if (tok == "btime")    ss >> lim.btime;
        else if (tok == "winc")     ss >> lim.winc;
        else if (tok == "binc")     ss >> lim.binc;
        else if (tok == "infinite") lim.infinite = true;
    }
    SearchResult res = search(b, lim);
    std::cout << "bestmove " << move_to_str(res.best_move) << "\n";
    std::cout.flush();
}

// ── Main UCI loop ─────────────────────────────────────────────────────────
void uci_loop() {
    Board b;
    b.load_fen(START_FEN);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line == "uci") {
            std::cout << "id name ChessEngine\n"
                      << "id author Chess Engine Developer\n"
                      << "uciok\n";
            std::cout.flush();
        } else if (line == "isready") {
            std::cout << "readyok\n";
            std::cout.flush();
        } else if (line == "ucinewgame") {
            b.load_fen(START_FEN);
            tt_clear();
        } else if (line.substr(0, 8) == "position") {
            parse_position(line, b);
        } else if (line.substr(0, 2) == "go") {
            parse_go(line, b);
        } else if (line == "stop") {
            stop_search.store(true, std::memory_order_relaxed);
        } else if (line == "quit") {
            break;
        }
    }
}
