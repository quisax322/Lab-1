// CPU Lab 02, extended with the address register and fault state in Lab 03.
#pragma once
#include <cstdint>
#include "memory.hpp"
#include "stack.hpp"

// Implemented opcodes from ISA.uk.md.
enum class Op : std::uint8_t {
    Halt = 0x00,
    Nop = 0x01,
    Out = 0x02,
    Outn = 0x03,
    Outs = 0x04,
    Add = 0x10,
    Sub = 0x11,
    And = 0x12,
    Or = 0x13,
    Xor = 0x14,
    Not = 0x15,
    Shl = 0x16,
    Shr = 0x17,
    Inc = 0x18,
    Dec = 0x19,
    Cmp = 0x1A,
    Mul = 0x1B,
    LoadiA = 0x20,
    LoadiB = 0x21,
    LoadA = 0x22,
    LoadB = 0x23,
    StoreA = 0x24,
    StoreB = 0x25,
    MovAB = 0x26, // A = B
    MovBA = 0x27, // B = A
    LoadH = 0x28,
    LoadAFromH = 0x29,
    StoreAToH = 0x2A,
    IncH = 0x2B,
    DecH = 0x2C,
    HLow = 0x2D,
    Jmp = 0x30,
    Jz = 0x31,
    Jnz = 0x32,
    Cls = 0x40,
    Plot = 0x41,
    Show = 0x43,
    PushA = 0x50,
    PopA = 0x51,
    PushB = 0x52,
    PopB = 0x53,
    Call = 0x54,
    Ret = 0x55,
};

struct Flags {
    bool z = false;   // результат нульовий
    bool n = false;   // старший біт результату — одиниця
    bool c = false;   // біт, який виїхав за межу восьми
};

struct CPU {
    Memory* mem = nullptr;   // де лежить коробка. Що таке `*`, пояснює Lab 3;
                             // поки читайте як «процесор знає, де його пам'ять»
    std::uint16_t pc = 0;    // адреса НАСТУПНОЇ інструкції
    std::uint16_t h = 0;     // адреса гостя, не вказівник хоста
    Byte a = 0;
    Byte b = 0;
    Flags f;
    bool halted = false;     // після HALT наступні step відмовляють
    const char* error = nullptr; // latched fault; PC stays at failing instruction
    Stack stack;             // guest SP and stack operations over *mem
};

void step(CPU& cpu);
bool run(CPU& cpu, std::size_t max_steps = 100000);
void dump_regs(const CPU& cpu);
void dump_stack(const CPU& cpu);
