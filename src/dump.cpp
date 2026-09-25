#include "dump.hpp"

#include <iomanip>
#include <iostream>

const std::size_t BYTES_PER_LINE = 16;

static bool is_printable(Byte b) { return b >= 0x20 && b <= 0x7E; }

void dump(const Memory& mem) {
    for (std::size_t row = 0; row < MEM_SIZE; row += BYTES_PER_LINE) {

        std::cout << std::hex << std::setfill('0') << std::setw(4) << row << "  ";

        for (std::size_t col = 0; col < BYTES_PER_LINE; ++col) {
            Byte b = mem.data[row + col];
            std::cout << std::setw(2) << (int)b << ' ';
        }

        std::cout << " |";

        for (std::size_t col = 0; col < BYTES_PER_LINE; ++col) {
            Byte b = mem.data[row + col];
            if (is_printable(b)) {
                std::cout << b;
            } else {
                std::cout << '.';
            }
        }

        std::cout << "|\n";
    }

    std::cout << std::dec << std::setfill(' ');
}

void show_byte(Byte b) {
    std::cout << std::dec << (int)b << "  ";

    std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << (int)b << "  ";

    std::cout << "0b";
    for (int i = 7; i >= 0; --i) {
        std::cout << ((b >> i) & 1);
    }
    std::cout << "  ";

    if (is_printable(b)) {
        std::cout << "'" << b << "'\n";
    } else {
        std::cout << "'.'\n";
    }

    std::cout << std::dec << std::setfill(' ');
}