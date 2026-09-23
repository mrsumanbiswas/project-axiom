#pragma once
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <memory>
#include <string>
#include <string_view>
#include <map>

namespace axiom {

class Prototype;

class CodeGenerator {
public:
    explicit CodeGenerator(const llvm::DataLayout& layout);
    ~CodeGenerator() = default;

    [[nodiscard]] llvm::LLVMContext& context() noexcept { return *m_context; }
    [[nodiscard]] llvm::IRBuilder<>& builder() noexcept { return *m_builder; }
    [[nodiscard]] llvm::Module* module() noexcept { return m_module.get(); }
    
    [[nodiscard]] std::map<std::string, llvm::Value*>& named_values() noexcept { return m_named_values; }

    // Prototype Registry (retains function signatures across module boundaries)
    void add_prototype(std::unique_ptr<Prototype> proto);
    llvm::Function* get_function(std::string_view name);

    // Module ownership handoff and refresh
    std::unique_ptr<llvm::Module> take_module();
    std::unique_ptr<llvm::LLVMContext> take_context();
    void reinitialize();

    void dump() const;

private:
    llvm::DataLayout m_layout;
    std::unique_ptr<llvm::LLVMContext> m_context;
    std::unique_ptr<llvm::IRBuilder<>> m_builder;
    std::unique_ptr<llvm::Module> m_module;
    std::map<std::string, llvm::Value*> m_named_values;
    std::map<std::string, std::unique_ptr<Prototype>> m_function_protos;
};

} // namespace axiom