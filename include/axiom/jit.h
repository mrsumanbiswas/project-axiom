#pragma once
#include "axiom/support/error.h"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DataLayout.h>
#include <memory>
#include <string_view>

namespace axiom {

class AxiomJIT {
public:
    ~AxiomJIT() = default;

    // Factory method to initialize native target and create LLJIT instance
    static Expected<std::unique_ptr<AxiomJIT>> create();

    [[nodiscard]] const llvm::DataLayout& data_layout() const noexcept {
        return m_jit->getDataLayout();
    }

    // Add an LLVM module to the JIT pipeline
    Expected<void> add_module(std::unique_ptr<llvm::Module> module,
                              std::unique_ptr<llvm::LLVMContext> context);

    // Add a module with a specific ResourceTracker (allows deallocating __anon_expr)
    Expected<void> add_module(std::unique_ptr<llvm::Module> module,
                              std::unique_ptr<llvm::LLVMContext> context,
                              llvm::orc::ResourceTrackerSP rt);

    // Create a new ResourceTracker to track and deallocate temporary REPL expressions
    [[nodiscard]] llvm::orc::ResourceTrackerSP create_resource_tracker() {
        return m_jit->getMainJITDylib().createResourceTracker();
    }

    // Lookup a compiled function symbol by name
    Expected<void*> lookup(std::string_view name);

private:
    explicit AxiomJIT(std::unique_ptr<llvm::orc::LLJIT> jit);

    std::unique_ptr<llvm::orc::LLJIT> m_jit;
};

} // namespace axiom