// main.cpp — the prompt.
//
// GIVEN, in full. This file is scaffolding: it reads a line, splits it into
// words, and calls one of your functions. It uses `while`, `if` and functions,
// which the course only explains properly in Labs 4 and 7. That is on purpose —
// in Lab 1 you read this file, you do not write it.
//
// What you add in later labs is one more `else if` branch per command.
#include <iostream>
#include <sstream>
#include <string>

#include "dump.hpp"
#include "memory.hpp"
#include "cpu.hpp"

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

static void print_help() {
    std::cout << "commands:\n"
              << "  dump              print all " << MEM_SIZE << " bytes\n"
              << "  get <addr>        show one byte four ways\n"
              << "  set <addr> <val>  write one byte (dec or 0x hex)\n"
              << "  get16 <addr>      read a little-endian word\n"
              << "  set16 <addr> <v>  write a little-endian word (0..65535)\n"
              << "  inc <addr>        increment one memory byte (wraps at 255)\n"
              << "  reg <a|b> <val>   set a CPU register (0..255)\n"
              << "  find <lo> <hi> <byte>  first match in [lo, hi)\n"
              << "  regs              show PC A B Z N C H\n"
              << "  step              execute one instruction\n"
              << "  run [limit]       execute to HALT/error (default 100000 steps)\n"
              << "  help              this list\n"
              << "  quit              leave\n";
}

int main() {
    Memory mem;  // 4096 bytes, on the stack, zeroed by the {} in memory.hpp
    CPU cpu;
    cpu.mem = &mem;

    std::cout << "ember 0.4 - conditions, loops and search. Type `help`.\n";

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
        } else if (cmd == "regs") {
            dump_regs(cpu);
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
