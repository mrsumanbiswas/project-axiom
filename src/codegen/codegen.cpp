#include "axiom/codegen.h"
#include "axiom/ast.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include <iostream>
#include <vector>

namespace axiom {

CodeGenerator::CodeGenerator() {
    m_context = std::make_unique<llvm::LLVMContext>();
    m_builder = std::make_unique<llvm::IRBuilder<>>(*m_context);
    m_module = std::make_unique<llvm::Module>("AxiomJIT", *m_context);
}

void CodeGenerator::dump() const {
    m_module->print(llvm::errs(), nullptr);
}

// 1. Literal Numbers
llvm::Value* NumExpr::codegen(CodeGenerator& cg) {
    return llvm::ConstantFP::get(cg.context(), llvm::APFloat(m_val));
}

// 2. Variable Lookups
llvm::Value* VarExpr::codegen(CodeGenerator& cg) {
    llvm::Value* val = cg.named_values()[m_name];
    if (!val) {
        std::cerr << "Error: Unknown variable reference name: " << m_name << "\n";
        return nullptr;
    }
    return val;
}

// 3. Binary Operators and Comparisons
llvm::Value* BinaryExpr::codegen(CodeGenerator& cg) {
    llvm::Value* L = m_lhs->codegen(cg);
    llvm::Value* R = m_rhs->codegen(cg);
    if (!L || !R) return nullptr;

    switch (m_op) {
        case '+':
            return cg.builder().CreateFAdd(L, R, "addtmp");
        case '-':
            return cg.builder().CreateFSub(L, R, "subtmp");
        case '*':
            return cg.builder().CreateFMul(L, R, "multmp");
        case '<': {
            L = cg.builder().CreateFCmpULT(L, R, "cmptmp");
            return cg.builder().CreateUIToFP(L, llvm::Type::getDoubleTy(cg.context()), "booltmp");
        }
        default:
            std::cerr << "Error: Invalid binary operator: " << m_op << "\n";
            return nullptr;
    }
}

// 4. Function Invocations
llvm::Value* CallExpr::codegen(CodeGenerator& cg) {
    llvm::Function* callee_fn = cg.module()->getFunction(m_callee);
    if (!callee_fn) {
        std::cerr << "Error: Unknown function referenced: " << m_callee << "\n";
        return nullptr;
    }

    if (callee_fn->arg_size() != m_args.size()) {
        std::cerr << "Error: Incorrect number of arguments passed to function\n";
        return nullptr;
    }

    std::vector<llvm::Value*> args_v;
    args_v.reserve(m_args.size());
    for (const auto& arg : m_args) {
        args_v.push_back(arg->codegen(cg));
        if (!args_v.back()) return nullptr;
    }

    return cg.builder().CreateCall(callee_fn, args_v, "calltmp");
}

// 5. Prototypes: Declare function signatures (all args and return types are double)
llvm::Function* Prototype::codegen(CodeGenerator& cg) {
    std::vector<llvm::Type*> doubles(m_args.size(), llvm::Type::getDoubleTy(cg.context()));
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(cg.context()), doubles, false);

    llvm::Function* f = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, m_name, cg.module());

    size_t idx = 0;
    for (auto& arg : f->args()) {
        arg.setName(m_args[idx++]);
    }

    return f;
}

// 6. Function Definitions: Attach basic blocks, bind parameters, generate body
llvm::Function* FuncNode::codegen(CodeGenerator& cg) {
    // Check if the function prototype was already declared (e.g. via 'extern')
    llvm::Function* function = cg.module()->getFunction(m_proto->name());

    if (!function) {
        function = m_proto->codegen(cg);
    }

    if (!function) return nullptr;

    if (!function->empty()) {
        std::cerr << "Error: Function " << m_proto->name() << " cannot be redefined.\n";
        return nullptr;
    }

    // Create entry basic block
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(cg.context(), "entry", function);
    cg.builder().SetInsertPoint(bb);

    // Bind argument names into the symbol table
    cg.named_values().clear();
    for (auto& arg : function->args()) {
        cg.named_values()[std::string(arg.getName())] = &arg;
    }

    // Codegen function body
    if (llvm::Value* ret_val = m_body->codegen(cg)) {
        cg.builder().CreateRet(ret_val);

        // Verify function integrity (catches malformed SSA, unlinked blocks, etc.)
        llvm::verifyFunction(*function);
        return function;
    }

    // On codegen failure, remove function from module to keep symbol table clean
    function->eraseFromParent();
    return nullptr;
}

} // namespace axiom