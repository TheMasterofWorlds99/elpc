#pragma once

#ifdef ELPC_ENABLE_LLVM

#include <elpc/ir/irBuilder.hpp>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

namespace elpc {

struct LLVMVar {
  llvm::Value *ptr;
  llvm::Type  *type;
};

template <typename T> using LLVMIRBuilder = llvm::IRBuilder<T>;

class LLVMBridge : public IRBuilder<LLVMVar> {
  llvm::LLVMContext &ctx;
  llvm::Module      &mod;
  LLVMIRBuilder<llvm::ConstantFolder> builder;
  llvm::Function   *currentFn = nullptr;

  static void initializeTargets() {
    static bool initialized = false;
    if (!initialized) {
      llvm::InitializeNativeTarget();
      llvm::InitializeNativeTargetAsmPrinter();
      llvm::InitializeNativeTargetAsmParser();
      initialized = true;
    }
  }

public:
  LLVMBridge(llvm::LLVMContext &ctx, llvm::Module &mod, DiagnosticEngine &diag)
      : IRBuilder<LLVMVar>(diag), ctx(ctx), mod(mod), builder(ctx) {
    initializeTargets();
  }

  LLVMBridge(const LLVMBridge &) = delete;
  LLVMBridge &operator=(const LLVMBridge &) = delete;

  // --- Raw access — use these to call LLVM directly ---
  llvm::LLVMContext &getContext() { return ctx; }
  llvm::Module      &getModule()  { return mod; }
  LLVMIRBuilder<llvm::ConstantFolder> &getBuilder() { return builder; }
  llvm::Function    *getCurrentFunction() { return currentFn; }

  // --- Function lifecycle ---
  llvm::Function *beginFunction(
      const std::string &name,
      llvm::Type *returnType,
      std::vector<llvm::Type *> argTypes,
      llvm::Function::LinkageTypes linkage = llvm::Function::ExternalLinkage) {

    auto *fnType = llvm::FunctionType::get(returnType, argTypes, false);
    currentFn = llvm::Function::Create(fnType, linkage, name, mod);
    auto *entry = llvm::BasicBlock::Create(ctx, "entry", currentFn);
    builder.SetInsertPoint(entry);
    pushScope();
    return currentFn;
  }

  void endFunction() {
    if (!currentFn) {
      error("[elpc] endFunction called with no active function");
      return;
    }
    std::string err;
    llvm::raw_string_ostream errStream(err);
    if (llvm::verifyFunction(*currentFn, &errStream))
      error("[elpc] LLVM function verification failed: " + errStream.str());

    popScope();
    currentFn = nullptr;
  }

  // --- Alloca (special: always inserted at entry block top) ---
  llvm::Value *emitAlloca(llvm::Type *type, const std::string &name = "") {
    auto &entry = currentFn->getEntryBlock();
    llvm::IRBuilder<> allocaBuilder(&entry, entry.getFirstInsertionPt());
    return allocaBuilder.CreateAlloca(type, nullptr, name);
  }

  // --- Output ---
  void dumpIR() const { mod.print(llvm::outs(), nullptr); }

  void writeIR(const std::string &path) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
    if (ec) { llvm::errs() << "[elpc] " << ec.message() << "\n"; return; }
    mod.print(out, nullptr);
  }

  void writeObject(const std::string &path) {
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    mod.setTargetTriple(triple);

    std::string err;
    auto *target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) { error("[elpc] Target lookup failed: " + err); return; }

    auto *tm = target->createTargetMachine(
        triple, llvm::sys::getHostCPUName(), "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_);

    mod.setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
    if (ec) { error("[elpc] Failed to open output: " + ec.message()); return; }

    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, llvm::CodeGenFileType::ObjectFile))
      error("[elpc] Cannot emit object file for this target");

    pm.run(mod);
  }
};

} // namespace elpc

#endif
