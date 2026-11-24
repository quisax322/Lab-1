#include "cpu.hpp"

#include <iomanip>
#include <iostream>

#include "alu.hpp"
#include "display.hpp"

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
    const Op op = static_cast<Op>(mem_get(*cpu.mem, cpu.pc));
    const auto length = instruction_size(op);
    if (length > MEM_SIZE - cpu.pc) {
        fault(cpu, "instruction operand outside memory");
        return;
    }
    std::uint16_t address = 0;
    if (op >= Op::LoadA && op <= Op::StoreB) {
        address = get16(*cpu.mem, cpu.pc + 1);
    } else if (op == Op::LoadAFromH || op == Op::StoreAToH) {
        address = cpu.h;
    }
    if (address >= MEM_SIZE) {
        fault(cpu, "data address outside memory");
        return;
    }
    // Taken branches replace the sequential PC, without adding their size again.
    auto next_pc = static_cast<std::uint16_t>(cpu.pc + length);
    switch (op) {
    case Op::Halt: cpu.halted = true; break;
    case Op::Nop: break;
    case Op::Out: std::cout.put(static_cast<char>(cpu.a)); break;
    case Op::Outn: {
        const auto saved_flags = std::cout.flags();
        std::cout << std::dec << static_cast<unsigned>(cpu.a) << ' ';
        std::cout.flags(saved_flags);
        break;
    }
    case Op::Outs: {
        std::size_t addr = get16(*cpu.mem, cpu.pc + 1);
        constexpr std::size_t max_bytes = 256;
        for (std::size_t count = 0; count < max_bytes && addr < MEM_SIZE; ++count, ++addr) {
            const Byte value = mem_get(*cpu.mem, addr);
            if (value == 0) break;
            std::cout.put(static_cast<char>(value));
        }
        break;
    }
    case Op::Add: cpu.a = alu_add(cpu.a, cpu.b, cpu.f); break;
    case Op::Sub: cpu.a = alu_sub(cpu.a, cpu.b, cpu.f); break;
    case Op::And: cpu.a = alu_and(cpu.a, cpu.b, cpu.f); break;
    case Op::Or:  cpu.a = alu_or(cpu.a, cpu.b, cpu.f); break;
    case Op::Xor: cpu.a = alu_xor(cpu.a, cpu.b, cpu.f); break;
    case Op::Not: cpu.a = alu_not(cpu.a, cpu.f); break;
    case Op::Shl: cpu.a = alu_shl(cpu.a, cpu.f); break;
    case Op::Shr: cpu.a = alu_shr(cpu.a, cpu.f); break;
    case Op::Inc: cpu.a = alu_inc(cpu.a, cpu.f); break;
    case Op::Dec: cpu.a = alu_dec(cpu.a, cpu.f); break;
    case Op::Cmp: (void)alu_sub(cpu.a, cpu.b, cpu.f); break;
    case Op::Mul: cpu.a = alu_mul(cpu.a, cpu.b, cpu.f); break;
    case Op::LoadiA: cpu.a = mem_get(*cpu.mem, cpu.pc + 1); break;
    case Op::LoadiB: cpu.b = mem_get(*cpu.mem, cpu.pc + 1); break;
    case Op::LoadA: cpu.a = mem_get(*cpu.mem, address); break;
    case Op::LoadB: cpu.b = mem_get(*cpu.mem, address); break;
    case Op::StoreA: mem_set(*cpu.mem, address, cpu.a); break;
    case Op::StoreB: mem_set(*cpu.mem, address, cpu.b); break;
    case Op::MovAB: cpu.a = cpu.b; break;
    case Op::MovBA: cpu.b = cpu.a; break;
    case Op::LoadH: cpu.h = get16(*cpu.mem, cpu.pc + 1); break;
    case Op::LoadAFromH: cpu.a = mem_get(*cpu.mem, address); break;
    case Op::StoreAToH: mem_set(*cpu.mem, address, cpu.a); break;
    case Op::IncH: cpu.h = static_cast<std::uint16_t>(cpu.h + 1); break;
    case Op::DecH: cpu.h = static_cast<std::uint16_t>(cpu.h - 1); break;
    case Op::HLow: cpu.a = static_cast<Byte>(cpu.h & 0xFFu); break;
    case Op::Jmp:
    case Op::Jz:
    case Op::Jnz: {
        const auto target = get16(*cpu.mem, cpu.pc + 1);
        const bool taken = op == Op::Jmp || (op == Op::Jz && cpu.f.z) ||
                           (op == Op::Jnz && !cpu.f.z);
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
    case Op::Cls: clear(*cpu.mem); break;
    case Op::Plot: cpu.f.c = !plot(*cpu.mem, cpu.a, cpu.b); break;
    case Op::Show: show(*cpu.mem); break;
    case Op::PushA:
    case Op::PushB:
        if (!cpu.stack.push(*cpu.mem, op == Op::PushA ? cpu.a : cpu.b)) {
            fault(cpu, "stack overflow");
            return;
        }
        break;
    case Op::PopA:
    case Op::PopB:
        if (!cpu.stack.pop(*cpu.mem, op == Op::PopA ? cpu.a : cpu.b)) {
            fault(cpu, "stack underflow");
            return;
        }
        break;
    case Op::Call: {
        const auto target = get16(*cpu.mem, cpu.pc + 1);
        if (target >= MEM_SIZE) {
            fault(cpu, "call target outside memory");
            return;
        }
        if (!cpu.stack.push_address(*cpu.mem, next_pc)) {
            fault(cpu, "stack overflow");
            return;
        }
        next_pc = target;
        break;
    }
    case Op::Ret: {
        // Commit the popped SP only after checking the guest return address.
        Stack after_return = cpu.stack;
        if (!after_return.pop_address(*cpu.mem, next_pc)) {
            fault(cpu, "stack underflow");
            return;
        }
        if (next_pc >= MEM_SIZE) {
            fault(cpu, "return target outside memory");
            return;
        }
        cpu.stack = after_return;
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
              << " H=" << cpu.h
              << " SP=0x" << std::hex << std::uppercase << cpu.stack.sp() << '\n';
    std::cout.flags(saved_flags);
}

void dump_stack(const CPU& cpu) {
    const auto saved_flags = std::cout.flags();
    const auto saved_fill = std::cout.fill();
    std::cout << "SP=0x" << std::hex << std::uppercase << cpu.stack.sp();
    Byte value = 0;
    if (cpu.mem && cpu.stack.peek(*cpu.mem, 0, value)) {
        std::cout << " top:";
        for (std::size_t i = 0; i < 8 && cpu.stack.peek(*cpu.mem, i, value); ++i) {
            std::cout << ' ' << std::setfill('0') << std::setw(2) << static_cast<unsigned>(value);
        }
    } else {
        std::cout << " (empty)";
    }
    std::cout << '\n';
    std::cout.flags(saved_flags);
    std::cout.fill(saved_fill);
}
