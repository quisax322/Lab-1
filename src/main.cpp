// main.cpp — the prompt.
//
// GIVEN, in full. This file is scaffolding: it reads a line, splits it into
// words, and calls one of your functions. It uses `while`, `if` and functions,
// which the course only explains properly in Labs 4 and 7. That is on purpose —
// in Lab 1 you read this file, you do not write it.
//
// What you add in later labs is one more `else if` branch per command.
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "asm/assembler.hpp"
#include "asm/lexer.hpp"
#include "cpu.hpp"
#include "display.hpp"
#include "dump.hpp"
#include "memory.hpp"
#include "sprite.hpp"

// Turn a word into a number. Accepts decimal (65) and hex (0x41).
// Returns false if the word is not a number at all.
static bool parse_number(const std::string& word, long& out) {
    try {
        std::size_t used = 0;
        // base 0 means: look at the prefix. "0x41" is hex, "65" is decimal.
        out = std::stol(word, &used, 0);
        return used == word.size();  // reject things like "12abc"
    } catch (...) {
        return false;
    }
}

static const char* token_name(Token::Kind kind) {
    switch (kind) {
    case Token::Kind::Ident: return "Ident";
    case Token::Kind::Number: return "Number";
    case Token::Kind::Comma: return "Comma";
    case Token::Kind::Colon: return "Colon";
    case Token::Kind::LBracket: return "LBracket";
    case Token::Kind::RBracket: return "RBracket";
    case Token::Kind::Newline: return "Newline";
    case Token::Kind::Eof: return "Eof";
    }
    return "?";
}

static bool read_assembly(const std::string& path, std::string& source) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "cannot open file: " << path << '\n';
        return false;
    }
    char buffer[4096];
    while (file.read(buffer, sizeof buffer) || file.gcount() > 0) {
        source.append(buffer, static_cast<std::size_t>(file.gcount()));
    }
    if (file.bad() || !file.eof()) {
        std::cerr << "cannot read file: " << path << '\n';
        return false;
    }
    return true;
}

static bool lex_file(const std::string& path) {
    std::string source, error;
    if (!read_assembly(path, source)) return false;
    const std::unique_ptr<Token, decltype(&destroy)> tokens(lex(source, error), destroy);
    if (!tokens) {
        std::cerr << path << ": " << error << '\n';
        return false;
    }
    for (const Token* token = tokens.get(); token; token = token->next) {
        std::cout << token_name(token->kind);
        if (token->kind == Token::Kind::Ident || token->kind == Token::Kind::Number) {
            std::cout << '(' << token->text << ')';
        }
        std::cout << '\n';
    }
    return true;
}

static std::vector<Byte> assemble_file(const std::string& path) {
    std::string source, error;
    if (!read_assembly(path, source)) return {};
    auto bytes = assemble(source, error);
    if (!error.empty()) std::cerr << path << ": " << error << '\n';
    return bytes;
}

static void load_program(const std::vector<Byte>& bytes, Memory& mem, CPU& cpu) {
    std::fill(mem.data + CODE_LO, mem.data + CODE_HI + 1, Byte{0});
    std::copy(bytes.begin(), bytes.end(), mem.data + CODE_LO);
    cpu = CPU{};
    cpu.mem = &mem;
}

static int run_cli(int argc, char* argv[]) {
    const std::string command = argv[1];
    if (argc == 3 && command == "lex") return lex_file(argv[2]) ? 0 : 1;
    if (argc == 3 && command == "asm") {
        const auto bytes = assemble_file(argv[2]);
        if (bytes.empty()) return 1;
        std::cout << std::uppercase << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (i != 0) std::cout << ' ';
            std::cout << std::setw(2) << static_cast<unsigned>(bytes[i]);
        }
        std::cout << '\n';
        return 0;
    }
    if (argc == 2 && command != "lex" && command != "asm") {
        const auto bytes = assemble_file(command);
        if (bytes.empty()) return 1;
        Memory mem;
        CPU cpu;
        load_program(bytes, mem, cpu);
        const bool halted = run(cpu);
        if (halted) std::cout << '\n';
        return halted ? 0 : 1;
    }
    std::cerr << "usage: ember [<file.asm> | lex <file.asm> | asm <file.asm>]\n";
    return 1;
}

static void print_help() {
    std::cout << "commands:\n"
              << "  dump              print all " << MEM_SIZE << " bytes\n"
              << "  show              draw the 64x32 screen\n"
              << "  sprite <i> <x> <y> <vx> <vy>  set and activate a sprite\n"
              << "  tick              move sprites and draw the next frame\n"
              << "  get <addr>        show one byte four ways\n"
              << "  set <addr> <val>  write one byte (dec or 0x hex)\n"
              << "  get16 <addr>      read a little-endian word\n"
              << "  set16 <addr> <v>  write a little-endian word (0..65535)\n"
              << "  inc <addr>        increment one memory byte (wraps at 255)\n"
              << "  reg <a|b> <val>   set a CPU register (0..255)\n"
              << "  find <lo> <hi> <byte>  first match in [lo, hi)\n"
              << "  regs              show PC A B Z N C H SP\n"
              << "  stack             show SP and the top stack bytes\n"
              << "  lex <file.asm>    print one token per line\n"
              << "  asm <file.asm>    assemble, load at 0x000 and reset the CPU\n"
              << "  step              execute one instruction\n"
              << "  run [limit]       execute to HALT/error (default 100000 steps)\n"
              << "  help              this list\n"
              << "  quit              leave\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) return run_cli(argc, argv);

    Memory mem;  // 4096 bytes, on the stack, zeroed by the {} in memory.hpp
    CPU cpu;
    cpu.mem = &mem;
    Sprite sprites[SPRITE_COUNT]{};

    std::cout << "ember 1.0.0 - give it a language. Type `help`.\n";

    std::string line;
    while (true) {
        std::cout << "ember> ";

        // getline reads one whole line. It returns false at end of input
        // (Ctrl-D), which is how the loop ends if you never type `quit`.
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }

        // Split the line into words: "set 0 65" -> cmd="set", args "0" and "65".
        std::istringstream words(line);
        std::string cmd;
        words >> cmd;

        if (cmd.empty()) {
            continue;  // the user just pressed Enter
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "dump") {
            dump(mem);
        } else if (cmd == "show") {
            show(mem);
        } else if (cmd == "sprite") {
            std::string i_word, x_word, y_word, vx_word, vy_word, extra;
            long i = 0, x = 0, y = 0, vx = 0, vy = 0;
            if (!(words >> i_word >> x_word >> y_word >> vx_word >> vy_word) ||
                !parse_number(i_word, i) || !parse_number(x_word, x) ||
                !parse_number(y_word, y) || !parse_number(vx_word, vx) ||
                !parse_number(vy_word, vy) || (words >> extra)) {
                std::cout << "usage: sprite <i> <x> <y> <vx> <vy>\n";
            } else if (i < 0 || i >= static_cast<long>(SPRITE_COUNT)) {
                std::cout << "sprite index must be 0.." << SPRITE_COUNT - 1 << '\n';
            } else if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
                std::cout << "sprite coordinates must be x=0..63, y=0..31\n";
            } else if (vx < -SPRITE_MAX_SPEED || vx > SPRITE_MAX_SPEED ||
                       vy < -SPRITE_MAX_SPEED || vy > SPRITE_MAX_SPEED) {
                std::cout << "sprite velocities must be -" << SPRITE_MAX_SPEED << ".."
                          << SPRITE_MAX_SPEED << '\n';
            } else {
                sprites[i] = {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
                              static_cast<std::int8_t>(vx), static_cast<std::int8_t>(vy), true};
            }
        } else if (cmd == "tick") {
            std::string extra;
            if (words >> extra) {
                std::cout << "usage: tick\n";
            } else {
                tick_sprites(mem, sprites);
                show(mem);
            }
        } else if (cmd == "regs") {
            dump_regs(cpu);
        } else if (cmd == "stack") {
            dump_stack(cpu);
        } else if (cmd == "lex" || cmd == "asm") {
            std::string path, extra;
            // Preserve Windows backslashes inside double-quoted paths.
            if (!(words >> std::quoted(path, '"', '\0')) || path.empty() || (words >> extra)) {
                std::cout << "usage: " << cmd << " <file.asm>\n";
            } else if (cmd == "lex") {
                lex_file(path);
            } else {
                const auto bytes = assemble_file(path);
                if (!bytes.empty()) {
                    load_program(bytes, mem, cpu);
                    std::cout << "loaded " << bytes.size()
                              << (bytes.size() == 1 ? " byte" : " bytes") << " at 0x000\n";
                }
            }
        } else if (cmd == "step") {
            step(cpu);
        } else if (cmd == "run") {
            std::string limit_word, extra;
            long limit = 100000;
            if ((words >> limit_word && (!parse_number(limit_word, limit) || limit <= 0)) ||
                (words >> extra)) {
                std::cout << "usage: run [positive instruction limit]\n";
            } else {
                run(cpu, static_cast<std::size_t>(limit));
                std::cout << '\n';
            }
        } else if (cmd == "find") {
            std::string l, h, v, extra;
            long lo = 0, hi = 0, value = 0;
            if (!(words >> l >> h >> v) || !parse_number(l, lo) ||
                !parse_number(h, hi) || !parse_number(v, value) || (words >> extra)) {
                std::cout << "usage: find <lo> <hi> <byte>\n";
            } else if (lo < 0 || hi < lo || static_cast<std::size_t>(hi) > MEM_SIZE) {
                std::cout << "find range must satisfy 0 <= lo <= hi <= 4096\n";
            } else if (value < 0 || value > 255) {
                std::cout << "a byte is 0..255\n";
            } else {
                const auto end = static_cast<std::size_t>(hi);
                const auto found = find_byte(mem, static_cast<std::size_t>(lo), end,
                                             static_cast<Byte>(value));
                if (found == end) {
                    std::cout << "not found\n";
                } else {
                    std::cout << "found address=" << found << " index="
                              << found - static_cast<std::size_t>(lo) << '\n';
                }
            }
        } else if (cmd == "get16" || cmd == "set16") {
            std::string a, v, extra;
            long addr = 0, value = 0;
            if (!(words >> a) || !parse_number(a, addr) ||
                (cmd == "set16" && (!(words >> v) || !parse_number(v, value))) ||
                (words >> extra)) {
                std::cout << "usage: " << cmd << " <addr>"
                          << (cmd == "set16" ? " <0..65535>" : "") << '\n';
            } else if (addr < 0 || static_cast<std::size_t>(addr) >= MEM_SIZE - 1) {
                std::cout << "word address must be 0..4094\n";
            } else if (cmd == "set16" && (value < 0 || value > 65535)) {
                std::cout << "a word is 0..65535\n";
            } else if (cmd == "set16") {
                set16(mem, static_cast<std::size_t>(addr), static_cast<std::uint16_t>(value));
            } else {
                std::cout << get16(mem, static_cast<std::size_t>(addr)) << '\n';
            }
        } else if (cmd == "reg") {
            std::string r, v;
            long value = 0;
            if (!(words >> r) || !(words >> v) || !parse_number(v, value) ||
                value < 0 || value > 255) {
                std::cout << "usage: reg <a|b> <0..255>\n";
            } else if (r == "a") {
                cpu.a = static_cast<Byte>(value);
            } else if (r == "b") {
                cpu.b = static_cast<Byte>(value);
            } else {
                std::cout << "unknown register: " << r << '\n';
            }
        } else if (cmd == "inc") {
            std::string a;
            long addr = 0;
            if (!(words >> a) || !parse_number(a, addr)) {
                std::cout << "usage: inc <addr>\n";
            } else if (addr < 0) {
                std::cout << "address must not be negative\n";
            } else if (static_cast<std::size_t>(addr) >= MEM_SIZE) {
                std::cout << "address " << addr << " is outside 0.." << MEM_SIZE - 1
                          << '\n';
            } else {
                const auto address = static_cast<std::size_t>(addr);
                mem_set(mem, address, static_cast<Byte>(mem_get(mem, address) + 1));
            }
        } else if (cmd == "get") {
            std::string a;
            long addr = 0;
            if (!(words >> a) || !parse_number(a, addr)) {
                std::cout << "usage: get <addr>\n";
            } else if (addr < 0) {
                std::cout << "address must not be negative\n";
            } else {
                show_byte(mem_get(mem, (std::size_t)addr));
            }
        } else if (cmd == "set") {
            std::string a, v;
            long addr = 0, value = 0;
            if (!(words >> a) || !(words >> v) || !parse_number(a, addr) ||
                !parse_number(v, value)) {
                std::cout << "usage: set <addr> <value>\n";
            } else if (addr < 0) {
                std::cout << "address must not be negative\n";
            } else if (value < 0 || value > 255) {
                // A cell holds ONE byte. 256 does not fit. Lab 1, theory 3.
                std::cout << "a byte is 0..255, got " << value << '\n';
            } else if (!mem_set(mem, (std::size_t)addr, (Byte)value)) {
                std::cout << "address " << addr << " is outside 0.." << MEM_SIZE - 1
                          << '\n';
            }
        } else {
            std::cout << "unknown command: " << cmd << " (try `help`)\n";
        }
    }

    return 0;  // 0 means "success" to the operating system
}
