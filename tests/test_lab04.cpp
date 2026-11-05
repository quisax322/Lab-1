#include "cpu.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

static unsigned checks = 0;
static void require(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
static bool same(Flags a, Flags b) { return a.z == b.z && a.n == b.n && a.c == b.c; }

static void compare() {
    Memory mem; mem.data[0] = 0x1A;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned b = 0; b < 256; ++b) {
            for (unsigned f = 0; f < 8; ++f) {
                CPU cpu; cpu.mem = &mem; cpu.a = static_cast<Byte>(a); cpu.b = static_cast<Byte>(b);
                cpu.f = {(f & 1) != 0, (f & 2) != 0, (f & 4) != 0};
                step(cpu);
                const unsigned difference = (a + 256 - b) % 256;
                require(cpu.pc == 1 && cpu.error == nullptr, "CMP length");
                require(cpu.a == a && cpu.b == b && cpu.h == 0, "CMP preserves A/B/H");
                require(cpu.f.z == (a == b) && cpu.f.n == (difference >= 128) &&
                        cpu.f.c == (a < b), "CMP independent Z/N/borrow oracle");
            }
        }
    }
}
static void branches() {
    for (Byte op : {Byte{0x30}, Byte{0x31}, Byte{0x32}}) {
        for (unsigned f = 0; f < 8; ++f) {
            const Flags flags{(f & 1) != 0, (f & 2) != 0, (f & 4) != 0};
            const bool taken = op == 0x30 || (op == 0x31 && flags.z) || (op == 0x32 && !flags.z);
            for (unsigned target : {0u, 0x123u, 4095u, 4096u, 65535u}) {
                for (unsigned start : {0u, 4093u}) {
                    Memory mem; mem.data[start] = op;
                    set16(mem, start + 1, static_cast<std::uint16_t>(target));
                    CPU cpu; cpu.mem = &mem; cpu.pc = static_cast<std::uint16_t>(start);
                    cpu.a = 42; cpu.b = 65; cpu.h = 0x800; cpu.f = flags;
                    step(cpu);
                    const bool fault = taken && target >= MEM_SIZE;
                    require((cpu.error != nullptr) == fault, "branch target range");
                    require(cpu.pc == (fault ? start : taken ? target : start + 3), "branch PC exactly");
                    require(cpu.a == 42 && cpu.b == 65 && cpu.h == 0x800 && same(cpu.f, flags),
                            "branch preserves registers and flags");
                    // At start=4093 the target overlaps the high operand byte (0x0F).
                    if (taken && target == 4095 && start == 0) {
                        step(cpu);
                        require(cpu.halted && cpu.pc == 4096, "jump to final HALT");
                    }
                }
            }
        }
        for (unsigned start : {4094u, 4095u}) {
            Memory mem; mem.data[start] = op;
            CPU cpu; cpu.mem = &mem; cpu.pc = static_cast<std::uint16_t>(start);
            cpu.f.z = op == 0x32; // conditional branch not taken, but still needs operands
            step(cpu);
            require(cpu.error != nullptr && cpu.pc == start, "truncated branch even if not taken");
        }
    }
    // All unsupported bytes, including opcodes reserved for Labs 05-08, fault.
    for (unsigned op = 0; op < 256; ++op) {
        const bool known = op <= 3 || (op >= 0x10 && op <= 0x1A) ||
                           (op >= 0x20 && op <= 0x2D) || (op >= 0x30 && op <= 0x32);
        if (known) continue;
        Memory mem; mem.data[0] = static_cast<Byte>(op);
        CPU cpu; cpu.mem = &mem; cpu.a = 7; cpu.b = 8; cpu.h = 9; cpu.f = {true,true,true};
        step(cpu);
        require(cpu.error != nullptr && !cpu.halted && cpu.pc == 0, "unknown byte faults without advancing");
        require(cpu.a == 7 && cpu.b == 8 && cpu.h == 9 && same(cpu.f, {true,true,true}), "unknown atomicity");
        mem.data[0] = 0x18; step(cpu);
        require(!run(cpu) && cpu.pc == 0 && cpu.a == 7, "unknown fault latched");
    }
    for (unsigned h : {0u, 0x800u, 0x807u, 0xFFFFu}) {
        for (unsigned f = 0; f < 8; ++f) {
            Memory mem; mem.data[0] = 0x2D;
            CPU cpu; cpu.mem = &mem; cpu.h = static_cast<std::uint16_t>(h); cpu.b = 65;
            cpu.f = {(f & 1) != 0, (f & 2) != 0, (f & 4) != 0}; const Flags before = cpu.f;
            step(cpu);
            require(cpu.pc == 1 && cpu.a == h % 256 && cpu.h == h && cpu.b == 65 && same(cpu.f,before),
                    "HLOW only changes A and PC");
        }
    }
    Memory mem; mem.data[0] = 0x30; // JMP 0
    CPU cpu; cpu.mem = &mem;
    require(!run(cpu) && cpu.pc == 0 && cpu.error != nullptr, "default budget stops real infinite loop");
}
static void search() {
    Memory mem; mem.data[0x802] = 0x41; mem.data[0x805] = 0x41; mem.data[4095] = 0xFF;
    require(find_byte(mem,0x800,0x808,0x41) == 0x802, "first match");
    require(find_byte(mem,0x802,0x803,0x41) == 0x802, "first byte");
    require(find_byte(mem,0x800,0x802,0x41) == 0x802, "exclusive hi");
    require(find_byte(mem,0x800,0x808,0x42) == 0x808, "not found sentinel");
    require(find_byte(mem,0,0,0) == 0 && find_byte(mem,4096,4096,0) == 4096, "empty endpoints");
    require(find_byte(mem,4095,4096,0xFF) == 4095, "final byte");
    for (const auto pair : {std::pair<std::size_t,std::size_t>{2,1}, {0,4097},
                           {0,std::numeric_limits<std::size_t>::max()}}) {
        bool rejected = false;
        try { (void)find_byte(mem,pair.first,pair.second,0); }
        catch (const std::out_of_range&) { rejected = true; }
        require(rejected, "invalid find range");
    }
    const Byte program[] = {
        0x28,0x00,0x08, 0x21,0x41, 0x29,0x1A,0x31,0x15,0x00,
        0x2B,0x2D,0x21,0x08,0x1A,0x31,0x18,0x00,0x30,0x03,0x00,
        0x2D,0x03,0x00, 0x20,0xFF,0x03,0x00};
    for (int match = -1; match < 8; ++match) {
        Memory guest;
        for (std::size_t i = 0; i < sizeof(program); ++i) guest.data[i] = program[i];
        for (std::size_t i = 0x800; i < 0x808; ++i) guest.data[i] = 0x42;
        guest.data[0x808] = 0x41; // must never count the ninth byte as a match
        if (match >= 0) {
            guest.data[0x800 + match] = 0x41;
            if (match < 7) guest.data[0x807] = 0x41; // return first match
        }
        CPU cpu; cpu.mem = &guest;
        std::ostringstream captured; auto* saved = std::cout.rdbuf(captured.rdbuf());
        const bool done = run(cpu,100);
        std::cout.rdbuf(saved);
        require(done, "bounded guest search halts");
        require(captured.str() == std::to_string(match < 0 ? 255 : match) + " ", "guest search output");
        require(cpu.h == static_cast<unsigned>(0x800 + (match < 0 ? 8 : match)), "guest never passes hi");
    }
}
static void countdown() {
    Memory mem; const Byte code[] = {0x20,3,3,0x19,0x32,2,0,0};
    for (std::size_t i = 0; i < sizeof(code); ++i) mem.data[i] = code[i];
    CPU cpu; cpu.mem = &mem;
    const unsigned pcs[] = {0,2,3,4,2,3,4,2,3,4,7,8};
    const unsigned values[] = {0,3,3,2,2,2,1,1,1,0,0,0};
    std::ostringstream out;
    std::cout << "Countdown trace: PC(before) -> PC(after), A(after), Z(after)\n";
    for (unsigned i = 0; i < 11; ++i) {
        auto* saved = std::cout.rdbuf(out.rdbuf()); step(cpu); std::cout.rdbuf(saved);
        require(cpu.pc == pcs[i+1] && cpu.a == values[i+1] && cpu.f.z == (i >= 8), "countdown complete trace");
        std::cout << pcs[i] << " -> " << cpu.pc << " A=" << static_cast<unsigned>(cpu.a) << " Z=" << cpu.f.z << '\n';
    }
    require(cpu.halted && out.str() == "3 2 1 ", "countdown output and HALT");
    std::cout << "Countdown output: " << out.str() << '\n';
}
int main() {
    std::ostringstream diagnostics; auto* saved = std::cout.rdbuf(diagnostics.rdbuf());
    compare(); branches(); search();
    std::cout.rdbuf(saved);
    require(diagnostics.str().find("unknown opcode 0x04") != std::string::npos &&
            diagnostics.str().find("unknown opcode 0xff") != std::string::npos, "two-digit unknown diagnostics");
    countdown();
    std::cout << "PASS Lab 04: " << checks << " checks\n";
}
