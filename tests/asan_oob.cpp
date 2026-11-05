// Isolated educational experiment. NEVER linked into Ember.
// asan_oob is EXCLUDE_FROM_ALL; bounds_fixed compiles the safe branch.
#include "memory.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    Byte data[MEM_SIZE]{};
    const std::size_t index = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : MEM_SIZE;
#ifdef EMBER_ASAN_DEMO_UNSAFE
    data[index] = 1; // Intentional one-past-end write: ASan must stop here.
#else
    if (index >= MEM_SIZE) {
        std::cout << "rejected index " << index << ": valid range 0.." << MEM_SIZE - 1 << '\n';
        return 0;
    }
    data[index] = 1;
#endif
    std::cout << "stored " << static_cast<unsigned>(data[index]) << '\n';
    return 0;
}
