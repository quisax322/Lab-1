#pragma once

#include "cpu.hpp"

// Each operation returns the wrapped byte and updates only its ISA flags.
Byte alu_add(Byte a, Byte b, Flags& f);
Byte alu_sub(Byte a, Byte b, Flags& f);
Byte alu_and(Byte a, Byte b, Flags& f);
Byte alu_or(Byte a, Byte b, Flags& f);
Byte alu_xor(Byte a, Byte b, Flags& f);
Byte alu_not(Byte a, Flags& f);
Byte alu_shl(Byte a, Flags& f);
Byte alu_shr(Byte a, Flags& f);
Byte alu_inc(Byte a, Flags& f);
Byte alu_dec(Byte a, Flags& f);
