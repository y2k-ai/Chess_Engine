#pragma once
#include "board.h"
#include <string>

void book_init(const std::string& path);
Move book_probe(const Board& b);
void book_close();
