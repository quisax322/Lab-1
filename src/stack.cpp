#include "stack.hpp"

std::uint16_t Stack::sp() const {
    return sp_;
}

bool Stack::push(Memory& mem, Byte value) {
    if (sp_ < STACK_LO) return false;
    mem_set(mem, sp_, value);
    --sp_;
    return true;
}

bool Stack::pop(const Memory& mem, Byte& value) {
    if (!peek(mem, 0, value)) return false;
    ++sp_;
    return true;
}

bool Stack::push_address(Memory& mem, std::uint16_t address) {
    if (sp_ < STACK_LO + 1) return false;
    push(mem, static_cast<Byte>(address & 0xFFu));
    push(mem, static_cast<Byte>(address >> 8));
    return true;
}

bool Stack::pop_address(const Memory& mem, std::uint16_t& address) {
    Byte hi = 0, lo = 0;
    if (!peek(mem, 0, hi) || !peek(mem, 1, lo)) return false;
    address = static_cast<std::uint16_t>((hi << 8) | lo);
    sp_ += 2;
    return true;
}

bool Stack::peek(const Memory& mem, std::size_t offset, Byte& value) const {
    if (offset >= static_cast<std::size_t>(STACK_HI - sp_)) return false;
    value = mem_get(mem, sp_ + 1 + offset);
    return true;
}
