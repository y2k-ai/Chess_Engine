#pragma once
#include "board.h"

// Syzygy tablebase interface
// Probe-only support; returns WDL (wins/draws/losses) for endgame positions

// Initialize tablebase from directory path
// Returns true if any tablebases found, false if directory empty/invalid
bool tb_init(const std::string& path);

// Probe position in tablebase
// Returns: 1 (win), 0 (draw), -1 (loss) relative to side-to-move
// Returns INT_MIN if position not in tablebase (fallback to search)
int tb_probe_wdl(const Board& b);

// Close tablebase (cleanup)
void tb_close();
