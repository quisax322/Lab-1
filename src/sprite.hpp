#pragma once

#include "memory.hpp"

#include <limits>

struct Sprite {
    std::uint8_t x, y;
    std::int8_t vx, vy;
    bool alive;
};

constexpr std::size_t SPRITE_COUNT = 8;
// Both the original speed and its negation must fit in int8_t.
constexpr int SPRITE_MAX_SPEED = std::numeric_limits<std::int8_t>::max();

// Active sprites must have on-screen coordinates and reversible velocities.
void tick_sprites(Memory& mem, Sprite (&sprites)[SPRITE_COUNT]);
