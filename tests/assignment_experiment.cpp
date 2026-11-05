// Deliberately isolated Notes 04 experiment: compile with warnings, without /WX.
#include <iostream>
int main() {
    int x = 3;
    if (x = 1) std::cout << "yes " << x << '\n';
    if (x == 0) std::cout << "zero\n";
    if (x = 0) std::cout << "unreachable\n";
    std::cout << "after assignment zero: " << x << '\n';
    if (x == 0) std::cout << "comparison true\n";
}
