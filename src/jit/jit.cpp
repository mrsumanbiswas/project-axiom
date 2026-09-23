#include "axiom/jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>
#include <llvm/ExecutionEngine/Orc/EPCDynamicLibrarySearchGenerator.h>

namespace axiom {

AxiomJIT::AxiomJIT(std::unique_ptr<llvm::orc::LLJIT> jit)
    : m_jit(std::move(jit)) {}

Expected<std::unique_ptr<AxiomJIT>> AxiomJIT::create() {
    // 1. Initialize native target machines and printers
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // 2. Instantiate LLJIT
    auto jit_expected = llvm::orc::LLJITBuilder().create();
    if (!jit_expected) {
        std::string err_str;
        llvm::raw_string_ostream os(err_str);
        os << jit_expected.takeError();
        return std::unexpected(CompilerError{
            .message = "Failed to initialize LLJIT: " + err_str,
            .location = {}
        });
    }

    auto jit = std::move(*jit_expected);

    // 3. Resolve host dynamic symbols (e.g., libc/libm) using GetForTargetProcess
    auto gen_expected = llvm::orc::EPCDynamicLibrarySearchGenerator::GetForTargetProcess(
        jit->getExecutionSession());

    if (!gen_expected) {
        std::string err_str;
        llvm::raw_string_ostream os(err_str);
        os << gen_expected.takeError();
        return std::unexpected(CompilerError{
            .message = "Failed to load target process dynamic symbols: " + err_str,
            .location = {}
        });
    }

    jit->getMainJITDylib().addGenerator(std::move(*gen_expected));

    return std::unique_ptr<AxiomJIT>(new AxiomJIT(std::move(jit)));
}

Expected<void> AxiomJIT::add_module(std::unique_ptr<llvm::Module> module,
                                    std::unique_ptr<llvm::LLVMContext> context) {
    llvm::orc::ThreadSafeModule tsm(std::move(module), std::move(context));
    if (llvm::Error err = m_jit->addIRModule(std::move(tsm))) {
        std::string err_str;
        llvm::raw_string_ostream os(err_str);
        os << err;
        return std::unexpected(CompilerError{
            .message = "Failed to add IR module to JIT: " + err_str,
            .location = {}
        });
    }
    return {};
}

Expected<void> AxiomJIT::add_module(std::unique_ptr<llvm::Module> module,
                                    std::unique_ptr<llvm::LLVMContext> context,
                                    llvm::orc::ResourceTrackerSP rt) {
    llvm::orc::ThreadSafeModule tsm(std::move(module), std::move(context));
    if (llvm::Error err = m_jit->addIRModule(std::move(rt), std::move(tsm))) {
        std::string err_str;
        llvm::raw_string_ostream os(err_str);
        os << err;
        return std::unexpected(CompilerError{
            .message = "Failed to add tracked IR module to JIT: " + err_str,
            .location = {}
        });
    }
    return {};
}

Expected<void*> AxiomJIT::lookup(std::string_view name) {
    auto sym_expected = m_jit->lookup(name);
    if (!sym_expected) {
        std::string err_str;
        llvm::raw_string_ostream os(err_str);
        os << sym_expected.takeError();
        return std::unexpected(CompilerError{
            .message = "Symbol lookup failed for '" + std::string(name) + "': " + err_str,
            .location = {}
        });
    }

    return sym_expected->toPtr<void*>();
}

} // namespace axiom