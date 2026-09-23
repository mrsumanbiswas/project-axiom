#pragma once
#include "axiom/support/source_location.h"
#include <string_view>
#include <format>

namespace axiom {

enum class TokenKind {
    Eof,
    Def,
    Extern,
    If,
    Then,
    Else,
    Identifier,
    Number,
    Op
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string_view lexeme{};
    SourceLocation location{};
};

} // namespace axiom

template <>
struct std::formatter<axiom::TokenKind> : std::formatter<std::string_view> {
    auto format(axiom::TokenKind kind, std::format_context& ctx) const {
        std::string_view name = "UNKNOWN";
        switch (kind) {
            case axiom::TokenKind::Eof:        name = "EOF"; break;
            case axiom::TokenKind::Def:        name = "DEF"; break;
            case axiom::TokenKind::Extern:     name = "EXTERN"; break;
            case axiom::TokenKind::If:         name = "IF"; break;
            case axiom::TokenKind::Then:       name = "THEN"; break;
            case axiom::TokenKind::Else:       name = "ELSE"; break;
            case axiom::TokenKind::Identifier: name = "IDENTIFIER"; break;
            case axiom::TokenKind::Number:     name = "NUMBER"; break;
            case axiom::TokenKind::Op:         name = "OPERATOR"; break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct std::formatter<axiom::Token> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const axiom::Token& tok, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{} ('{}') at {}:{}",
                              tok.kind, tok.lexeme, tok.location.line, tok.location.column);
    }
};