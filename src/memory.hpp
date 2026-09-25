#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <cstddef>
#include <cstdint>

using Byte = std::uint8_t;
const std::size_t MEM_SIZE = 4096;

struct Memory {
    Byte data[MEM_SIZE]{};
};

Byte mem_get(const Memory& mem, std::size_t addr);
bool mem_set(Memory& mem, std::size_t addr, Byte value);

#endif