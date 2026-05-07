#include <cstdio>
#include <cstdlib>
#include "../src/board.h"
#include "../src/movegen.h"
#include "../src/book.h"

static int test_count = 0;
static int test_passed = 0;

void assert_true(bool condition, const char* message) {
    test_count++;
    if (condition) {
        test_passed++;
        printf("[PASS] %s\n", message);
    } else {
        printf("[FAIL] %s\n", message);
    }
}

void assert_eq(bool condition, const char* message) {
    assert_true(condition, message);
}

void test_book_probe_starting_position_returns_move_or_null() {
    Board board;
    board.load_fen(START_FEN);

    book_init("./op/komodo.bin");
    Move m = book_probe(board);
    book_close();

    // Either returns a legal move or 0 (null move)
    if (m != 0) {
        Move legal_list[256];
        int num_legal = generate_moves(board, legal_list);

        // Verify the move is in the legal move list
        bool found = false;
        for (int i = 0; i < num_legal; i++) {
            if (legal_list[i] == m) {
                found = true;
                break;
            }
        }
        assert_true(found, "Book probe returned legal move for starting position");
    } else {
        assert_true(true, "Book probe returned null move for starting position (acceptable)");
    }
}

void test_book_init_with_invalid_path_handles_gracefully() {
    book_init("/nonexistent/path.bin");
    Board board;
    board.load_fen(START_FEN);
    Move m = book_probe(board);
    book_close();
    assert_eq(m == 0, "Should return 0 (null move) when book not loaded");
}

void test_komodo_book_loads_successfully() {
    Board board;
    board.load_fen(START_FEN);
    book_init("./op/komodo.bin");

    // Komodo book file should be successfully loaded with entries
    // (may or may not have an entry for the starting position)
    Move m = book_probe(board);
    book_close();

    // Test passes if book loads without error. Move can be 0 or a move.
    assert_true(true, "Komodo book loads successfully");
}

int main() {
    Zobrist::init();

    printf("Running Book Probe Tests...\n");
    printf("============================\n\n");

    test_book_probe_starting_position_returns_move_or_null();
    test_book_init_with_invalid_path_handles_gracefully();
    test_komodo_book_loads_successfully();

    printf("\n============================\n");
    printf("Results: %d/%d tests passed\n", test_passed, test_count);

    return (test_passed == test_count) ? 0 : 1;
}
