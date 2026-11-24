#include <iostream>

int main() {
#ifdef EMBER_ASAN_DEMO_UNSAFE
    char text[] = "AB";
    text[2] = 'X';
    std::cout << text << '\n';
#else
    int a[] = {1, 2, 3};
    const int* first = a;
    std::cout << "sizeof array=" << sizeof(a) << " pointer=" << sizeof(first) << '\n';
    std::cout << "index and pointer=" << a[1] << ' ' << *(first + 1) << '\n';

    const int matrix[2][3] = {{1, 2, 3}, {4, 5, 6}};
    const int flat[] = {1, 2, 3, 4, 5, 6};
    const int y = 1, x = 2, width = 3;
    std::cout << "matrix and flat=" << matrix[y][x] << ' ' << flat[y * width + x] << '\n';

    const char text[] = "AB";
    std::cout << "string bytes=" << int(text[0]) << ' ' << int(text[1]) << ' '
              << int(text[2]) << '\n';
    std::cout << "string=" << text << " digit=" << static_cast<char>('0' + 3) << '\n';

    int values[] = {4, 1, 3, 2};
    for (int i = 0; i < 3; ++i) {
        if (values[i] > values[i + 1]) {
            const int previous = values[i];
            values[i] = values[i + 1];
            values[i + 1] = previous;
        }
        std::cout << "bubble " << (i + 1) << '=' << values[0] << ' ' << values[1]
                  << ' ' << values[2] << ' ' << values[3] << '\n';
    }
#endif
}
