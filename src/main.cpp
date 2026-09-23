#include "axiom/lexer.h"
#include "axiom/parser.h"
#include "axiom/codegen.h"
#include "axiom/jit.h"
#include "axiom/ast.h"
#include <llvm/Support/Error.h>
#include <iostream>
#include <print>
#include <string>

void handle_definition(axiom::Parser& parser, axiom::CodeGenerator& cg, axiom::AxiomJIT& jit) {
    auto func_ast = parser.parse_definition();
    if (!func_ast) {
        axiom::report_error(func_ast.error());
        parser.consume_token();
        return;
    }

    if ((*func_ast)->codegen(cg)) {
        std::println(std::cerr, "; Defined function: {}", (*func_ast)->proto()->name());
        auto add_res = jit.add_module(cg.take_module(), cg.take_context());
        if (!add_res) {
            axiom::report_error(add_res.error());
        }
        cg.reinitialize();
    }
}

void handle_extern(axiom::Parser& parser, axiom::CodeGenerator& cg) {
    auto proto_ast = parser.parse_extern();
    if (!proto_ast) {
        axiom::report_error(proto_ast.error());
        parser.consume_token();
        return;
    }

    std::println(std::cerr, "; Registered extern: {}", (*proto_ast)->name());
    cg.add_prototype(std::move(*proto_ast));
}

void handle_top_level_expression(axiom::Parser& parser, axiom::CodeGenerator& cg, axiom::AxiomJIT& jit) {
    auto expr_ast = parser.parse_top_level_expression();
    if (!expr_ast) {
        axiom::report_error(expr_ast.error());
        parser.consume_token();
        return;
    }

    if ((*expr_ast)->codegen(cg)) {
        auto rt = jit.create_resource_tracker();
        auto add_res = jit.add_module(cg.take_module(), cg.take_context(), rt);
        if (!add_res) {
            axiom::report_error(add_res.error());
            cg.reinitialize();
            return;
        }
        cg.reinitialize();

        auto sym_res = jit.lookup("__anon_expr");
        if (!sym_res) {
            axiom::report_error(sym_res.error());
        } else {
            auto fn_ptr = reinterpret_cast<double (*)()>(*sym_res);
            double result = fn_ptr();
            std::println("Evaluated to: {}", result);
        }

        // Deallocate the temporary anonymous function from JIT memory
        if (llvm::Error err = rt->remove()) {
            llvm::consumeError(std::move(err));
        }
    }
}

int main() {
    std::println("=== Axiom JIT Compiler (C++23 / LLVM OrcJIT) ===");
    std::println("Type expressions or functions ending with ';'. Press Ctrl+D to exit.\n");

    auto jit_res = axiom::AxiomJIT::create();
    if (!jit_res) {
        axiom::report_error(jit_res.error());
        return 1;
    }
    auto jit = std::move(*jit_res);

    axiom::CodeGenerator cg(jit->data_layout());

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
            if (parser.current_tok_kind() == axiom::TokenKind::Op && 
                parser.current_token().lexeme == ";") {
                parser.consume_token();
                continue;
            }

            switch (parser.current_tok_kind()) {
                case axiom::TokenKind::Def:
                    handle_definition(parser, cg, *jit);
                    break;
                case axiom::TokenKind::Extern:
                    handle_extern(parser, cg);
                    break;
                default:
                    handle_top_level_expression(parser, cg, *jit);
                    break;
            }
        }
    }

    return 0;
}