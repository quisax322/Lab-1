#include <cstdint>
#include <cstring>
#include <iostream>

static void inc_copy(int value) { value = value + 1; }
static void inc_ptr(int* ptr) { if (ptr != nullptr) *ptr = *ptr + 1; }

int main() {
    int x = 65;
    int* p = &x;
    std::cout << "x=" << x << " *p=" << *p << '\n';
    std::cout << "&x=" << &x << " p=" << p << " &p=" << &p << '\n';
    *p = 66;
    if (x != 66 || p != &x) return 1;
    std::cout << "after *p=66: x=" << x << '\n';
    int* empty = nullptr;
    inc_ptr(empty); // checked, never dereferenced
    std::cout << "nullptr check=" << (empty == nullptr) << '\n';
    std::cout << "sizeof(p)=" << sizeof(p) << " sizeof(*p)=" << sizeof(*p)
              << " sizeof(void*)=" << sizeof(void*) << '\n';
    int a[3] = {10, 20, 30};
    p = a;
    std::cout << "int elements=" << *p << ' ' << *(p + 1) << ' ' << *(p + 2)
              << " distance=" << (p + 1) - p
              << " bytes per step=" << sizeof(*p) << '\n';
    char chars[3] = {'A', 'B', 'C'};
    char* c = chars;
    std::cout << "char next=" << *(c + 1) << " distance=" << (c + 1) - c
              << " bytes per step=" << sizeof(*c) << '\n';
    void* raw = &x;
    int* restored = static_cast<int*>(raw);
    std::cout << "void* roundtrip=" << (restored == &x) << " value=" << *restored << '\n';
    x = 1;
    inc_copy(x);
    std::cout << "after inc_copy=" << x << '\n';
    if (x != 1) return 1;
    inc_ptr(&x);
    std::cout << "after inc_ptr=" << x << '\n';
    if (x != 2 || *(p + 1) != 20 || *(c + 1) != 'B') return 1;
    const float f = 1.0f;
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(f), "32-bit float required for this experiment");
    std::memcpy(&bits, &f, sizeof(bits));
    std::cout << "float bits via memcpy=0x" << std::hex << bits << '\n';
    return 0;
}
