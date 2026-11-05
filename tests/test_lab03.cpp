#include "cpu.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

static unsigned checks = 0;
static void require(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

static void test_words() {
    Memory mem;
    for (unsigned value = 0; value <= 65535; ++value) {
        for (std::size_t address : {std::size_t{0}, MEM_SIZE - 2}) {
            require(set16(mem, address, static_cast<std::uint16_t>(value)), "valid word write");
            require(mem.data[address] == value % 256, "low byte first");
            require(mem.data[address + 1] == value / 256, "high byte second");
            require(get16(mem, address) == value, "word roundtrip");
        }
    }
    for (std::size_t address : {MEM_SIZE - 1, MEM_SIZE,
                               std::numeric_limits<std::size_t>::max()}) {
        const Memory before = mem;
        require(!set16(mem, address, 0x1234), "invalid word write refused");
        require(get16(mem, address) == 0, "invalid word read returns zero");
        for (std::size_t i = 0; i < MEM_SIZE; ++i) {
            require(mem.data[i] == before.data[i], "word failure is atomic");
        }
    }
}

static void test_instructions(std::ostringstream& output) {
    // Numeric opcodes and independent expectations from the ISA table.
    const unsigned ops[] = {0x02, 0x03, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                            0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C};
    for (unsigned op : ops) {
        const unsigned length = (op == 0x20 || op == 0x21) ? 2 :
            ((op >= 0x22 && op <= 0x25) || op == 0x28) ? 3 : 1;
        for (unsigned flags = 0; flags < 8; ++flags) {
            for (unsigned value : {0u, 128u, 255u}) {
                for (std::size_t pc : {std::size_t{0}, MEM_SIZE - length}) {
                    Memory mem;
                    const unsigned addr = pc == 0 ? 4095 : 0;
                    mem.data[addr] = 90;
                    mem.data[pc] = static_cast<Byte>(op);
                    if (length == 2) mem.data[pc + 1] = static_cast<Byte>(value);
                    if (length == 3) set16(mem, pc + 1, static_cast<std::uint16_t>(addr));
                    CPU cpu;
                    cpu.mem = &mem;
                    cpu.pc = static_cast<std::uint16_t>(pc);
                    cpu.h = static_cast<std::uint16_t>(op == 0x28 ? addr + 1 : addr);
                    const unsigned initial_a = (value + 1) % 256;
                    cpu.a = static_cast<Byte>(initial_a);
                    cpu.b = 67;
                    cpu.f = {(flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0};
                    const Flags original = cpu.f;
                    unsigned a = initial_a, b = 67, h = cpu.h, cell = 90;
                    std::string expected_output;
                    switch (op) {
                    case 0x02: expected_output.assign(1, static_cast<char>(initial_a)); break;
                    case 0x03: expected_output = std::to_string(initial_a) + " "; break;
                    case 0x20: a = value; break;
                    case 0x21: b = value; break;
                    case 0x22: case 0x29: a = 90; break;
                    case 0x23: b = 90; break;
                    case 0x24: case 0x2A: cell = initial_a; break;
                    case 0x25: cell = 67; break;
                    case 0x26: a = 67; break;
                    case 0x27: b = initial_a; break;
                    case 0x28: h = addr; break;
                    case 0x2B: h = addr + 1; break;
                    case 0x2C: h = (addr + 65535) % 65536; break;
                    }
                    output.str("");
                    step(cpu);
                    require(cpu.error == nullptr && !cpu.halted, "successful Lab 03 instruction");
                    require(cpu.pc == pc + length, "ISA length, also at memory end");
                    require(cpu.a == a && cpu.b == b && cpu.h == h, "register result");
                    require(mem.data[addr] == cell, "correct memory direction");
                    require(cpu.f.z == original.z && cpu.f.n == original.n &&
                            cpu.f.c == original.c, "all Lab 03 flags preserved");
                    require(output.str() == expected_output, "exact OUT/OUTN bytes");
                }
            }
        }
        // Missing operands must not partially update any state or memory.
        for (unsigned available = 1; available < length; ++available) {
            Memory mem;
            const auto pc = MEM_SIZE - available;
            mem.data[pc] = static_cast<Byte>(op);
            CPU cpu;
            cpu.mem = &mem;
            cpu.pc = static_cast<std::uint16_t>(pc);
            cpu.a = 65; cpu.b = 66; cpu.h = 0x800;
            step(cpu);
            require(cpu.error != nullptr && cpu.pc == pc, "truncated operand fault");
            require(cpu.a == 65 && cpu.b == 66 && cpu.h == 0x800, "fault preserves registers");
            require(mem.data[0] == 0 && mem.data[pc] == op, "fault preserves memory");
        }
    }
    // H holds 16 bits; range is checked on dereference, not on assignment.
    Memory mem;
    mem.data[0] = 0x28; set16(mem, 1, 0xFFFF);
    mem.data[3] = 0x2B; mem.data[4] = 0x2C;
    CPU cpu; cpu.mem = &mem;
    step(cpu); require(cpu.h == 65535, "LOADH full 16-bit immediate");
    step(cpu); require(cpu.h == 0, "INCH uint16 wrap");
    step(cpu); require(cpu.h == 65535, "DECH uint16 wrap");
}

static void test_faults_and_run(std::ostringstream& output) {
    for (unsigned op : {0x22u, 0x23u, 0x24u, 0x25u, 0x29u, 0x2Au}) {
        for (unsigned address : {4096u, 65535u}) {
            Memory mem;
            mem.data[0] = static_cast<Byte>(op);
            set16(mem, 1, static_cast<std::uint16_t>(address));
            CPU cpu; cpu.mem = &mem; cpu.a = 65; cpu.b = 66;
            cpu.h = static_cast<std::uint16_t>(address);
            require(!run(cpu), "run stops on invalid data address");
            require(cpu.error != nullptr && cpu.pc == 0 && !cpu.halted, "fault distinct from HALT");
            require(cpu.a == 65 && cpu.b == 66 && mem.data[0] == op, "fault atomicity");
            output.str("");
            step(cpu);
            require(!run(cpu) && output.str().empty(), "latched fault prevents execution");
        }
    }
    Memory mem;
    mem.data[MEM_SIZE - 1] = 1; // final valid NOP, then invalid fetch
    CPU cpu; cpu.mem = &mem; cpu.pc = static_cast<std::uint16_t>(MEM_SIZE - 1);
    require(!run(cpu) && cpu.pc == MEM_SIZE, "invalid fetch must not become implicit HALT");
    for (unsigned pc : {4096u, 65535u}) {
        CPU bad; bad.mem = &mem; bad.pc = static_cast<std::uint16_t>(pc);
        require(!run(bad) && bad.pc == pc && bad.error != nullptr, "invalid PC checked first");
    }
    mem.data[MEM_SIZE - 1] = 0;
    CPU last; last.mem = &mem; last.pc = static_cast<std::uint16_t>(MEM_SIZE - 1);
    require(run(last, 1) && last.pc == MEM_SIZE, "HALT at final byte allowed");
    const auto stopped_pc = last.pc;
    step(last); require(run(last) && last.pc == stopped_pc, "HALT stays stopped");
    CPU detached;
    require(!run(detached) && detached.error != nullptr, "null memory stops run");

    Memory nops;
    for (auto& byte : nops.data) byte = 1;
    CPU limited; limited.mem = &nops;
    require(!run(limited, 5) && limited.pc == 5 && limited.error != nullptr,
            "instruction budget prevents unbounded execution");
    CPU zero; zero.mem = &nops;
    require(!run(zero, 0) && zero.pc == 0, "zero budget does not execute");

    // Integration: exact seven-instruction program, code at 0, data at 0x800.
    Memory ab;
    const Byte program[] = {0x28, 0x00, 0x08, 0x29, 0x02, 0x2B, 0x29, 0x02, 0x00};
    for (std::size_t i = 0; i < sizeof(program); ++i) ab.data[i] = program[i];
    ab.data[0x800] = 65; ab.data[0x801] = 66; ab.data[0x802] = 67;
    CPU demo; demo.mem = &ab;
    output.str("");
    require(run(demo, 7), "seven-instruction program halts at exact budget");
    require(output.str() == "AB", "AB integration output");
    require(demo.pc == 9 && demo.h == 0x801 && demo.a == 66, "AB final registers");

    // Von Neumann memory: STORE into code is permitted by the ISA.
    Memory code;
    const Byte modify[] = {0x20, 0x00, 0x24, 0x05, 0x00, 0x01};
    for (std::size_t i = 0; i < sizeof(modify); ++i) code.data[i] = modify[i];
    CPU self; self.mem = &code;
    require(run(self, 3) && self.pc == 6 && code.data[5] == 0,
            "STORE can replace NOP with HALT in shared code memory");
}

int main() {
    std::ostringstream output;
    auto* saved = std::cout.rdbuf(output.rdbuf());
    test_words();
    test_instructions(output);
    test_faults_and_run(output);
    std::cout.rdbuf(saved);
    std::cout << "PASS Lab 03: " << checks << " checks\n";
}
