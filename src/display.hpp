#pragma once

#include "memory.hpp"

constexpr int SCREEN_WIDTH = 64;
constexpr int SCREEN_HEIGHT = 32;
constexpr int SCREEN_ROW_BYTES = SCREEN_WIDTH / 8;
constexpr std::size_t VRAM_LO = 0xA00;
constexpr std::size_t VRAM_HI = VRAM_LO + SCREEN_HEIGHT * SCREEN_ROW_BYTES - 1;

void show(const Memory& mem);
// Set one pixel. Return false for coordinates outside the screen.
bool plot(Memory& mem, int x, int y);
void clear(Memory& mem);
