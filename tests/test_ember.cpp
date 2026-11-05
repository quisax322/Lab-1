#include "alu.hpp"
#include "cpu.hpp"
#include "memory.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

static unsigned checks = 0;

static void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

static void result_is(Byte result, const Flags& f, unsigned expected, bool carry) {
    require(static_cast<unsigned>(result) == expected, "ALU result");
    require(f.z == (expected == 0), "Z flag");
    require(f.n == (expected >= 128), "N flag");
    require(f.c == carry, "C flag");
}

static void test_alu() {
    // Exhaust all byte pairs, with both initial values of C. Arithmetic
    // expectations use modulo/division rather than the ALU's masks/casts.
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned initial_c = 0; initial_c < 2; ++initial_c) {
            const Byte x = static_cast<Byte>(a);
            Flags f{true, true, initial_c != 0};
            Byte r = alu_not(x, f);
            result_is(r, f, 255 - a, false);
            f = {true, true, initial_c != 0};
            r = alu_shl(x, f);
            result_is(r, f, (a * 2) % 256, a >= 128);
            f = {true, true, initial_c != 0};
            r = alu_shr(x, f);
            result_is(r, f, a / 2, a % 2 != 0);
            f = {true, true, initial_c != 0};
            r = alu_inc(x, f);
            result_is(r, f, (a + 1) % 256, initial_c != 0);
            f = {true, true, initial_c != 0};
            r = alu_dec(x, f);
            result_is(r, f, (a + 255) % 256, initial_c != 0);

            for (unsigned b = 0; b < 256; ++b) {
                const Byte y = static_cast<Byte>(b);
                f = {true, true, initial_c != 0};
                r = alu_add(x, y, f);
                result_is(r, f, (a + b) % 256, a + b >= 256);
                f = {true, true, initial_c != 0};
                r = alu_sub(x, y, f);
                result_is(r, f, (a + 256 - b) % 256, a < b);
                f = {true, true, initial_c != 0};
                r = alu_and(x, y, f);
                result_is(r, f, a & b, false);
                f = {true, true, initial_c != 0};
                r = alu_or(x, y, f);
                result_is(r, f, a | b, false);
                f = {true, true, initial_c != 0};
                r = alu_xor(x, y, f);
                result_is(r, f, a ^ b, false);
            }
        }
    }

    Flags f;
    const Byte sum = alu_add(200, 100, f);
    std::cout << "ADD 200 100: result=" << static_cast<unsigned>(sum)
              << "  Z=" << f.z << " N=" << f.n << " C=" << f.c << '\n';
    const Byte masked = alu_and(7, 1, f);
    std::cout << "AND 7 1: result=" << static_cast<unsigned>(masked)
              << "  Z=" << f.z << " N=" << f.n << " C=" << f.c << '\n';
    const Byte shifted = alu_shl(128, f);
    std::cout << "SHL 128: result=" << static_cast<unsigned>(shifted)
              << "  Z=" << f.z << " N=" << f.n << " C=" << f.c << '\n';
}

static void test_cpu() {
    // Preserve Lab 02 opcodes; unknown-as-NOP is superseded by Lab 04 M1.
    // New Lab 03 opcodes have separate tests with multi-byte operands.
    for (unsigned op = 0; op < 256; ++op) {
        if (!(op == 0x00 || op == 0x01 || (op >= 0x10 && op <= 0x19))) {
            continue;
        }
        for (unsigned initial_flags = 0; initial_flags < 8; ++initial_flags) {
            Memory mem;
            mem.data[31] = static_cast<Byte>(op);
            CPU cpu;
            cpu.mem = &mem;
            cpu.pc = 31;
            cpu.a = 200;
            cpu.b = 100;
            cpu.f = {initial_flags % 2 != 0, (initial_flags / 2) % 2 != 0,
                     initial_flags >= 4};
            unsigned expected_a = 200;
            Flags expected_f = cpu.f;
            bool alu = true;
            switch (op) {
            case 0x10: expected_a = 44; expected_f.c = true; break;
            case 0x11: expected_a = 100; expected_f.c = false; break;
            case 0x12: expected_a = 64; expected_f.c = false; break;
            case 0x13: expected_a = 236; expected_f.c = false; break;
            case 0x14: expected_a = 172; expected_f.c = false; break;
            case 0x15: expected_a = 55; expected_f.c = false; break;
            case 0x16: expected_a = 144; expected_f.c = true; break;
            case 0x17: expected_a = 100; expected_f.c = false; break;
            case 0x18: expected_a = 201; break;
            case 0x19: expected_a = 199; break;
            default: alu = false; break;
            }
            if (alu) {
                expected_f.z = (expected_a == 0);
                expected_f.n = (expected_a >= 128);
            }
            step(cpu);
            require(cpu.pc == 32, "one-byte PC increment, including HALT");
            require(cpu.a == expected_a && cpu.b == 100, "CPU register routing");
            require(cpu.f.z == expected_f.z && cpu.f.n == expected_f.n &&
                    cpu.f.c == expected_f.c, "CPU flag routing/preservation");
            require(cpu.halted == (op == 0), "only HALT stops CPU");
            require(mem.data[31] == op, "CPU must not overwrite code");
        }
    }

    Memory mem;
    mem.data[0] = OP_SHL;
    mem.data[1] = OP_SHL;
    mem.data[2] = OP_INC;
    mem.data[3] = OP_HALT;
    mem.data[4] = OP_DEC;
    CPU cpu;
    cpu.mem = &mem;
    cpu.a = 64;
    step(cpu);
    result_is(cpu.a, cpu.f, 128, false);
    step(cpu);
    result_is(cpu.a, cpu.f, 0, true);
    step(cpu);
    result_is(cpu.a, cpu.f, 1, true);
    step(cpu);
    require(cpu.halted && cpu.pc == 4, "HALT completes its own step");

    std::ostringstream output;
    auto* saved = std::cout.rdbuf(output.rdbuf());
    step(cpu);
    step(cpu);
    std::cout.rdbuf(saved);
    require(output.str() == "halted\nhalted\n", "repeated step refuses after HALT");
    require(cpu.pc == 4 && cpu.a == 1 && !cpu.f.z && !cpu.f.n && cpu.f.c,
            "HALT freezes PC/registers/flags, next DEC is not executed");

    // A NOP at the final valid address does not access past Memory's array.
    CPU last;
    last.mem = &mem;
    last.pc = static_cast<std::uint16_t>(MEM_SIZE - 1);
    mem.data[MEM_SIZE - 1] = OP_NOP;
    step(last);
    require(last.pc == MEM_SIZE && !last.halted, "last-address NOP");

    CPU detached;
    saved = std::cout.rdbuf(output.rdbuf());
    step(detached);
    std::cout.rdbuf(saved);
    require(detached.pc == 0 && !detached.halted, "missing memory is safe");
}

static void test_memory() {
    Memory mem;
    for (std::size_t address = 0; address < MEM_SIZE; ++address) {
        require(mem_get(mem, address) == 0, "initial memory must be zero");
    }
    require(mem_set(mem, 0, 65), "first address writable");
    require(mem_set(mem, MEM_SIZE - 1, 255), "last address writable");
    require(!mem_set(mem, MEM_SIZE, 1), "one-past-end write refused");
    require(!mem_set(mem, std::numeric_limits<std::size_t>::max(), 1),
            "largest address write refused");
    require(mem_get(mem, MEM_SIZE) == 0, "out-of-bounds read returns zero");
    require(mem_get(mem, std::numeric_limits<std::size_t>::max()) == 0,
            "largest address read returns zero");
    require(mem_get(mem, 0) == 65 && mem_get(mem, MEM_SIZE - 1) == 255,
            "invalid writes preserve valid memory");
}

int main() {
    test_memory();
    test_alu();
    test_cpu();
    std::cout << "PASS: " << checks << " checks\n";
}
