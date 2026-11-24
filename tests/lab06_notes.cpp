#include "cpu.hpp"
#include "sprite.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>

struct P {
    char c;
    int n;
};

struct Value {
    enum class Kind : std::uint8_t { Byte, Addr, SpriteId };
    Kind kind;
    std::uint16_t bits;
};

union Payload {
    std::uint8_t byte;
    std::uint16_t address;
};

struct Lifetime {
    const char* name;

    ~Lifetime() { std::cout << "destroy " << name << '\n'; }
};

static Lifetime global{"static"};

int main() {
    static_assert(std::is_standard_layout_v<CPU> && std::is_standard_layout_v<Flags> &&
                  std::is_standard_layout_v<Sprite>);
    std::cout << "P sizeof=" << sizeof(P) << " alignof=" << alignof(P)
              << " payload=" << sizeof(char) + sizeof(int)
              << " offsets c=" << offsetof(P, c) << " n=" << offsetof(P, n) << '\n';
    std::cout << "CPU sizeof=" << sizeof(CPU) << " alignof=" << alignof(CPU)
              << " offsets mem=" << offsetof(CPU, mem) << " pc=" << offsetof(CPU, pc)
              << " h=" << offsetof(CPU, h) << " a=" << offsetof(CPU, a)
              << " b=" << offsetof(CPU, b) << " f=" << offsetof(CPU, f)
              << " halted=" << offsetof(CPU, halted) << " error=" << offsetof(CPU, error) << '\n';
    std::cout << "Flags sizeof=" << sizeof(Flags) << " alignof=" << alignof(Flags)
              << " offsets z=" << offsetof(Flags, z) << " n=" << offsetof(Flags, n)
              << " c=" << offsetof(Flags, c) << '\n';
    std::cout << "Sprite sizeof=" << sizeof(Sprite) << " alignof=" << alignof(Sprite)
              << " offsets x=" << offsetof(Sprite, x) << " y=" << offsetof(Sprite, y)
              << " vx=" << offsetof(Sprite, vx) << " vy=" << offsetof(Sprite, vy)
              << " alive=" << offsetof(Sprite, alive) << '\n';
    std::cout << "pointer bytes=" << sizeof(void*) << " Sprite[8] bytes="
              << sizeof(Sprite[SPRITE_COUNT]) << '\n';
    const Sprite idle{};
    std::cout << "zero-initialized Sprite: " << +idle.x << ' ' << +idle.y << ' '
              << +idle.vx << ' ' << +idle.vy << ' ' << idle.alive << '\n';

    Op op = Op::Add;
#ifdef EMBER_LAB06_ENUM_REJECT_INT
    op = 1;
#endif
    op = static_cast<Op>(0x10);
    std::cout << "Op sizeof=" << sizeof(op) << " Add=" << static_cast<int>(op) << '\n';

    const Value value{Value::Kind::Addr, 0x800};
    Payload payload{};
    std::cout << "Value sizeof=" << sizeof(value) << " alignof=" << alignof(Value)
              << " offsets kind=" << offsetof(Value, kind) << " bits=" << offsetof(Value, bits)
              << " Payload sizeof=" << sizeof(payload);
    if (value.kind == Value::Kind::Addr) {
        payload.address = value.bits;
        std::cout << " address=" << payload.address;
    }
    std::cout << '\n';

    Lifetime* heap = nullptr;
    {
        Lifetime local{"automatic"};
        heap = new Lifetime{"dynamic"};
        std::cout << "static @" << &global << " automatic @" << &local
                  << " dynamic @" << heap << '\n';
        const auto sprite = std::make_unique<Sprite>(Sprite{10, 5, 1, 0, true});
        std::cout << "unique_ptr Sprite.x=" << +sprite->x << '\n';
    }
    std::cout << "after local scope: " << heap->name << " still alive\n";
    delete heap;
    heap = nullptr;
    std::cout << "leaving main\n";
}
