#include "tablebase.h"
#include <cstring>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Global state
static bool tb_enabled = false;
static std::string tb_path;

bool tb_init(const std::string& path) {
    tb_path = path;

    // Check if directory exists
    if (!fs::exists(path)) {
        tb_enabled = false;
        return false;
    }

    // Scan for tablebase files (.rtbz, .rtbw)
    int tb_count = 0;
    for (const auto& entry : fs::directory_iterator(path)) {
        std::string ext = entry.path().extension().string();
        if (ext == ".rtbz" || ext == ".rtbw") {
            tb_count++;
        }
    }

    tb_enabled = (tb_count > 0);
    if (tb_enabled) {
        std::cout << "Tablebase: loaded " << tb_count << " files from " << path << std::endl;
    }

    return tb_enabled;
}

void tb_close() {
    tb_enabled = false;
    tb_path.clear();
}

int tb_probe_wdl(const Board& b) {
    // Stub: no tablebase files loaded yet
    // Return INT_MIN to signal "not in tablebase, use search"
    if (!tb_enabled) {
        return INT_MIN;
    }

    // TODO: Implement Syzygy format reading
    // For now, always fallback to search
    return INT_MIN;
}
