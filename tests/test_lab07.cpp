#include "cpu.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

static void expect_fault(CPU& cpu, const char* error) {
    const CPU before = cpu;
    const Memory memory_before = *cpu.mem;
    std::ostringstream diagnostics;
    auto* saved = std::cout.rdbuf(diagnostics.rdbuf());
    step(cpu);
    step(cpu);
    const bool completed = run(cpu);
    std::cout.rdbuf(saved);

    require(!completed && cpu.error && std::string(cpu.error) == error, "expected latched fault");
    require(diagnostics.str() == "CPU error: " + std::string(error) +
            " (PC=" + std::to_string(before.pc) + ")\n", "one diagnostic for the fault");
    require(cpu.pc == before.pc && cpu.stack.sp() == before.stack.sp() &&
            cpu.a == before.a && cpu.b == before.b && cpu.h == before.h &&
            cpu.f.z == before.f.z && cpu.f.n == before.f.n && cpu.f.c == before.f.c &&
            cpu.halted == before.halted, "fault preserves CPU state except the error latch");
    require(std::equal(std::begin(cpu.mem->data), std::end(cpu.mem->data), memory_before.data),
            "fault preserves all bytes");
}

static void test_register_stack() {
    Memory mem;
    const Byte code[] = {0x50, 0x52, 0x51, 0x53, 0x00};
    std::copy(std::begin(code), std::end(code), mem.data);
    CPU cpu;
    cpu.mem = &mem;
    cpu.a = 0xA1;
    cpu.b = 0xB2;
    cpu.h = 0x800;
    cpu.f = {true, false, true};

    require(cpu.stack.sp() == 0xFFF, "initial SP");
    step(cpu);
    require(cpu.pc == 1 && cpu.stack.sp() == 0xFFE &&
            mem_get(mem, 0xFFF) == 0xA1 && cpu.a == 0xA1 && cpu.b == 0xB2, "PUSH A");
    step(cpu);
    require(cpu.pc == 2 && cpu.stack.sp() == 0xFFD &&
            mem_get(mem, 0xFFE) == 0xB2 && cpu.a == 0xA1 && cpu.b == 0xB2, "PUSH B");
    step(cpu);
    require(cpu.pc == 3 && cpu.stack.sp() == 0xFFE &&
            cpu.a == 0xB2 && cpu.b == 0xB2, "POP A takes most recently pushed byte");
    step(cpu);
    require(cpu.pc == 4 && cpu.stack.sp() == 0xFFF && cpu.a == 0xB2 &&
            cpu.b == 0xA1 && cpu.h == 0x800 && cpu.f.z && !cpu.f.n && cpu.f.c,
            "POP B completes LIFO and stack instructions preserve H and flags");
    require(mem_get(mem, 0xFFE) == 0xB2 && mem_get(mem, 0xFFF) == 0xA1,
            "POP only removes bytes from the live stack");
    require(run(cpu, 1) && cpu.pc == 5, "stack instructions advance past their opcode");
}

static void test_stack_limits() {
    Memory mem;
    std::fill_n(mem.data, MEM_SIZE, Byte{0xA5});
    mem.data[0] = 0x54; // CALL 0x0100
    set16(mem, 1, 0x0100);
    const Memory before = mem;
    CPU cpu;
    cpu.mem = &mem;
    Byte value = 0xBE;
    require(!cpu.stack.pop(mem, value) && value == 0xBE, "empty pop leaves output unchanged");
    for (unsigned i = 0; i < 255; ++i) {
        require(cpu.stack.push(mem, static_cast<Byte>(i)), "first 255 bytes fit");
    }
    Byte top = 0;
    require(cpu.stack.sp() == 0xF00 && cpu.stack.peek(mem, 0, top) && top == 254 &&
            !cpu.stack.peek(mem, 255, top) && top == 254, "peek only reads live stack bytes");
    CPU one_free = cpu;
    expect_fault(one_free, "stack overflow");

    require(cpu.stack.push(mem, 255) && cpu.stack.sp() == 0xEFF &&
            mem_get(mem, 0xF00) == 255, "256th byte fits at 0xF00");
    require(std::equal(mem.data, mem.data + 0xF00, before.data),
            "pushes preserve every byte outside the stack region");
    for (Byte op : {Byte{0x50}, Byte{0x52}}) {
        mem.data[0] = op;
        CPU full = cpu;
        expect_fault(full, "stack overflow");
    }
    for (unsigned remaining = 256; remaining != 0; --remaining) {
        require(cpu.stack.pop(mem, value) && value == remaining - 1, "full-region LIFO");
    }
    require(cpu.stack.sp() == 0xFFF && !cpu.stack.pop(mem, value), "all 256 bytes removed");
    for (Byte op : {Byte{0x51}, Byte{0x53}, Byte{0x55}}) {
        mem.data[0] = op;
        CPU empty = cpu;
        expect_fault(empty, "stack underflow");
    }
    mem.data[0] = 0x55;
    require(cpu.stack.push(mem, 0x32), "stage a single byte for RET");
    expect_fault(cpu, "stack underflow");
}

static void test_calls() {
    Memory mem;
    CPU cpu;
    cpu.mem = &mem;
    cpu.pc = 0x123;
    cpu.a = 19;
    cpu.b = 20;
    cpu.h = 0x803;
    cpu.f = {false, true, true};
    mem.data[0x123] = 0x54;
    set16(mem, 0x124, 0x456);
    mem.data[0x456] = 0x54;
    set16(mem, 0x457, 0x789);
    mem.data[0x789] = mem.data[0x459] = 0x55;

    step(cpu);
    require(cpu.pc == 0x456 && cpu.stack.sp() == 0xFFD &&
            mem_get(mem, 0xFFF) == 0x26 && mem_get(mem, 0xFFE) == 0x01,
            "CALL stores low then high byte of PC+3 before jumping");
    step(cpu);
    require(cpu.pc == 0x789 && cpu.stack.sp() == 0xFFB &&
            mem_get(mem, 0xFFD) == 0x59 && mem_get(mem, 0xFFC) == 0x04, "nested CALL");
    step(cpu);
    require(cpu.pc == 0x459 && cpu.stack.sp() == 0xFFD, "inner RET pops high then low");
    step(cpu);
    require(cpu.pc == 0x126 && cpu.stack.sp() == 0xFFF && cpu.error == nullptr &&
            cpu.a == 19 && cpu.b == 20 && cpu.h == 0x803 && !cpu.f.z && cpu.f.n && cpu.f.c,
            "outer RET and register preservation");

    for (std::uint16_t target : {std::uint16_t{0x1000}, std::uint16_t{0xFFFF}}) {
        CPU bad_call = cpu;
        bad_call.pc = 0x123;
        set16(mem, 0x124, target);
        expect_fault(bad_call, "call target outside memory");

        CPU bad_return = cpu;
        bad_return.pc = 0x789;
        require(bad_return.stack.push_address(mem, target), "stage an invalid return address");
        expect_fault(bad_return, "return target outside memory");
    }
    for (std::uint16_t pc : {std::uint16_t{0xFFE}, std::uint16_t{0xFFF}}) {
        CPU truncated_call = cpu;
        truncated_call.pc = pc;
        mem.data[pc] = 0x54;
        expect_fault(truncated_call, "instruction operand outside memory");
    }
}

static void test_mul() {
    Memory mem;
    mem.data[0] = 0x1B;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned b = 0; b < 256; ++b) {
            const unsigned product = a * b;
            const unsigned wrapped = product % 256;
            CPU cpu;
            cpu.mem = &mem;
            cpu.a = static_cast<Byte>(a);
            cpu.b = static_cast<Byte>(b);
            cpu.f = {wrapped != 0, wrapped < 128, product < 256};
            step(cpu);
            require(cpu.a == wrapped && cpu.b == b && !cpu.error && cpu.pc == 1,
                    "MUL wraps in A and preserves B");
            require(cpu.f.z == (wrapped == 0) && cpu.f.n == (wrapped >= 128) &&
                    cpu.f.c == (product >= 256), "MUL overwrites Z, N and C");
        }
    }
}

static void test_factorial(Byte n, unsigned result, unsigned steps, unsigned max_bytes, bool carry) {
    // The 30-byte listing in ISA.uk.md §6, with only the first immediate varied.
    const Byte code[] = {
        0x20, n, 0x54, 0x07, 0x00, 0x03, 0x00, 0x21, 0x00, 0x1A,
        0x31, 0x1B, 0x00, 0x21, 0x01, 0x1A, 0x31, 0x1B, 0x00, 0x50,
        0x19, 0x54, 0x07, 0x00, 0x53, 0x1B, 0x55, 0x20, 0x01, 0x55
    };
    Memory mem;
    std::copy(std::begin(code), std::end(code), mem.data);
    CPU cpu;
    cpu.mem = &mem;
    unsigned count = 0;
    std::uint16_t min_sp = cpu.stack.sp();
    std::ostringstream guest_output;
    std::cout << "fact(" << static_cast<unsigned>(n) << ")\n";
    while (!cpu.halted && !cpu.error && count < steps) {
        const auto op = mem_get(mem, cpu.pc);
        const CPU before = cpu;
        auto* saved = std::cout.rdbuf(guest_output.rdbuf());
        step(cpu);
        std::cout.rdbuf(saved);
        ++count;
        min_sp = std::min(min_sp, cpu.stack.sp());
        if (op == 0x54 || op == 0x55) {
            std::cout << std::dec << count << (op == 0x54 ? " CALL" : " RET ")
                      << std::hex << std::uppercase
                      << " PC=" << before.pc << "->" << cpu.pc
                      << " SP=" << before.stack.sp() << "->" << cpu.stack.sp()
                      << std::dec << " A=" << static_cast<unsigned>(cpu.a) << '\n';
            require(cpu.a == before.a, "CALL and RET preserve A");
        }
    }
    require(cpu.halted && !cpu.error && count == steps && cpu.pc == 7, "factorial HALT and step count");
    require(guest_output.str() == std::to_string(result) + " " && cpu.a == result &&
            cpu.f.c == carry, "factorial output and carry");
    require(cpu.stack.sp() == 0xFFF && min_sp == 0xFFFu - max_bytes, "factorial stack depth and balance");
    std::cout << "result=" << result << " steps=" << count << " max_bytes=" << max_bytes
              << " C=" << cpu.f.c << '\n';
}

static void test_unbounded_recursion() {
    Memory mem;
    mem.data[0] = 0x54; // CALL 0
    mem.data[0xEFF] = 0xA5;
    CPU cpu;
    cpu.mem = &mem;
    for (unsigned i = 0; i < 128; ++i) {
        step(cpu);
        require(!cpu.error && cpu.pc == 0 && cpu.stack.sp() == 0xFFFu - 2 * (i + 1),
                "each recursive call consumes exactly two bytes");
    }
    require(mem_get(mem, 0xF00) == 0 && mem_get(mem, 0xF01) == 3 &&
            mem_get(mem, 0xEFF) == 0xA5, "last full frame fits within the stack region");
    expect_fault(cpu, "stack overflow");
}

int main() {
    test_register_stack();
    test_stack_limits();
    test_calls();
    test_mul();
    test_factorial(0, 1, 9, 2, false);
    test_factorial(1, 1, 12, 2, false);
    test_factorial(5, 120, 60, 14, false);
    test_factorial(6, 208, 72, 17, true);
    test_unbounded_recursion();
    std::cout << "PASS Lab 07 stack, calls and all 65536 MUL input pairs\n";
}
