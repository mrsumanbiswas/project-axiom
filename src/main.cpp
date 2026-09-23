#include "axiom/lexer.h"
#include "axiom/parser.h"
#include "axiom/codegen.h"
#include "axiom/ast.h"
#include <llvm/IR/Function.h>
#include <iostream>
#include <print>
#include <string>

void handle_definition(axiom::Parser& parser, axiom::CodeGenerator& cg) {
    auto func_ast = parser.parse_definition();
    if (!func_ast) {
        axiom::report_error(func_ast.error());
        parser.consume_token(); // recover
        return;
    }

    if (llvm::Function* lf = (*func_ast)->codegen(cg)) {
        std::println(std::cerr, "; Parsed a function definition:");
        lf->print(llvm::errs());
        std::println(std::cerr, "");
    }
}

void handle_extern(axiom::Parser& parser, axiom::CodeGenerator& cg) {
    auto proto_ast = parser.parse_extern();
    if (!proto_ast) {
        axiom::report_error(proto_ast.error());
        parser.consume_token(); // recover
        return;
    }

    if (llvm::Function* lf = (*proto_ast)->codegen(cg)) {
        std::println(std::cerr, "; Parsed an extern declaration:");
        lf->print(llvm::errs());
        std::println(std::cerr, "");
    }
}

void handle_top_level_expression(axiom::Parser& parser, axiom::CodeGenerator& cg) {
    auto expr_ast = parser.parse_top_level_expression();
    if (!expr_ast) {
        axiom::report_error(expr_ast.error());
        parser.consume_token(); // recover
        return;
    }

    if (llvm::Function* lf = (*expr_ast)->codegen(cg)) {
        std::println(std::cerr, "; Parsed a top-level expression:");
        lf->print(llvm::errs());
        std::println(std::cerr, "");

        // Remove the temporary anonymous function so it doesn't pollute the module
        lf->eraseFromParent();
    }
}

int main() {
    std::println("=== Axiom Kaleidoscope Compiler (C++23 / LLVM) ===");
    std::println("Type expressions or function definitions ending with ';'. Press Ctrl+D to exit.\n");

    axiom::CodeGenerator cg;

    while (true) {
        std::print("axiom> ");
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::println("\nExiting Axiom REPL.");
            break;
        }

        if (line.empty()) continue;

        axiom::Lexer lexer(line);
        axiom::Parser parser(std::move(lexer));

        while (!parser.is_eof()) {
            // Ignore stray semicolons
            if (parser.current_tok_kind() == axiom::TokenKind::Op && 
                parser.current_token().lexeme == ";") {
                parser.consume_token();
                continue;
            }

            switch (parser.current_tok_kind()) {
                case axiom::TokenKind::Def:
                    handle_definition(parser, cg);
                    break;
                case axiom::TokenKind::Extern:
                    handle_extern(parser, cg);
                    break;
                default:
                    handle_top_level_expression(parser, cg);
                    break;
            }
        }
    }

    return 0;
}