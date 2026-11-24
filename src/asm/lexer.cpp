#include "lexer.hpp"

#include <cctype>
#include <utility>

#include "number.hpp"

namespace {

struct Lexer {
    const std::string& src;
    std::size_t i = 0;
    int line = 1;
    Token* head = nullptr;
    Token* tail = nullptr;
    std::string error;

    ~Lexer() { destroy(head); }
};

void emit(Lexer& lx, Token::Kind kind, std::string text) {
    auto* token = new Token{kind, std::move(text), lx.line, nullptr};
    if (!lx.head) {
        lx.head = lx.tail = token;
    } else {
        lx.tail->next = token;
        lx.tail = token;
    }
}

bool fail(Lexer& lx, const std::string& message) {
    lx.error = "line " + std::to_string(lx.line) + ": " + message;
    return false;
}

bool word_char(unsigned char c) {
    return std::isalnum(c) || c == '_';
}

std::string take_word(Lexer& lx) {
    const auto start = lx.i++;
    while (lx.i < lx.src.size() && word_char(static_cast<unsigned char>(lx.src[lx.i]))) {
        ++lx.i;
    }
    return lx.src.substr(start, lx.i - start);
}

bool number(Lexer& lx) {
    auto text = take_word(lx);
    std::uint64_t value = 0;
    const auto status = parse_number_literal(text, value);
    if (status == std::errc::result_out_of_range) return fail(lx, "number overflow");
    if (status != std::errc{}) return fail(lx, "bad number");
    emit(lx, Token::Kind::Number, std::move(text));
    return true;
}

} // namespace

void destroy(Token* head) {
    while (head) {
        Token* next = head->next;
        delete head;
        head = next;
    }
}

Token* lex(const std::string& src, std::string& error) {
    using Kind = Token::Kind;
    error.clear();
    Lexer lx{src, 0, 1, nullptr, nullptr, {}};
    if (src.compare(0, 3, "\xEF\xBB\xBF") == 0) lx.i = 3;
    while (lx.i < src.size()) {
        const char c = src[lx.i];
        const auto ch = static_cast<unsigned char>(c);
        if (c == ' ' || c == '\t' || c == '\r') {
            ++lx.i;
        } else if (c == ';') {
            while (lx.i < src.size() && src[lx.i] != '\n') ++lx.i;
        } else if (c == '\n') {
            emit(lx, Kind::Newline, "\\n");
            ++lx.i;
            ++lx.line;
        } else if (std::isalpha(ch) || c == '_') {
            emit(lx, Kind::Ident, take_word(lx));
        } else if (std::isdigit(ch)) {
            if (!number(lx)) break;
        } else if (c == ',') {
            emit(lx, Kind::Comma, ",");
            ++lx.i;
        } else if (c == ':') {
            emit(lx, Kind::Colon, ":");
            ++lx.i;
        } else if (c == '[' || c == ']') {
            emit(lx, c == '[' ? Kind::LBracket : Kind::RBracket, std::string(1, c));
            ++lx.i;
        } else {
            fail(lx, std::isprint(ch) ? std::string("unexpected character '") + c + "'"
                                     : "unexpected non-printable byte");
            break;
        }
    }
    if (!lx.error.empty()) {
        error = std::move(lx.error);
        return nullptr;
    }
    emit(lx, Kind::Eof, "");
    return std::exchange(lx.head, nullptr);
}
