#include "lab07_fact.hpp"

#include <iostream>

int fact(int n) {
    std::cout << '>' << n << '\n';
    if (n <= 1) return 1;
    const int result = n * fact(n - 1);
    std::cout << '<' << n << '\n';
    return result;
}
