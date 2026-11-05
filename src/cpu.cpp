#include "cpu.hpp"

#include <iomanip>
#include <iostream>

#include "alu.hpp"

static void fault(CPU& cpu, const char* message) {
    cpu.error = message;
    std::cout << "CPU error: " << message << " (PC=" << cpu.pc << ")\n";
}

void step(CPU& cpu) {
    if (cpu.error != nullptr) return;
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
    } else if ((op >= OP_LOAD_A && op <= OP_STORE_B) || op == OP_LOADH ||
               op == OP_JMP || op == OP_JZ || op == OP_JNZ) {
        length = 3;
    }
    if (length > MEM_SIZE - cpu.pc) {
        fault(cpu, "instruction operand outside memory");
        return;
    }
    std::uint16_t address = 0;
    if (op >= OP_LOAD_A && op <= OP_STORE_B) {
        address = get16(*cpu.mem, cpu.pc + 1);
    } else if (op == OP_LOAD_H || op == OP_STORE_H) {
        address = cpu.h;
    }
    if (address >= MEM_SIZE) {
        fault(cpu, "data address outside memory");
        return;
    }
    // Taken branches replace the sequential PC, without adding their size again.
    auto next_pc = static_cast<std::uint16_t>(cpu.pc + length);
    switch (op) {
    case OP_HALT: cpu.halted = true; break;
    case OP_NOP: break;
    case OP_OUT: std::cout.put(static_cast<char>(cpu.a)); break;
    case OP_OUTN: {
        const auto saved_flags = std::cout.flags();
        std::cout << std::dec << static_cast<unsigned>(cpu.a) << ' ';
        std::cout.flags(saved_flags);
        break;
    }
    case OP_ADD: cpu.a = alu_add(cpu.a, cpu.b, cpu.f); break;
    case OP_SUB: cpu.a = alu_sub(cpu.a, cpu.b, cpu.f); break;
    case OP_AND: cpu.a = alu_and(cpu.a, cpu.b, cpu.f); break;
    case OP_OR:  cpu.a = alu_or(cpu.a, cpu.b, cpu.f); break;
    case OP_XOR: cpu.a = alu_xor(cpu.a, cpu.b, cpu.f); break;
    case OP_NOT: cpu.a = alu_not(cpu.a, cpu.f); break;
    case OP_SHL: cpu.a = alu_shl(cpu.a, cpu.f); break;
    case OP_SHR: cpu.a = alu_shr(cpu.a, cpu.f); break;
    case OP_INC: cpu.a = alu_inc(cpu.a, cpu.f); break;
    case OP_DEC: cpu.a = alu_dec(cpu.a, cpu.f); break;
    case OP_CMP: (void)alu_sub(cpu.a, cpu.b, cpu.f); break;
    case OP_LOADI_A: cpu.a = mem_get(*cpu.mem, cpu.pc + 1); break;
    case OP_LOADI_B: cpu.b = mem_get(*cpu.mem, cpu.pc + 1); break;
    case OP_LOAD_A: cpu.a = mem_get(*cpu.mem, address); break;
    case OP_LOAD_B: cpu.b = mem_get(*cpu.mem, address); break;
    case OP_STORE_A: mem_set(*cpu.mem, address, cpu.a); break;
    case OP_STORE_B: mem_set(*cpu.mem, address, cpu.b); break;
    case OP_MOV_AB: cpu.a = cpu.b; break;
    case OP_MOV_BA: cpu.b = cpu.a; break;
    case OP_LOADH: cpu.h = get16(*cpu.mem, cpu.pc + 1); break;
    case OP_LOAD_H: cpu.a = mem_get(*cpu.mem, address); break;
    case OP_STORE_H: mem_set(*cpu.mem, address, cpu.a); break;
    case OP_INCH: cpu.h = static_cast<std::uint16_t>(cpu.h + 1); break;
    case OP_DECH: cpu.h = static_cast<std::uint16_t>(cpu.h - 1); break;
    case OP_HLOW: cpu.a = static_cast<Byte>(cpu.h & 0xFFu); break;
    case OP_JMP:
    case OP_JZ:
    case OP_JNZ: {
        const auto target = get16(*cpu.mem, cpu.pc + 1);
        const bool taken = op == OP_JMP || (op == OP_JZ && cpu.f.z) ||
                           (op == OP_JNZ && !cpu.f.z);
        // Operand bytes are always checked; an unused target is not dereferenced.
        if (taken) {
            if (target >= MEM_SIZE) {
                fault(cpu, "jump target outside memory");
                return;
            }
            next_pc = target;
        }
        break;
    }
    default: {
        cpu.error = "unknown opcode";
        const auto saved_flags = std::cout.flags();
        const auto saved_fill = std::cout.fill();
        std::cout << "CPU error: unknown opcode 0x" << std::hex << std::nouppercase
                  << std::setfill('0') << std::setw(2) << static_cast<unsigned>(op)
                  << std::dec << " (PC=" << cpu.pc << ")\n";
        std::cout.flags(saved_flags);
        std::cout.fill(saved_fill);
        return;
    }
    }
    cpu.pc = next_pc;
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
              << " H=" << cpu.h << '\n';
    std::cout.flags(saved_flags);
}
