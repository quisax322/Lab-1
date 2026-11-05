#include "alu.hpp"

static Byte update_zn(Byte result, Flags& f) {
    f.z = (result == 0);
    f.n = ((result & 0x80u) != 0);
    return result;
}

Byte alu_add(Byte a, Byte b, Flags& f) {
    // Keep the ninth bit before narrowing to eight bits.
    const std::uint16_t sum = static_cast<std::uint16_t>(a) + b;
    f.c = (sum > 0xFFu);
    return update_zn(static_cast<Byte>(sum), f);
}

Byte alu_sub(Byte a, Byte b, Flags& f) {
    f.c = (a < b);  // ISA: C means borrow for subtraction.
    return update_zn(static_cast<Byte>(a - b), f);
}

Byte alu_and(Byte a, Byte b, Flags& f) {
    f.c = false;
    return update_zn(static_cast<Byte>(a & b), f);
}

Byte alu_or(Byte a, Byte b, Flags& f) {
    f.c = false;
    return update_zn(static_cast<Byte>(a | b), f);
}

Byte alu_xor(Byte a, Byte b, Flags& f) {
    f.c = false;
    return update_zn(static_cast<Byte>(a ^ b), f);
}

Byte alu_not(Byte a, Flags& f) {
    f.c = false;
    return update_zn(static_cast<Byte>(~a), f);
}

Byte alu_shl(Byte a, Flags& f) {
    f.c = ((a & 0x80u) != 0);  // Old bit 7 leaves the byte.
    return update_zn(static_cast<Byte>(static_cast<unsigned>(a) << 1), f);
}

Byte alu_shr(Byte a, Flags& f) {
    f.c = ((a & 0x01u) != 0);  // Old bit 0 leaves the byte.
    return update_zn(static_cast<Byte>(static_cast<unsigned>(a) >> 1), f);
}

Byte alu_inc(Byte a, Flags& f) {
    // INC and DEC change Z/N only: preserve the incoming carry.
    return update_zn(static_cast<Byte>(a + 1), f);
}

Byte alu_dec(Byte a, Flags& f) {
    return update_zn(static_cast<Byte>(a - 1), f);
}
