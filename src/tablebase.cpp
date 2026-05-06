#include "tablebase.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>

namespace fs = std::filesystem;

// Syzygy format constants
static const uint32_t MAGIC_WDL = 0x71e8e1c3;

// Global state
static bool tb_enabled = false;
static std::string tb_path;
static std::map<std::string, std::vector<uint8_t>> tb_cache; // filename -> file data
static std::unordered_map<uint64_t, int> probe_cache; // position key -> WDL

// Helper: read entire file into memory
static bool load_file(const std::string& filename, std::vector<uint8_t>& data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(size);
    file.read((char*)data.data(), size);
    return file.good();
}

// Helper: count pieces in position
static int count_pieces(const Board& b) {
    int count = 0;
    for (int color = 0; color < 2; color++) {
        for (int piece = 0; piece < 6; piece++) {
            count += __popcnt64(b.pieces[color][piece]);
        }
    }
    return count;
}

// Helper: get material signature string (e.g., "KQvK", "KRvK")
static std::string get_material_signature(const Board& b) {
    int white_pieces[6] = {0}, black_pieces[6] = {0};

    for (int piece = 0; piece < 6; piece++) {
        white_pieces[piece] = __popcnt64(b.pieces[0][piece]);
        black_pieces[piece] = __popcnt64(b.pieces[1][piece]);
    }

    const char* piece_chars = "PNBRQK";
    std::string sig;

    // White pieces (King first, then others)
    sig += "K";
    for (int p = 5; p >= 1; p--) {  // Q, R, B, N, P
        for (int i = 0; i < white_pieces[p]; i++) sig += piece_chars[p];
    }

    sig += "v";

    // Black pieces
    sig += "K";
    for (int p = 5; p >= 1; p--) {
        for (int i = 0; i < black_pieces[p]; i++) sig += piece_chars[p];
    }

    return sig;
}

bool tb_init(const std::string& path) {
    tb_path = path;

    // Check if directory exists
    if (!fs::exists(path)) {
        tb_enabled = false;
        return false;
    }

    // Try WDL subdirectory first (most common)
    std::string wdl_path = path + "/Syzygy345WDL";
    if (!fs::exists(wdl_path)) {
        wdl_path = path; // Fall back to main directory
    }

    // Scan for .rtbw files (uncompressed Syzygy)
    int tb_count = 0;
    for (const auto& entry : fs::directory_iterator(wdl_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".rtbw") {
            tb_count++;
        }
    }

    tb_enabled = (tb_count > 0);
    tb_path = wdl_path;

    if (tb_enabled) {
        std::cout << "Tablebase: found " << tb_count << " .rtbw files in " << wdl_path << std::endl;
    }

    return tb_enabled;
}

void tb_close() {
    tb_enabled = false;
    tb_path.clear();
    tb_cache.clear();
    probe_cache.clear();
}

int tb_probe_wdl(const Board& b) {
    if (!tb_enabled) return INT_MIN;

    // Check probe cache
    uint64_t key = b.hash; // Use board hash as cache key
    auto it = probe_cache.find(key);
    if (it != probe_cache.end()) {
        return it->second;
    }

    // Get material signature
    std::string sig = get_material_signature(b);
    int piece_count = count_pieces(b);

    // Only probe if <=5 pieces (we have Syzygy345 files)
    if (piece_count > 5) {
        return INT_MIN;
    }

    // Try to load tablebase file
    std::string filename = tb_path + "/" + sig + ".rtbw";
    std::vector<uint8_t>* file_data = nullptr;

    // Check cache
    auto cache_it = tb_cache.find(sig);
    if (cache_it != tb_cache.end()) {
        file_data = &cache_it->second;
    } else {
        // Try to load file
        std::vector<uint8_t> data;
        if (!load_file(filename, data)) {
            // File not found or failed to read
            probe_cache[key] = INT_MIN;
            return INT_MIN;
        }

        // Verify magic bytes
        if (data.size() < 4 || *(uint32_t*)data.data() != MAGIC_WDL) {
            // Invalid file format
            probe_cache[key] = INT_MIN;
            return INT_MIN;
        }

        // Store in cache
        tb_cache[sig] = data;
        file_data = &tb_cache[sig];
    }

    // TODO: Implement actual Syzygy format parsing and WDL extraction
    // For now, return INT_MIN as fallback while file loading works
    probe_cache[key] = INT_MIN;
    return INT_MIN;
}
