#pragma once

#include "memory.hpp"

class Stack {
public:
    std::uint16_t sp() const;
    bool push(Memory& mem, Byte value);
    bool pop(const Memory& mem, Byte& value);
    bool push_address(Memory& mem, std::uint16_t address);
    bool pop_address(const Memory& mem, std::uint16_t& address);
    bool peek(const Memory& mem, std::size_t offset, Byte& value) const;

private:
    static constexpr std::uint16_t STACK_LO = 0xF00;
    static constexpr std::uint16_t STACK_HI = 0xFFF;
    std::uint16_t sp_ = STACK_HI;
};
