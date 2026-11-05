#include <iostream>
static void lifetime() {
    static int a = 0;
    int b = 0;
    ++a; ++b;
    std::cout << a << ' ' << b << '\n';
}
int main() {
    int x = 1;
    {
        int x = 10;
        static int c = 0;
        ++c;
        std::cout << x << ' ' << c << '\n';
    }
    std::cout << x << '\n';
    for (int i = 0; i < 3; ++i) lifetime();
    // x from the inner block and c are out of scope here; no invalid access.
}
