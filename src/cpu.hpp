// CPU Lab 02, extended with the address register and fault state in Lab 03.
#pragma once
#include <cstdint>
#include "memory.hpp"

// Коди операцій — саме з ISA.uk.md, свого не вигадуйте.
// Поки це звичайні константи (Lab 1, `const`). У Lab 6 вони стануть `enum class Op`.
const Byte OP_HALT = 0x00;
const Byte OP_NOP  = 0x01;
const Byte OP_OUT  = 0x02;
const Byte OP_OUTN = 0x03;
const Byte OP_ADD  = 0x10;
const Byte OP_SUB  = 0x11;
const Byte OP_AND  = 0x12;
const Byte OP_OR   = 0x13;
const Byte OP_XOR  = 0x14;
const Byte OP_NOT  = 0x15;
const Byte OP_SHL  = 0x16;
const Byte OP_SHR  = 0x17;
const Byte OP_INC  = 0x18;
const Byte OP_DEC  = 0x19;
const Byte OP_CMP  = 0x1A;
const Byte OP_LOADI_A = 0x20;
const Byte OP_LOADI_B = 0x21;
const Byte OP_LOAD_A  = 0x22;
const Byte OP_LOAD_B  = 0x23;
const Byte OP_STORE_A = 0x24;
const Byte OP_STORE_B = 0x25;
const Byte OP_MOV_AB  = 0x26; // A = B
const Byte OP_MOV_BA  = 0x27; // B = A
const Byte OP_LOADH   = 0x28;
const Byte OP_LOAD_H  = 0x29;
const Byte OP_STORE_H = 0x2A;
const Byte OP_INCH    = 0x2B;
const Byte OP_DECH    = 0x2C;
const Byte OP_HLOW    = 0x2D;
const Byte OP_JMP     = 0x30;
const Byte OP_JZ      = 0x31;
const Byte OP_JNZ     = 0x32;

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
};

void step(CPU& cpu);
bool run(CPU& cpu, std::size_t max_steps = 100000);
void dump_regs(const CPU& cpu);
