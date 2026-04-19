#include "uci.h"
#include "board.h"
#include "magic.h"

int main() {
    Zobrist::init();
    Magic::init();
    uci_loop();
}
