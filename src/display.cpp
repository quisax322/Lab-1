#include "display.hpp"

#include <algorithm>
#include <iostream>

static_assert(VRAM_HI < MEM_SIZE);

// The row-by-row renderer supplied in the lab handout.
void show(const Memory& mem) {
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            Byte cell = mem.data[VRAM_LO + y * SCREEN_ROW_BYTES + (x / 8)];
            bool lit = (cell >> (7 - (x % 8))) & 1;
            std::cout << (lit ? '#' : '.');
        }
        std::cout << '\n';
    }
}

bool plot(Memory& mem, int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return false;
    }
    const auto address = VRAM_LO + y * SCREEN_ROW_BYTES + (x / 8);
    mem.data[address] |= static_cast<Byte>(1u << (7 - (x % 8)));
    return true;
}

void clear(Memory& mem) {
    std::fill(mem.data + VRAM_LO, mem.data + VRAM_HI + 1, Byte{0});
}
