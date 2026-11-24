#include "display.hpp"
#include "dump.hpp"

int main() {
    Memory mem;
    clear(mem);
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        plot(mem, x, 0);
        plot(mem, x, SCREEN_HEIGHT - 1);
    }
    for (int y = 1; y < SCREEN_HEIGHT - 1; ++y) {
        plot(mem, 0, y);
        plot(mem, SCREEN_WIDTH - 1, y);
    }
    show(mem);
    dump(mem);
}
