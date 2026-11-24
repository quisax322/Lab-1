#include "cpu.hpp"
#include "display.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

static bool same_memory(const Memory& actual, const Memory& expected) {
    return std::equal(actual.data, actual.data + MEM_SIZE, expected.data);
}

static void expect_step(const CPU& cpu, unsigned pc, Flags flags, Byte a, Byte b) {
    require(cpu.pc == pc && cpu.error == nullptr && !cpu.halted, "instruction and next PC");
    require(cpu.a == a && cpu.b == b && cpu.h == 0x812, "A, B and H preserved");
    require(cpu.f.z == flags.z && cpu.f.n == flags.n && cpu.f.c == flags.c, "ISA flags");
}

static void test_display(std::ostringstream& output) {
    Memory mem;
    std::fill_n(mem.data, MEM_SIZE, Byte{0xA5});
    Memory expected = mem;
    std::fill_n(expected.data + 0xA00, 256, Byte{0});

    clear(mem);
    require(same_memory(mem, expected), "clear only VRAM");
    for (int x : {-1, 64, 255}) {
        require(!plot(mem, x, 0), "reject invalid host x");
    }
    for (int y : {-1, 32, 255}) {
        require(!plot(mem, 0, y), "reject invalid host y");
    }
    require(same_memory(mem, expected), "invalid host coordinates leave all memory intact");

    require(plot(mem, 0, 0) && plot(mem, 7, 0) && plot(mem, 8, 0), "plot across byte boundary");
    require(plot(mem, 13, 2) && plot(mem, 63, 31) && plot(mem, 0, 0), "plot interior and corners");
    expected.data[0xA00] = 0x81;
    expected.data[0xA01] = 0x80;
    expected.data[0xA11] = 0x04;
    expected.data[0xAFF] = 0x01;
    require(same_memory(mem, expected), "bit order, row stride, OR and repeated plot");

    const std::string blank = std::string(64, '.') + '\n';
    show(mem);
    std::string screen;
    for (int y = 0; y < 32; ++y) screen += blank;
    screen[0] = screen[7] = screen[8] = screen[2 * 65 + 13] = screen[31 * 65 + 63] = '#';
    require(output.str() == screen, "exact 64x32 rendering");
    require(same_memory(mem, expected), "show preserves memory");
    output.str("");
}

static void test_screen_instructions(std::ostringstream& output) {
    Memory mem;
    std::fill_n(mem.data, MEM_SIZE, Byte{0xA5});
    const Byte program[] = {0x40, 0x41, 0x41, 0x41, 0x41, 0x43, 0x00};
    std::copy(program, program + sizeof(program), mem.data);
    Memory expected = mem;
    std::fill_n(expected.data + 0xA00, 256, Byte{0});

    CPU cpu;
    cpu.mem = &mem;
    cpu.h = 0x812;
    cpu.f = {true, false, true};
    step(cpu);  // CLS
    expect_step(cpu, 1, {true, false, true}, 0, 0);
    require(same_memory(mem, expected), "CLS clears VRAM only");

    cpu.a = 64;
    cpu.f.c = false;
    step(cpu);  // Invalid x.
    expect_step(cpu, 2, {true, false, true}, 64, 0);
    require(same_memory(mem, expected), "invalid guest x leaves memory intact");

    cpu.a = 0;
    step(cpu);  // First pixel, after a failed PLOT.
    expect_step(cpu, 3, {true, false, false}, 0, 0);
    expected.data[0xA00] = 0x80;
    require(same_memory(mem, expected), "first guest pixel");

    cpu.b = 32;
    step(cpu);  // Invalid y.
    expect_step(cpu, 4, {true, false, true}, 0, 32);
    require(same_memory(mem, expected), "invalid guest y leaves memory intact");

    cpu.a = 63;
    cpu.b = 31;
    cpu.f = {false, true, true};
    step(cpu);  // Last pixel.
    expect_step(cpu, 5, {false, true, false}, 63, 31);
    expected.data[0xAFF] = 0x01;
    require(same_memory(mem, expected), "last guest pixel");
    require(output.str().empty(), "CLS and PLOT produce no output");

    cpu.f.c = true;
    step(cpu);  // SHOW
    expect_step(cpu, 6, {false, true, true}, 63, 31);
    std::string screen;
    for (int y = 0; y < 32; ++y) screen += std::string(64, '.') + '\n';
    screen[0] = screen[31 * 65 + 63] = '#';
    require(output.str() == screen, "SHOW displays shared VRAM");
    require(same_memory(mem, expected), "SHOW reads without writing");
    step(cpu);
    require(cpu.halted && cpu.pc == 7 && cpu.error == nullptr, "HALT follows SHOW");
    output.str("");
}

static void test_outs(std::ostringstream& output) {
    Memory mem;
    std::fill_n(mem.data, MEM_SIZE, Byte{'X'});
    const Byte text[] = {'H', 'E', 'L', 'L', 'O', 0, '!'};
    std::copy(text, text + sizeof(text), mem.data + 0x800);
    mem.data[0xFFF] = 'Y';

    struct Case {
        std::uint16_t address;
        std::uint16_t pc;
        std::string expected;
    };
    const Case cases[] = {
        {0x800, 0, "HELLO"},
        {0x805, 0, ""},
        {0x900, 0, std::string(256, 'X')},  // The 257th byte is also nonzero.
        {0xFFD, 0, "XXY"},
        {0xFFF, 0, "Y"},
        {0x1000, 0, ""},
        {0xFFFF, 0, ""},
        {0x800, 0xFFD, "HELLO"},
    };
    for (const auto& test : cases) {
        Memory trial = mem;
        trial.data[test.pc] = 0x04;
        require(set16(trial, test.pc + 1, test.address), "complete little-endian OUTS operand");
        const Memory before = trial;
        CPU cpu;
        cpu.mem = &trial;
        cpu.pc = test.pc;
        cpu.a = 0x23;
        cpu.b = 0x45;
        cpu.h = 0x812;
        cpu.f = {true, false, true};
        step(cpu);
        expect_step(cpu, test.pc + 3u, {true, false, true}, 0x23, 0x45);
        require(output.str() == test.expected, "exact OUTS bytes");
        require(same_memory(trial, before), "OUTS preserves memory");
        output.str("");
    }

    for (std::uint16_t pc : {std::uint16_t{0xFFE}, std::uint16_t{0xFFF}}) {
        mem.data[pc] = 0x04;
        CPU cpu;
        cpu.mem = &mem;
        cpu.pc = pc;
        step(cpu);
        require(cpu.error != nullptr && cpu.pc == pc, "truncated OUTS operand faults at original PC");
        require(output.str().find("instruction operand outside memory") != std::string::npos,
                "truncated OUTS operand diagnostic");
        output.str("");
    }
}

int main() {
    std::ostringstream output;
    auto* original = std::cout.rdbuf(output.rdbuf());
    test_display(output);
    test_screen_instructions(output);
    test_outs(output);
    std::cout.rdbuf(original);
    std::cout << "PASS Lab 05 display and OUTS\n";
}
