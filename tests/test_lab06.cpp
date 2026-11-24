#include "cpu.hpp"
#include "display.hpp"
#include "sprite.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct Encoding {
    Op op;
    Byte byte;
    std::uint16_t length;
};

// ISA.uk.md §5, including the previously implemented optional HLOW.
constexpr Encoding isa[] = {
    {Op::Halt, 0x00, 1},
    {Op::Nop, 0x01, 1},
    {Op::Out, 0x02, 1},
    {Op::Outn, 0x03, 1},
    {Op::Outs, 0x04, 3},
    {Op::Add, 0x10, 1},
    {Op::Sub, 0x11, 1},
    {Op::And, 0x12, 1},
    {Op::Or, 0x13, 1},
    {Op::Xor, 0x14, 1},
    {Op::Not, 0x15, 1},
    {Op::Shl, 0x16, 1},
    {Op::Shr, 0x17, 1},
    {Op::Inc, 0x18, 1},
    {Op::Dec, 0x19, 1},
    {Op::Cmp, 0x1A, 1},
    {Op::Mul, 0x1B, 1},
    {Op::LoadiA, 0x20, 2},
    {Op::LoadiB, 0x21, 2},
    {Op::LoadA, 0x22, 3},
    {Op::LoadB, 0x23, 3},
    {Op::StoreA, 0x24, 3},
    {Op::StoreB, 0x25, 3},
    {Op::MovAB, 0x26, 1},
    {Op::MovBA, 0x27, 1},
    {Op::LoadH, 0x28, 3},
    {Op::LoadAFromH, 0x29, 1},
    {Op::StoreAToH, 0x2A, 1},
    {Op::IncH, 0x2B, 1},
    {Op::DecH, 0x2C, 1},
    {Op::HLow, 0x2D, 1},
    {Op::Jmp, 0x30, 3},
    {Op::Jz, 0x31, 3},
    {Op::Jnz, 0x32, 3},
    {Op::Cls, 0x40, 1},
    {Op::Plot, 0x41, 1},
    {Op::Show, 0x43, 1},
    {Op::PushA, 0x50, 1},
    {Op::PopA, 0x51, 1},
    {Op::PushB, 0x52, 1},
    {Op::PopB, 0x53, 1},
    {Op::Call, 0x54, 3},
    {Op::Ret, 0x55, 1},
};

static_assert(std::is_same_v<std::underlying_type_t<Op>, std::uint8_t>);

static void test_opcodes(std::ostringstream& output) {
    for (const auto& encoding : isa) {
        require(static_cast<Byte>(encoding.op) == encoding.byte, "enum value matches ISA");
    }

    for (unsigned byte = 0; byte <= 0xFF; ++byte) {
        const auto* encoding = std::find_if(std::begin(isa), std::end(isa),
            [byte](const Encoding& entry) { return entry.byte == byte; });
        Memory mem{};
        mem.data[0] = static_cast<Byte>(byte);
        mem.data[1] = 3; // Jumps and CALL use the sequential address as their target.
        CPU cpu{};
        cpu.mem = &mem;
        cpu.h = 0x800;
        if (byte == 0x51 || byte == 0x53) {
            require(cpu.stack.push(mem, 0), "provide a byte for POP");
        } else if (byte == 0x55) {
            require(cpu.stack.push_address(mem, 1), "provide a sequential return address");
        }
        step(cpu);

        if (encoding == std::end(isa)) {
            require(cpu.error != nullptr && std::string(cpu.error) == "unknown opcode" &&
                    cpu.pc == 0 && !cpu.halted, "unsupported opcode faults at original PC");
            const auto diagnostic = output.str();
            step(cpu);
            require(output.str() == diagnostic && cpu.pc == 0, "unknown fault remains latched");
        } else {
            require(cpu.error == nullptr && cpu.pc == encoding->length, "opcode decodes with ISA length");
            require(cpu.halted == (encoding->op == Op::Halt), "only HALT stops the CPU");
        }
        output.str("");
    }
}

static void test_sprites() {
    Memory mem{};
    std::fill_n(mem.data, MEM_SIZE, Byte{0xA5});
    Memory expected = mem;
    std::fill_n(expected.data + 0xA00, 256, Byte{0});
    Sprite sprites[SPRITE_COUNT] = {
        {1, 1, -1, -1, true},
        {62, 30, 1, 1, true},
        {20, 10, 5, -3, false},
    };

    tick_sprites(mem, sprites);
    require(sprites[0].x == 0 && sprites[0].y == 0 && sprites[0].vx == 1 && sprites[0].vy == 1,
            "left and top edges reverse velocity on contact");
    require(sprites[1].x == 63 && sprites[1].y == 31 && sprites[1].vx == -1 && sprites[1].vy == -1,
            "right and bottom edges reverse velocity on contact");
    require(sprites[2].x == 20 && sprites[2].y == 10 && sprites[2].vx == 5 &&
            sprites[2].vy == -3 && !sprites[2].alive, "inactive sprite stays in place");
    expected.data[0xA00] = 0x80;
    expected.data[0xAFF] = 0x01;
    require(std::equal(std::begin(mem.data), std::end(mem.data), expected.data),
            "VRAM contains only two sprites and all other bytes are preserved");

    tick_sprites(mem, sprites);
    require(sprites[0].x == 1 && sprites[0].y == 1 && sprites[1].x == 62 && sprites[1].y == 30,
            "both sprites move back into the screen");
    expected.data[0xA00] = expected.data[0xAFF] = 0;
    expected.data[0xA08] = 0x40;
    expected.data[0xAF7] = 0x02;
    require(std::equal(std::begin(mem.data), std::end(mem.data), expected.data),
            "second frame clears previous pixels and preserves memory outside VRAM");

    sprites[0] = {0, 31, -5, 4, true};
    sprites[1] = {16, 8, -127, 127, true};
    sprites[7] = {63, 0, 0, 0, true};
    tick_sprites(mem, sprites);
    require(sprites[0].x == 5 && sprites[0].y == 27 && sprites[0].vx == 5 && sprites[0].vy == -4,
            "outward movement from an edge reflects its full distance");
    require(sprites[1].x == 15 && sprites[1].y == 11 && sprites[1].vx == -127 && sprites[1].vy == 127,
            "maximum speeds support multiple reflections per tick");
    require(sprites[7].x == 63 && sprites[7].y == 0 && sprites[7].vx == 0 && sprites[7].vy == 0,
            "zero speeds leave an active sprite stationary");
}

int main() {
    std::ostringstream output;
    auto* original = std::cout.rdbuf(output.rdbuf());
    test_opcodes(output);
    std::cout.rdbuf(original);
    test_sprites();
    std::cout << "PASS Lab 06 opcode encodings and sprite movement\n";
}
