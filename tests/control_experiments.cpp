#include <iostream>
bool a() { std::cout << 'A'; return false; }
bool b() { std::cout << 'B'; return true; }
int main() {
    if (a() && b()) {} std::cout << '\n';
    if (a() || b()) {} std::cout << '\n';
    if (b() || a()) {} std::cout << '\n';
    if (b() && a()) {} std::cout << '\n';
    int* p = nullptr;
    std::cout << "null guard=" << (p && *p == 3) << '\n';
    std::cout << "logical=" << (1 && 2) << " bitwise=" << (1 & 2) << '\n';
    int n = 1;
    switch (n) {
    case 1: std::cout << '1'; [[fallthrough]];
    case 2: std::cout << '2'; break;
    default: std::cout << 'D';
    }
    std::cout << '\n';
    if (n == 1) { std::cout << "if one\n"; } else { std::cout << "else\n"; }
    if (n == 0) { std::cout << "zero\n"; } else { std::cout << "else nonzero\n"; }
    int count = 0;
    while (false) { ++count; }
    std::cout << "while=" << count << '\n';
    do { ++count; } while (false);
    std::cout << "do-while=" << count << '\n';
    std::cout << "for=";
    for (int i = 0; i < 3; ++i) std::cout << i << ' ';
    std::cout << '\n';
    int value = 3;
    std::cout << "while countdown=";
    while (value != 0) { std::cout << value << ' '; --value; }
    std::cout << '\n';
}
