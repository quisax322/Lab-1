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

static void fault(CPU& cpu, const char* message) {
    cpu.error = message;
    std::cout << "CPU error: " << message << " (PC=" << cpu.pc << ")\n";
}

void step(CPU& cpu) {
    if (cpu.error != nullptr) {
        return;
    }
    if (cpu.halted) {
        std::cout << "halted\n";
        return;
    }
    if (cpu.mem == nullptr) {
        fault(cpu, "CPU has no memory");
        return;
    }
    if (cpu.pc >= MEM_SIZE) {
        fault(cpu, "PC outside memory");
        return;
    }

    const Byte op = mem_get(*cpu.mem, cpu.pc);
    std::size_t length = 1;
    if (op == OP_LOADI_A || op == OP_LOADI_B) {
        length = 2;
    } else if ((op >= OP_LOAD_A && op <= OP_STORE_B) || op == OP_LOADH) {
        length = 3;
    }
    if (length > MEM_SIZE - cpu.pc) {
        fault(cpu, "instruction operand outside memory");
        return;
    }

    std::uint16_t address = 0;
    if (op >= OP_LOAD_A && op <= OP_STORE_B) {
        address = get16(*cpu.mem, static_cast<std::size_t>(cpu.pc) + 1);
    } else if (op == OP_LOAD_H || op == OP_STORE_H) {
        address = cpu.h;
    }
    if (address >= MEM_SIZE) {
        fault(cpu, "data address outside memory");
        return;
    }
    const DecodedOpcode instruction = decode(op);
    switch (instruction.group) {
    case 0x0:
        switch (instruction.index) {
        case (OP_HALT & 0x0F):
            cpu.halted = true;
            break;
        case (OP_NOP & 0x0F):
            break;
        case (OP_OUT & 0x0F):
            std::cout.put(static_cast<char>(cpu.a));
            break;
        case (OP_OUTN & 0x0F): {
            const auto saved_flags = std::cout.flags();
            std::cout << std::dec << static_cast<unsigned>(cpu.a) << ' ';
            std::cout.flags(saved_flags);
            break;
        }
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
    case 0x2:
        switch (instruction.index) {
        case (OP_LOADI_A & 0x0F): cpu.a = mem_get(*cpu.mem, cpu.pc + 1); break;
        case (OP_LOADI_B & 0x0F): cpu.b = mem_get(*cpu.mem, cpu.pc + 1); break;
        case (OP_LOAD_A & 0x0F): cpu.a = mem_get(*cpu.mem, address); break;
        case (OP_LOAD_B & 0x0F): cpu.b = mem_get(*cpu.mem, address); break;
        case (OP_STORE_A & 0x0F): mem_set(*cpu.mem, address, cpu.a); break;
        case (OP_STORE_B & 0x0F): mem_set(*cpu.mem, address, cpu.b); break;
        case (OP_MOV_AB & 0x0F): cpu.a = cpu.b; break;
        case (OP_MOV_BA & 0x0F): cpu.b = cpu.a; break;
        case (OP_LOADH & 0x0F):
            cpu.h = get16(*cpu.mem, static_cast<std::size_t>(cpu.pc) + 1);
            break;
        case (OP_LOAD_H & 0x0F): cpu.a = mem_get(*cpu.mem, address); break;
        case (OP_STORE_H & 0x0F): mem_set(*cpu.mem, address, cpu.a); break;
        case (OP_INCH & 0x0F): cpu.h = static_cast<std::uint16_t>(cpu.h + 1); break;
        case (OP_DECH & 0x0F): cpu.h = static_cast<std::uint16_t>(cpu.h - 1); break;
        default: break; // Preserve Lab 02 NOP behavior for unimplemented opcodes.
        }
        break;
    default:
        break;
    }
    cpu.pc = static_cast<std::uint16_t>(cpu.pc + length);
}

bool run(CPU& cpu, std::size_t max_steps) {
    if (cpu.halted) {
        std::cout << "halted\n";
        return true;
    }
    std::size_t executed = 0;
    while (!cpu.halted && cpu.error == nullptr) {
        if (executed == max_steps) {
            fault(cpu, "instruction limit reached");
            break;
        }
        step(cpu);
        ++executed;
    }
    return cpu.halted && cpu.error == nullptr;
}

void dump_regs(const CPU& cpu) {
    const auto saved_flags = std::cout.flags();
    std::cout << std::dec << std::noboolalpha
              << "PC=" << cpu.pc
              << " A=" << static_cast<unsigned>(cpu.a)
              << " B=" << static_cast<unsigned>(cpu.b)
              << " Z=" << cpu.f.z << " N=" << cpu.f.n << " C=" << cpu.f.c
              << " H=" << cpu.h
              << '\n';
    std::cout.flags(saved_flags);
}
