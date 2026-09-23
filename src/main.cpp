#include "axiom/codegen.h"
#include "axiom/ast.h"
#include <print>
#include <memory>
#include <vector>

int main() {
    std::println(">_ Testing Expression & Operator Codegen");

    axiom::CodeGenerator cg;

    // Build: def square(x) x * x
    std::vector<std::string> args = {"x"};
    auto proto = std::make_unique<axiom::Prototype>("square", std::move(args));
    auto lhs = std::make_unique<axiom::VarExpr>("x");
    auto rhs = std::make_unique<axiom::VarExpr>("x");
    auto body = std::make_unique<axiom::BinaryExpr>('*', std::move(lhs), std::move(rhs));

    axiom::FuncNode func(std::move(proto), std::move(body));

    llvm::Function* lf = func.codegen(cg);

    if (lf) {
        std::println("Generated LLVM IR Function:");
        cg.dump();
    } else {
        std::println("Failed to generate function IR.");
    }

    return 0;
}