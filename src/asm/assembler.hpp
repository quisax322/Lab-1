#pragma once

#include <string>
#include <vector>

#include "memory.hpp"

// Returns a complete CODE image, or an empty vector and a line-numbered error.
std::vector<Byte> assemble(const std::string& source, std::string& error);
