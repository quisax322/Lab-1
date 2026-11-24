#include "assembler.hpp"

#include <cctype>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "cpu.hpp"
#include "lexer.hpp"
#include "number.hpp"

namespace {

enum class Form { None, A, B, H, IndirectH, Value, Memory, Imm8, Imm16, Address };
using Kind = Token::Kind;

struct Instruction {
    std::string_view name;
    Op op;
    Form first = Form::None;
    Form second = Form::None;
};

constexpr Instruction instructions[] = {
    {"HALT", Op::Halt},
    {"NOP", Op::Nop},
    {"OUT", Op::Out},
    {"OUTN", Op::Outn},
    {"OUTS", Op::Outs, Form::Address},
    {"ADD", Op::Add, Form::A, Form::B},
    {"SUB", Op::Sub, Form::A, Form::B},
    {"AND", Op::And, Form::A, Form::B},
    {"OR", Op::Or, Form::A, Form::B},
    {"XOR", Op::Xor, Form::A, Form::B},
    {"NOT", Op::Not, Form::A},
    {"SHL", Op::Shl, Form::A},
    {"SHR", Op::Shr, Form::A},
    {"INC", Op::Inc, Form::A},
    {"DEC", Op::Dec, Form::A},
    {"CMP", Op::Cmp, Form::A, Form::B},
    {"MUL", Op::Mul, Form::A, Form::B},
    {"LOADI", Op::LoadiA, Form::A, Form::Imm8},
    {"LOADI", Op::LoadiB, Form::B, Form::Imm8},
    {"LOAD", Op::LoadA, Form::A, Form::Memory},
    {"LOAD", Op::LoadB, Form::B, Form::Memory},
    {"STORE", Op::StoreA, Form::Memory, Form::A},
    {"STORE", Op::StoreB, Form::Memory, Form::B},
    {"MOV", Op::MovAB, Form::A, Form::B},
    {"MOV", Op::MovBA, Form::B, Form::A},
    {"LOADH", Op::LoadH, Form::H, Form::Imm16},
    {"LOAD", Op::LoadAFromH, Form::A, Form::IndirectH},
    {"STORE", Op::StoreAToH, Form::IndirectH, Form::A},
    {"INCH", Op::IncH},
    {"DECH", Op::DecH},
    {"HLOW", Op::HLow},
    {"JMP", Op::Jmp, Form::Address},
    {"JZ", Op::Jz, Form::Address},
    {"JNZ", Op::Jnz, Form::Address},
    {"CLS", Op::Cls},
    {"PLOT", Op::Plot},
    {"SHOW", Op::Show},
    {"PUSH", Op::PushA, Form::A},
    {"POP", Op::PopA, Form::A},
    {"PUSH", Op::PushB, Form::B},
    {"POP", Op::PopB, Form::B},
    {"CALL", Op::Call, Form::Address},
    {"RET", Op::Ret},
};

constexpr std::size_t operand_size(Form form) {
    if (form == Form::Imm8) return 1;
    if (form == Form::Imm16 || form == Form::Address || form == Form::Memory) return 2;
    return 0;
}

constexpr bool sizes_match_cpu() {
    for (const auto& inst : instructions) {
        if (instruction_size(inst.op) != 1 + operand_size(inst.first) + operand_size(inst.second)) {
            return false;
        }
    }
    return true;
}

static_assert(sizes_match_cpu(), "assembler operands must match CPU instruction sizes");

using Labels = std::unordered_map<std::string_view, std::size_t>;

struct Operand {
    Form form = Form::None;
    const Token* token = nullptr;
};

struct ParsedInstruction {
    const Instruction* instruction;
    const Token* value;
    Form value_form;
};

std::string uppercase(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

Form register_form(const Token& token) {
    if (token.kind == Kind::Ident && token.text.size() == 1) {
        switch (token.text.front()) {
        case 'a': case 'A': return Form::A;
        case 'b': case 'B': return Form::B;
        case 'h': case 'H': return Form::H;
        }
    }
    return Form::Value;
}

bool end_of_line(const Token& token) {
    return token.kind == Kind::Newline || token.kind == Kind::Eof;
}

bool fail(std::string& error, int line, const std::string& message) {
    error = "line " + std::to_string(line) + ": " + message;
    return false;
}

bool parse_operand(const Token*& cursor, Operand& operand, std::string& error) {
    const bool brackets = cursor->kind == Kind::LBracket;
    if (brackets) cursor = cursor->next;
    const Token& value = *cursor;
    if (value.kind != Kind::Ident && value.kind != Kind::Number) {
        return fail(error, value.line, brackets ? "expected an address or H after '['"
                                                : "expected an operand");
    }
    cursor = cursor->next;
    auto form = register_form(value);
    if (brackets) {
        if (cursor->kind != Kind::RBracket) return fail(error, value.line, "expected ']'");
        cursor = cursor->next;
        if (form == Form::H) form = Form::IndirectH;
        else if (form == Form::Value) form = Form::Memory;
        else return fail(error, value.line, "only H can be used as an indirect register");
    }
    operand = {form, &value};
    return true;
}

bool matches(Form expected, const Operand& operand) {
    if (expected == Form::Imm8 || expected == Form::Imm16 || expected == Form::Address) {
        return operand.form == Form::Value;
    }
    return expected == operand.form;
}

bool parse_instruction(const Token*& cursor, std::vector<ParsedInstruction>& parsed,
                       std::size_t& address, std::string& error) {
    const Token& mnemonic = *cursor;
    if (mnemonic.kind != Kind::Ident) return fail(error, mnemonic.line, "expected a mnemonic");
    const auto name = uppercase(mnemonic.text);
    cursor = cursor->next;

    Operand first, second;
    if (!end_of_line(*cursor)) {
        if (!parse_operand(cursor, first, error)) return false;
        if (!end_of_line(*cursor)) {
            if (cursor->kind != Kind::Comma) return fail(error, mnemonic.line, "expected ','");
            cursor = cursor->next;
            if (!parse_operand(cursor, second, error)) return false;
        }
    }
    if (!end_of_line(*cursor)) return fail(error, mnemonic.line, "unexpected token after operands");

    bool known = false;
    for (const auto& inst : instructions) {
        if (inst.name != name) continue;
        known = true;
        if (!matches(inst.first, first) || !matches(inst.second, second)) continue;
        if (instruction_size(inst.op) > CODE_HI + 1 - address) {
            return fail(error, mnemonic.line, "program exceeds the 2048-byte CODE region");
        }
        address += instruction_size(inst.op);
        if (operand_size(inst.first) != 0) parsed.push_back({&inst, first.token, inst.first});
        else if (operand_size(inst.second) != 0) parsed.push_back({&inst, second.token, inst.second});
        else parsed.push_back({&inst, nullptr, Form::None});
        return true;
    }
    return fail(error, mnemonic.line, known ? "invalid operands for " + name
                                            : "unknown mnemonic '" + mnemonic.text + "'");
}

bool resolve(const Token& token, Form form, const Labels& labels, std::uint64_t& value,
             std::string& error) {
    if (token.kind == Kind::Number) {
        if (parse_number_literal(token.text, value) != std::errc{}) {
            return fail(error, token.line, "bad number");
        }
    } else {
        if (form == Form::Imm8) return fail(error, token.line, "expected an 8-bit number");
        const auto label = labels.find(token.text);
        if (label == labels.end()) return fail(error, token.line, "undefined label '" + token.text + "'");
        value = label->second;
    }
    if (form == Form::Imm8 && value > std::numeric_limits<Byte>::max()) {
        return fail(error, token.line, "8-bit value must be 0..255");
    }
    if (form == Form::Imm16 && value > std::numeric_limits<std::uint16_t>::max()) {
        return fail(error, token.line, "16-bit value must be 0..65535");
    }
    if ((form == Form::Address || form == Form::Memory) && value >= MEM_SIZE) {
        return fail(error, token.line, "address must be 0..4095");
    }
    return true;
}

} // namespace

std::vector<Byte> assemble(const std::string& source, std::string& error) {
    const std::unique_ptr<Token, decltype(&destroy)> tokens(lex(source, error), destroy);
    if (!tokens) return {};

    // Pass 1 records labels and instruction sizes without resolving addresses.
    Labels labels;
    std::vector<ParsedInstruction> parsed;
    std::size_t address = CODE_LO;
    const Token* cursor = tokens.get();
    while (cursor->kind != Kind::Eof) {
        if (cursor->kind == Kind::Ident && cursor->next->kind == Kind::Colon) {
            if (register_form(*cursor) != Form::Value) {
                fail(error, cursor->line, "register name cannot be used as a label");
                return {};
            }
            if (!labels.emplace(cursor->text, address).second) {
                fail(error, cursor->line, "duplicate label '" + cursor->text + "'");
                return {};
            }
            cursor = cursor->next->next;
        }
        if (!end_of_line(*cursor) && !parse_instruction(cursor, parsed, address, error)) return {};
        if (cursor->kind == Kind::Newline) cursor = cursor->next;
    }
    if (parsed.empty()) {
        fail(error, cursor->line, "expected at least one instruction");
        return {};
    }

    // Pass 2 resolves operands and emits the complete program.
    std::vector<Byte> bytes;
    bytes.reserve(address - CODE_LO);
    for (const auto& entry : parsed) {
        bytes.push_back(static_cast<Byte>(entry.instruction->op));
        if (!entry.value) continue;
        std::uint64_t value = 0;
        if (!resolve(*entry.value, entry.value_form, labels, value, error)) return {};
        bytes.push_back(static_cast<Byte>(value & 0xFFu));
        if (operand_size(entry.value_form) == 2) bytes.push_back(static_cast<Byte>(value >> 8));
    }
    return bytes;
}
