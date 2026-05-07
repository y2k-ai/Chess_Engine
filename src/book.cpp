#include "book.h"
#include <fstream>
#include <cstring>
#include <random>
#include <iostream>

// ── Global book state ─────────────────────────────────────────────────────
static bool book_enabled = false;
static std::string book_path;
static std::vector<uint8_t> book_data;

struct BookEntry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learns;
};

// ── Initialize and load book ──────────────────────────────────────────────
void book_init(const std::string& path) {
    if (path.empty()) {
        book_close();
        return;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "warning: could not open book file: " << path << "\n";
        return;
    }

    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    book_data.clear();
    book_data.resize((size_t)file_size);
    file.read(reinterpret_cast<char*>(book_data.data()), file_size);

    if (!file) {
        std::cerr << "warning: could not read book file: " << path << "\n";
        book_data.clear();
        return;
    }

    file.close();

    if (book_data.size() % sizeof(BookEntry) != 0) {
        std::cerr << "warning: book file size not multiple of entry size\n";
        book_data.clear();
        return;
    }

    book_enabled = true;
    book_path = path;
    int num_entries = (int)(book_data.size() / sizeof(BookEntry));
    std::cout << "info string Book loaded: " << num_entries << " entries\n";
    std::cout.flush();
}

// ── Close and clear book ──────────────────────────────────────────────────
void book_close() {
    book_enabled = false;
    book_path.clear();
    book_data.clear();
}

// ── Probe book for a position ─────────────────────────────────────────────
Move book_probe(const Board& b) {
    if (!book_enabled || book_data.empty()) return 0;

    uint64_t key = b.hash;
    const BookEntry* entries = reinterpret_cast<const BookEntry*>(book_data.data());
    int num_entries = (int)(book_data.size() / sizeof(BookEntry));

    // Binary search for the key
    int left = 0, right = num_entries - 1;
    int found_idx = -1;

    while (left <= right) {
        int mid = (left + right) / 2;
        if (entries[mid].key == key) {
            found_idx = mid;
            break;
        } else if (entries[mid].key < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (found_idx == -1) return 0;

    // Found at least one entry with this key
    // Could have multiple moves for same position, collect them all
    int first = found_idx;
    while (first > 0 && entries[first - 1].key == key) first--;

    int last = found_idx;
    while (last < num_entries - 1 && entries[last + 1].key == key) last++;

    // Sum weights for weighted random selection
    uint64_t total_weight = 0;
    for (int i = first; i <= last; i++) {
        total_weight += entries[i].weight;
    }

    if (total_weight == 0) return 0;

    // Weighted random selection
    static std::mt19937_64 rng(std::random_device{}());
    uint64_t rnd = rng() % total_weight;
    uint64_t cumul = 0;

    Move selected_move = 0;
    for (int i = first; i <= last; i++) {
        cumul += entries[i].weight;
        if (rnd < cumul) {
            selected_move = entries[i].move;
            break;
        }
    }

    if (selected_move == 0) return 0;

    // Decode Polyglot move: from(6) | to(6) | promo(3) in bits
    int from = (selected_move >> 6) & 0x3F;
    int to = selected_move & 0x3F;
    int promo = (selected_move >> 12) & 0x7;

    // Convert promo: Polyglot uses 0=knight, 1=bishop, 2=rook, 3=queen
    // Internal uses: KNIGHT=1, BISHOP=2, ROOK=3, QUEEN=4
    int promo_piece = NO_PIECE;
    if (promo > 0) {
        promo_piece = promo;  // 1=knight, 2=bishop, 3=rook, 4=queen in our system
    }

    // Create Move using internal representation
    Move m = make_move(from, to, NO_PIECE, NO_PIECE, promo_piece);

    // Sanity check: verify move is legal in current position
    // Quick legality check via piece validation (full generation is expensive)
    int from_piece = b.piece_on(from);
    if (from_piece == NO_PIECE) return 0;
    if (b.color_on(from) != b.side) return 0;

    return m;
}
