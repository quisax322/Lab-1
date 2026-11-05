#include "memory.hpp"

Byte mem_get(const Memory& mem, std::size_t addr) {
    if (addr >= MEM_SIZE) {
        return 0;
    }
    return mem.data[addr];
}

bool mem_set(Memory& mem, std::size_t addr, Byte value) {
    if (addr >= MEM_SIZE) {
        return false;
    }
    mem.data[addr] = value;
    return true;
}

std::uint16_t get16(const Memory& mem, std::size_t addr) {
    // Guard BEFORE addr + 1, including SIZE_MAX (no unsigned overflow).
    if (addr >= MEM_SIZE - 1) {
        return 0;
    }
    // Given Lab 03 algorithm: low byte first, then high byte.
    Byte lo = mem_get(mem, addr);
    Byte hi = mem_get(mem, addr + 1);
    return (std::uint16_t)(lo | (hi << 8));
}

bool set16(Memory& mem, std::size_t addr, std::uint16_t value) {
    if (addr >= MEM_SIZE - 1) {
        return false;
    }
    mem_set(mem, addr, static_cast<Byte>(value & 0xFFu));
    mem_set(mem, addr + 1, static_cast<Byte>(value >> 8));
    return true;
}
