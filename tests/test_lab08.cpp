#include "asm/assembler.hpp"
#include "asm/lexer.hpp"
#include "cpu.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <utility>

#ifdef EMBER_CHECK_ASAN_ALLOCATIONS
#include <atomic>
#include <sanitizer/allocator_interface.h>
#endif

static void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

static void test_lexer() {
    using Kind = Token::Kind;
    const Token expected[] = {
        {Kind::Ident, "ADD", 1}, {Kind::Ident, "A", 1}, {Kind::Comma, ",", 1},
        {Kind::Ident, "B", 1}, {Kind::Newline, "\\n", 1},
        {Kind::Ident, "LOADI", 2}, {Kind::Ident, "A", 2}, {Kind::Comma, ",", 2},
        {Kind::Number, "65", 2}, {Kind::Newline, "\\n", 2},
        {Kind::Ident, "LOADI", 3}, {Kind::Ident, "A", 3}, {Kind::Comma, ",", 3},
        {Kind::Number, "0x41", 3}, {Kind::Newline, "\\n", 3},
        {Kind::Ident, "LOADI", 4}, {Kind::Ident, "A", 4}, {Kind::Comma, ",", 4},
        {Kind::Number, "0b01000001", 4}, {Kind::Newline, "\\n", 4},
        {Kind::Ident, "loop", 5}, {Kind::Colon, ":", 5}, {Kind::Ident, "JMP", 5},
        {Kind::Ident, "loop", 5}, {Kind::Newline, "\\n", 5},
        {Kind::Ident, "STORE", 6}, {Kind::LBracket, "[", 6}, {Kind::Ident, "H", 6},
        {Kind::RBracket, "]", 6}, {Kind::Comma, ",", 6}, {Kind::Ident, "A", 6},
        {Kind::Eof, "", 6},
    };
    std::string error = "previous error";
    Token* head = lex("\xEF\xBB\xBF\tADD A, B ; comment\r\nLOADI A, 65\r\nLOADI A, 0x41\n"
                      "LOADI A, 0b01000001\nloop: JMP loop\r\nSTORE [H], A", error);
    require(error.empty(), "lex clears a previous error");
    const Token* cursor = head;
    for (const auto& token : expected) {
        require(cursor && cursor->kind == token.kind && cursor->text == token.text &&
                cursor->line == token.line, "token kind, text and line");
        cursor = cursor->next;
    }
    require(!cursor, "Eof terminates the linked list");
    destroy(head);
    destroy(nullptr);

    head = lex("; comment at EOF", error);
    require(head && head->kind == Kind::Eof && head->line == 1 && !head->next, "comment at EOF");
    destroy(head);
    head = lex("18446744073709551615", error);
    require(head && head->kind == Kind::Number, "largest lexical value (uint64)");
    destroy(head);

    for (const auto* invalid : {"0xGG", "0x", "0b", "0b102", "12ab", "100_0", "0xFZ"}) {
        head = lex("NOP\r\n\r\nLOADI A, " + std::string(invalid), error);
        require(!head && error == "line 3: bad number", invalid);
    }
    for (const auto* invalid : {"18446744073709551616", "0x10000000000000000",
                               "0b1" "00000000" "00000000" "00000000" "00000000"
                                     "00000000" "00000000" "00000000" "00000000"}) {
        head = lex("NOP\nLOADI A, " + std::string(invalid), error);
        require(!head && error == "line 2: number overflow", invalid);
    }
    require(!lex("HALT\n@", error) && error == "line 2: unexpected character '@'", "illegal punctuation");
    require(!lex("HALT\n\x01", error) && error == "line 2: unexpected non-printable byte", "control byte");
}

static std::vector<Byte> assemble_ok(const std::string& source) {
    std::string error;
    auto bytes = assemble(source, error);
    require(error.empty(), error);
    require(!bytes.empty(), "nonempty CODE image");
    return bytes;
}

static void expect_error(const std::string& source, std::string_view expected) {
    std::string error;
    const auto bytes = assemble(source, error);
    require(bytes.empty(), "an assembly error must discard the entire image");
    require(error == expected, error);
}

static void test_assembler() {
    // Each line exercises a distinct opcode, compared against ISA §5.
    const std::pair<const char*, std::vector<Byte>> cases[] = {
        {"HALT", {0x00}}, {"NOP", {0x01}}, {"OUT", {0x02}}, {"OUTN", {0x03}},
        {"OUTS 0xFFF", {0x04, 0xFF, 0x0F}},
        {"ADD A, B", {0x10}}, {"SUB A, B", {0x11}}, {"AND A, B", {0x12}},
        {"OR A, B", {0x13}}, {"XOR A, B", {0x14}}, {"NOT A", {0x15}},
        {"SHL A", {0x16}}, {"SHR A", {0x17}}, {"INC A", {0x18}}, {"DEC A", {0x19}},
        {"CMP A, B", {0x1A}}, {"MUL A, B", {0x1B}},
        {"LOADI A, 008", {0x20, 0x08}}, {"LOADI B, 0B11111111", {0x21, 0xFF}},
        {"LOAD A, [0x800]", {0x22, 0x00, 0x08}}, {"LOAD B, [0xABC]", {0x23, 0xBC, 0x0A}},
        {"STORE [0x0FFF], A", {0x24, 0xFF, 0x0F}}, {"STORE [0], B", {0x25, 0x00, 0x00}},
        {"MOV A, B", {0x26}}, {"MOV B, A", {0x27}}, {"LOADH H, 0XFFFF", {0x28, 0xFF, 0xFF}},
        {"load a, [h]", {0x29}}, {"STORE [H], A", {0x2A}},
        {"INCH", {0x2B}}, {"DECH", {0x2C}}, {"HLOW", {0x2D}},
        {"JMP 0x0123", {0x30, 0x23, 0x01}}, {"JZ 0x0100", {0x31, 0x00, 0x01}},
        {"JNZ 4095", {0x32, 0xFF, 0x0F}},
        {"CLS", {0x40}}, {"PLOT", {0x41}}, {"SHOW", {0x43}},
        {"PUSH A", {0x50}}, {"POP A", {0x51}}, {"PUSH B", {0x52}}, {"POP B", {0x53}},
        {"CALL 0b10000000001", {0x54, 0x01, 0x04}}, {"RET", {0x55}},
    };
    for (const auto& [source, bytes] : cases) require(assemble_ok(source) == bytes, source);

    const auto labels = assemble_ok("entry:\n JMP _next_entry\n NOP\n_next_entry: JMP entry\n"
                                    "LOAD A, [_next_entry]\nLOADH H, _next_entry\nOUTS entry");
    require(labels == std::vector<Byte>{0x30, 4, 0, 0x01, 0x30, 0, 0, 0x22, 4, 0,
                                       0x28, 4, 0, 0x04, 0, 0}, "forward and backward labels");

    const auto code = assemble_ok(
        "LOADI A, 5\nCALL fact\nOUTN\nHALT\n"
        "fact: LOADI B, 0\nCMP A, B\nJZ base\nLOADI B, 1\nCMP A, B\nJZ base\n"
        "PUSH A\nDEC A\nCALL fact\nPOP B\nMUL A, B\nRET\nbase: LOADI A, 1\nRET\n");
    const std::vector<Byte> fact_isa{0x20, 0x05, 0x54, 0x07, 0x00, 0x03, 0x00, 0x21, 0x00, 0x1A,
                                    0x31, 0x1B, 0x00, 0x21, 0x01, 0x1A, 0x31, 0x1B, 0x00, 0x50,
                                    0x19, 0x54, 0x07, 0x00, 0x53, 0x1B, 0x55, 0x20, 0x01, 0x55};
    require(code == fact_isa, "all 30 factorial bytes from ISA §6");

    expect_error("HALT\nWAT", "line 2: unknown mnemonic 'WAT'");
    expect_error("HALT\nADD A B", "line 2: expected ','");
    expect_error("HALT\nLOAD A, [H", "line 2: expected ']'");
    expect_error("LOAD A, H]", "line 1: unexpected token after operands");
    expect_error("STORE [A], B", "line 1: only H can be used as an indirect register");
    expect_error("LOAD B, [H]", "line 1: invalid operands for LOAD");
    expect_error("HALT\nLOADI H, 1", "line 2: invalid operands for LOADI");
    expect_error("HALT\nLOADI A,", "line 2: expected an operand");
    expect_error("LOADI A, word\nword: HALT", "line 1: expected an 8-bit number");
    expect_error("HALT\nLOADI A, 256", "line 2: 8-bit value must be 0..255");
    expect_error("LOADH H, 0x10000", "line 1: 16-bit value must be 0..65535");
    expect_error("OUTS 4096", "line 1: address must be 0..4095");
    expect_error("JMP 0x1000", "line 1: address must be 0..4095");
    expect_error("STORE [65535], A", "line 1: address must be 0..4095");
    expect_error("HALT\nCALL absent", "line 2: undefined label 'absent'");
    expect_error("JMP Loop\nloop: HALT", "line 1: undefined label 'Loop'");
    expect_error("loop: HALT\nloop: HALT", "line 2: duplicate label 'loop'");
    expect_error("H: HALT", "line 1: register name cannot be used as a label");
    expect_error("HALT\nLOADI A, 0x", "line 2: bad number");
    expect_error("; empty\n", "line 2: expected at least one instruction");
    expect_error("HALT NOP", "line 1: invalid operands for HALT");

    std::string full;
    for (std::size_t i = 0; i < CODE_HI; ++i) full += "NOP\n";
    require(assemble_ok(full + "HALT\n").size() == 2048, "exact CODE capacity");
    expect_error(full + "LOADI B, 0", "line 2048: program exceeds the 2048-byte CODE region");
    expect_error(full + "HALT\nHALT", "line 2049: program exceeds the 2048-byte CODE region");
}

static std::string read_program(const char* name) {
    std::ifstream file(name, std::ios::binary);
    require(file.is_open(), name);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static std::string execute(const std::vector<Byte>& code, Memory& mem, CPU& cpu, std::size_t& steps) {
    std::copy(code.begin(), code.end(), mem.data);
    cpu.mem = &mem;
    std::ostringstream output;
    auto* saved = std::cout.rdbuf(output.rdbuf());
    for (steps = 0; steps < 100000 && !cpu.halted && !cpu.error; ++steps) step(cpu);
    std::cout.rdbuf(saved);
    require(cpu.halted && !cpu.error && cpu.stack.sp() == 0xFFF, "successful run with restored SP");
    return output.str();
}

static void test_programs() {
    {
        Memory mem;
        CPU cpu;
        std::size_t steps = 0;
        require(execute(assemble_ok(read_program("programs/hello.asm")), mem, cpu, steps) == "HELLO",
                "OUTS prints HELLO");
        require(std::equal(mem.data + 0x800, mem.data + 0x806, "HELLO"), "terminated HELLO in DATA");
    }
    {
        auto source = read_program("programs/search.asm");
        const auto code = assemble_ok(source);
        const std::vector<Byte> lab04{0x28, 0x00, 0x08, 0x21, 0x41, 0x29, 0x1A, 0x31, 0x15,
                                      0x00, 0x2B, 0x2D, 0x21, 0x08, 0x1A, 0x31, 0x18, 0x00,
                                      0x30, 0x03, 0x00, 0x2D, 0x03, 0x00, 0x20, 0xFF, 0x03, 0x00};
        require(code.size() >= lab04.size(), "search code includes Lab 04 body");
        const auto start = code.size() - lab04.size();
        for (std::size_t i = 0; i < lab04.size(); ++i) {
            if (i == 8 || i == 16 || i == 19) {
                const auto target = static_cast<unsigned>(code[start + i]) |
                                    (static_cast<unsigned>(code[start + i + 1]) << 8);
                require(target == lab04[i] + start, "relocated Lab 04 branch target");
                ++i; // Checked the high byte with its low byte.
            } else {
                require(code[start + i] == lab04[i], "unchanged Lab 04 search byte");
            }
        }
        Memory mem;
        CPU cpu;
        std::size_t steps = 0;
        require(execute(code, mem, cpu, steps) == "2 " && cpu.h == 0x802 && steps == 40,
                "first match in bounded Lab 04 search");
        const std::string match = "0x41 ; in-range matches";
        const auto offset = source.find(match);
        require(offset != std::string::npos, "search input fixture");
        source.replace(offset, 4, "0x42");
        cpu = CPU{};
        require(execute(assemble_ok(source), mem, cpu, steps) == "255 " && cpu.h == 0x808 &&
                mem.data[0x808] == 0x41 && steps == 95, "search does not inspect address 0x808");
    }

    const auto source = read_program("programs/fib.asm");
    require(assemble_ok(source).size() == 34, "34-byte fib program");
    const auto argument = source.find("6 ; n");
    require(argument != std::string::npos, "fib argument fixture");
    struct FibCase { int n; unsigned value; std::size_t steps; };
    for (const auto& item : {FibCase{0, 0, 8}, FibCase{1, 1, 11}, FibCase{6, 8, 284}, FibCase{10, 55, 2021}}) {
        auto variant = source;
        variant.replace(argument, 1, std::to_string(item.n));
        Memory mem;
        CPU cpu;
        std::size_t steps = 0;
        const auto output = execute(assemble_ok(variant), mem, cpu, steps);
        require(output == std::to_string(item.value) + " " && cpu.a == item.value && steps == item.steps,
                "recursive fib value and step count");
        std::cout << "fib(" << item.n << ") = " << output << "steps=" << steps << " SP=0xFFF\n";
    }
}

#ifdef EMBER_CHECK_ASAN_ALLOCATIONS
static std::atomic<std::ptrdiff_t> live_allocations{0};

static void on_allocation(const volatile void* address, std::size_t) {
    if (address) ++live_allocations;
}

static void on_free(const volatile void* address) {
    if (address) --live_allocations;
}

static void exercise_ownership() {
    std::string error;
    Token* head = lex("loop: LOADI A, 0x41\nJMP loop\n", error);
    require(head, "lexer success allocates nodes");
    destroy(head);
    require(!lex("NOP\nLOADI B, 0b12", error), "lexing fails after allocating nodes");
    require(assemble_ok("JMP done\nNOP\ndone: HALT").size() == 5, "successful assembly owns tokens");
    expect_error("NOP\nLOADI B, 0b12", "line 2: bad number");
    expect_error("NOP\nLOAD A, [H", "line 2: expected ']'");
    expect_error("NOP\nJMP nowhere", "line 2: undefined label 'nowhere'");
}

static void test_token_allocations() {
    require(__sanitizer_install_malloc_and_free_hooks(on_allocation, on_free) > 0,
            "install ASan allocation hooks");
    exercise_ownership(); // Warm up the runtime before taking a baseline.
    std::string error;
    const auto before = live_allocations.load();
    Token* head = lex("HALT\n", error);
    // MSVC's debug string implementation can allocate alongside the three nodes.
    require(head && live_allocations.load() - before >= 3, "ASan hooks count live token allocations");
    destroy(head);
    require(live_allocations == before, "destroy releases the chain");
    for (int i = 0; i < 64; ++i) exercise_ownership();
    require(live_allocations == before, "no live allocations left by lex/assemble");
}
#endif

int main(int argc, char* argv[]) {
    require(argc == 2, "select lexer, assembler, programs or allocations");
    const std::string_view check = argv[1];
    if (check == "lexer") test_lexer();
    else if (check == "assembler") test_assembler();
    else if (check == "programs") test_programs();
#ifdef EMBER_CHECK_ASAN_ALLOCATIONS
    else if (check == "allocations") test_token_allocations();
#endif
    else require(false, "unknown Lab 08 test");
}
