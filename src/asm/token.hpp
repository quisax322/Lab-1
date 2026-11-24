#pragma once

#include <string>

struct Token {
    enum class Kind { Ident, Number, Comma, Colon, LBracket, RBracket, Newline, Eof };
    Kind kind;
    std::string text;
    int line;
    Token* next = nullptr;
};

void destroy(Token* head);
