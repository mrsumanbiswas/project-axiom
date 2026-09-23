#pragma once
#include "axiom/lexer.h"
#include "axiom/token.h"
#include "axiom/ast.h"
#include "axiom/support/error.h"
#include <memory>

namespace axiom {

class Parser {
public:
    explicit Parser(Lexer lexer);

    // Top-level structural grammar entry points
    Expected<std::unique_ptr<FuncNode>> parse_definition();
    Expected<std::unique_ptr<Prototype>> parse_extern();
    Expected<std::unique_ptr<FuncNode>> parse_top_level_expression();

    // Stream queries and navigation for top-level drivers
    [[nodiscard]] bool is_eof() const noexcept { return m_current_tok.kind == TokenKind::Eof; }
    [[nodiscard]] TokenKind current_tok_kind() const noexcept { return m_current_tok.kind; }
    [[nodiscard]] const Token& current_token() const noexcept { return m_current_tok; }
    void consume_token() noexcept { consume(); }

private:
    void consume() noexcept;
    
    // Expressions
    Expected<std::unique_ptr<ExprAST>> parse_primary();
    Expected<std::unique_ptr<ExprAST>> parse_number_expr();
    Expected<std::unique_ptr<ExprAST>> parse_identifier_expr();
    Expected<std::unique_ptr<ExprAST>> parse_paren_expr();
    Expected<std::unique_ptr<ExprAST>> parse_if_expr();
    Expected<std::unique_ptr<ExprAST>> parse_expression();

    // Precedence climbing
    [[nodiscard]] int get_tok_precedence() noexcept;
    Expected<std::unique_ptr<ExprAST>> parse_bin_op_rhs(int expr_prec, std::unique_ptr<ExprAST> lhs);

    // Structural helper
    Expected<std::unique_ptr<Prototype>> parse_prototype();

    Lexer m_lexer;
    Token m_current_tok{};
};

} // namespace axiom