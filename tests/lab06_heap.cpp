#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    const std::string_view experiment = argc == 2 ? argv[1] : "";
    if (experiment != "leak" && experiment != "use-after-free" && experiment != "double-free") {
        std::cerr << "usage: lab06_heap <leak|use-after-free|double-free>\n";
        return 2;
    }

    int* p = new int{42};
    std::cout << experiment << ": live value " << *p << std::endl;

    if (experiment == "leak") {
#ifndef EMBER_ASAN_DEMO_UNSAFE
        delete p;
#endif
        p = nullptr;
        return 0;
    }

    delete p;
#ifndef EMBER_ASAN_DEMO_UNSAFE
    p = nullptr;
#endif
    if (experiment == "use-after-free") {
#ifdef EMBER_ASAN_DEMO_UNSAFE
        std::cout << *p << '\n';
#endif
    } else {
        // In the fixed build this is delete nullptr, which is safe.
        delete p;
    }
}
