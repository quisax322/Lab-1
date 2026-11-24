#pragma once

#include "token.hpp"

// On success the caller owns the list, including its final Eof node.
Token* lex(const std::string& src, std::string& error);
