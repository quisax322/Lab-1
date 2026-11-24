#include "sprite.hpp"

#include "display.hpp"

static void advance_axis(std::uint8_t& position, std::int8_t& velocity, int edge) {
    int next = position + velocity;
    int speed = velocity;
    // Reflect the overshoot, including steps that cross more than one wall.
    while (next < 0 || next > edge) {
        next = next < 0 ? -next : 2 * edge - next;
        speed = -speed;
    }
    if ((next == 0 && speed < 0) || (next == edge && speed > 0)) {
        speed = -speed;
    }
    position = static_cast<std::uint8_t>(next);
    velocity = static_cast<std::int8_t>(speed);
}

void tick_sprites(Memory& mem, Sprite (&sprites)[SPRITE_COUNT]) {
    clear(mem);
    for (auto& sprite : sprites) {
        if (!sprite.alive) continue;
        advance_axis(sprite.x, sprite.vx, SCREEN_WIDTH - 1);
        advance_axis(sprite.y, sprite.vy, SCREEN_HEIGHT - 1);
        plot(mem, sprite.x, sprite.y);
    }
}
