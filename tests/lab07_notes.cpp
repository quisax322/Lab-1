#include "lab07_fact.hpp"

#include <iostream>

static void by_value(int x) { ++x; }
static void by_reference(int& x) { ++x; }
static void by_pointer(int* x) { ++*x; }

int main() {
    int a = 1, b = 1, c = 1;
    by_value(a);
    by_reference(b);
    by_pointer(&c);
    std::cout << a << ' ' << b << ' ' << c << '\n';

    const int result = fact(3);
    std::cout << result << '\n';
}
