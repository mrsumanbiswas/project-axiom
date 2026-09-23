#include "axiom/codegen.h"
#include "axiom/ast.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Instructions.h>
#include <iostream>
#include <vector>

namespace axiom {

CodeGenerator::CodeGenerator(const llvm::DataLayout& layout)
    : m_layout(layout) {
    reinitialize();
}

void CodeGenerator::reinitialize() {
    m_context = std::make_unique<llvm::LLVMContext>();
    m_builder = std::make_unique<llvm::IRBuilder<>>(*m_context);
    m_module = std::make_unique<llvm::Module>("AxiomModule", *m_context);
    m_module->setDataLayout(m_layout);
    m_named_values.clear();
}

std::unique_ptr<llvm::Module> CodeGenerator::take_module() {
    return std::move(m_module);
}

std::unique_ptr<llvm::LLVMContext> CodeGenerator::take_context() {
    return std::move(m_context);
}

void CodeGenerator::add_prototype(std::unique_ptr<Prototype> proto) {
    m_function_protos[proto->name()] = std::move(proto);
}

llvm::Function* CodeGenerator::get_function(std::string_view name) {
    if (auto* f = m_module->getFunction(llvm::StringRef(name.data(), name.size()))) {
        return f;
    }

    auto it = m_function_protos.find(std::string(name));
    if (it != m_function_protos.end()) {
        return it->second->codegen(*this);
    }

    return nullptr;
}

void CodeGenerator::dump() const {
    if (m_module) {
        m_module->print(llvm::errs(), nullptr);
    }
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
    llvm::Function* callee_fn = cg.get_function(m_callee);
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

// 5. If-Then-Else Expressions
llvm::Value* IfExpr::codegen(CodeGenerator& cg) {
    llvm::Value* cond_v = m_cond->codegen(cg);
    if (!cond_v) return nullptr;

    // Convert condition to boolean by comparing non-equal to 0.0
    cond_v = cg.builder().CreateFCmpONE(
        cond_v, llvm::ConstantFP::get(cg.context(), llvm::APFloat(0.0)), "ifcond");

    llvm::Function* parent_fn = cg.builder().GetInsertBlock()->getParent();

    // Create then, else, and continuation basic blocks
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(cg.context(), "then", parent_fn);
    llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(cg.context(), "else");
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(cg.context(), "ifcont");

    cg.builder().CreateCondBr(cond_v, then_bb, else_bb);

    // Emit 'then' block
    cg.builder().SetInsertPoint(then_bb);
    llvm::Value* then_v = m_then->codegen(cg);
    if (!then_v) return nullptr;

    cg.builder().CreateBr(merge_bb);
    then_bb = cg.builder().GetInsertBlock();

    // Emit 'else' block
    parent_fn->insert(parent_fn->end(), else_bb);
    cg.builder().SetInsertPoint(else_bb);
    llvm::Value* else_v = m_else->codegen(cg);
    if (!else_v) return nullptr;

    cg.builder().CreateBr(merge_bb);
    else_bb = cg.builder().GetInsertBlock();

    // Emit 'merge' continuation block with PHI node
    parent_fn->insert(parent_fn->end(), merge_bb);
    cg.builder().SetInsertPoint(merge_bb);

    llvm::PHINode* phi = cg.builder().CreatePHI(
        llvm::Type::getDoubleTy(cg.context()), 2, "iftmp");

    phi->addIncoming(then_v, then_bb);
    phi->addIncoming(else_v, else_bb);
    return phi;
}

// 6. Function Signatures
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

// 7. Function Definitions
llvm::Function* FuncNode::codegen(CodeGenerator& cg) {
    auto proto_name = m_proto->name();
    
    auto proto_copy = std::make_unique<Prototype>(proto_name, m_proto->args());
    cg.add_prototype(std::move(proto_copy));

    llvm::Function* function = cg.get_function(proto_name);
    if (!function) return nullptr;

    if (!function->empty()) {
        std::cerr << "Error: Function " << proto_name << " cannot be redefined.\n";
        return nullptr;
    }

    llvm::BasicBlock* bb = llvm::BasicBlock::Create(cg.context(), "entry", function);
    cg.builder().SetInsertPoint(bb);

    cg.named_values().clear();
    for (auto& arg : function->args()) {
        cg.named_values()[std::string(arg.getName())] = &arg;
    }

    if (llvm::Value* ret_val = m_body->codegen(cg)) {
        cg.builder().CreateRet(ret_val);
        llvm::verifyFunction(*function);
        return function;
    }

    function->eraseFromParent();
    return nullptr;
}

} // namespace axiom