#pragma once
#include "board.h"

int  generate_moves(const Board& b, Move* list);
int  generate_captures(const Board& b, Move* list);
bool is_attacked(int sq, Color by, Bitboard occ, const Board& b);
int  king_sq(const Board& b, Color c);
