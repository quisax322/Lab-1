#include "cpu.hpp"

#include <iomanip>
#include <iostream>

#include "alu.hpp"

struct DecodedOpcode {
    Byte group;
    Byte index;
};

static DecodedOpcode decode(Byte op) {
    return {static_cast<Byte>((op >> 4) & 0x0F),
            static_cast<Byte>(op & 0x0F)};
}

void step(CPU& cpu) {
    if (cpu.halted) {
        std::cout << "halted\n";
        return;
    }
    if (cpu.mem == nullptr) {
        std::cout << "CPU has no memory\n";
        return;
    }

    const Byte op = mem_get(*cpu.mem, cpu.pc);
    const DecodedOpcode instruction = decode(op);
    switch (instruction.group) {
    case 0x0:
        switch (instruction.index) {
        case (OP_HALT & 0x0F):
            cpu.halted = true;
            break;
        case (OP_NOP & 0x0F):
            break;
        default:
            break;  // Given Lab 02 scaffold: unknown opcode acts as NOP.
        }
        break;
    case 0x1:
        switch (instruction.index) {
        case (OP_ADD & 0x0F): cpu.a = alu_add(cpu.a, cpu.b, cpu.f); break;
        case (OP_SUB & 0x0F): cpu.a = alu_sub(cpu.a, cpu.b, cpu.f); break;
        case (OP_AND & 0x0F): cpu.a = alu_and(cpu.a, cpu.b, cpu.f); break;
        case (OP_OR  & 0x0F): cpu.a = alu_or(cpu.a, cpu.b, cpu.f); break;
        case (OP_XOR & 0x0F): cpu.a = alu_xor(cpu.a, cpu.b, cpu.f); break;
        case (OP_NOT & 0x0F): cpu.a = alu_not(cpu.a, cpu.f); break;
        case (OP_SHL & 0x0F): cpu.a = alu_shl(cpu.a, cpu.f); break;
        case (OP_SHR & 0x0F): cpu.a = alu_shr(cpu.a, cpu.f); break;
        case (OP_INC & 0x0F): cpu.a = alu_inc(cpu.a, cpu.f); break;
        case (OP_DEC & 0x0F): cpu.a = alu_dec(cpu.a, cpu.f); break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    // All instructions in this lab (including HALT) are one byte long.
    cpu.pc += 1;
}

void dump_regs(const CPU& cpu) {
    const auto saved_flags = std::cout.flags();
    std::cout << std::dec << std::noboolalpha
              << "PC=" << cpu.pc
              << " A=" << static_cast<unsigned>(cpu.a)
              << " B=" << static_cast<unsigned>(cpu.b)
              << " Z=" << cpu.f.z << " N=" << cpu.f.n << " C=" << cpu.f.c
              << '\n';
    std::cout.flags(saved_flags);
}
