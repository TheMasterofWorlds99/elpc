/*
   LLVMBRIDGE.HPP
   This file contains the LLVM backend bridge for elpc. It subclasses
   IRBuilder<llvm::Value*> and provides a complete LLVM code generation
   interface, wrapping the most common LLVM operations in a clean API
   while still exposing the underlying LLVM objects for advanced use.

   The user owns and provides the LLVMContext and Module — the bridge
   never deletes anything it didn't create.

   Requires LLVM to be found via find_package(LLVM).
   Enable with: cmake -DELPC_ENABLE_LLVM=ON ..

   Example:
     llvm::LLVMContext ctx;
     auto mod = std::make_unique<llvm::Module>("mymod", ctx);

     elpc::DiagnosticEngine diag;
     elpc::LLVMBridge bridge(ctx, *mod, diag);

     auto *fn = bridge.beginFunction("main",
         llvm::Type::getInt32Ty(ctx), {});

     auto *val = bridge.emitInt(42);
     bridge.emitReturn(val);
     bridge.endFunction();

     bridge.dumpIR();
*/

#pragma once

#ifdef ELPC_ENABLE_LLVM

#include <elpc/ir/irBuilder.hpp>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <string>
#include <vector>

namespace elpc {

// Alias to avoid collision between elpc::IRBuilder and llvm::IRBuilder
template <typename T> using LLVMIRBuilder = llvm::IRBuilder<T>;

class LLVMBridge : public IRBuilder<llvm::Value *> {
  llvm::LLVMContext &ctx;
  llvm::Module &mod;
  LLVMIRBuilder<llvm::ConstantFolder> builder;
  llvm::Function *currentFn = nullptr;
  llvm::BasicBlock *currentBB = nullptr;

  // ----------------------------------------------------------------
  // Target initialization — called once on first bridge construction
  // ----------------------------------------------------------------

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
      : IRBuilder<llvm::Value *>(diag), ctx(ctx), mod(mod), builder(ctx) {
    initializeTargets();
  }

  // Non-copyable
  LLVMBridge(const LLVMBridge &) = delete;
  LLVMBridge &operator=(const LLVMBridge &) = delete;

  // ----------------------------------------------------------------
  // LLVM object access — for advanced users who need direct access
  // ----------------------------------------------------------------

  llvm::LLVMContext &getContext() { return ctx; }
  llvm::Module &getModule() { return mod; }
  LLVMIRBuilder<llvm::ConstantFolder> &getBuilder() { return builder; }
  llvm::Function *getCurrentFunction() { return currentFn; }

  // ----------------------------------------------------------------
  // Function management
  // ----------------------------------------------------------------

  llvm::Function *beginFunction(
      const std::string &name, llvm::Type *returnType,
      std::vector<llvm::Type *> argTypes,
      llvm::Function::LinkageTypes linkage = llvm::Function::ExternalLinkage) {
    auto *fnType = llvm::FunctionType::get(returnType, argTypes, false);
    currentFn = llvm::Function::Create(fnType, linkage, name, mod);
    currentBB = llvm::BasicBlock::Create(ctx, "entry", currentFn);
    builder.SetInsertPoint(currentBB);
    pushScope();
    return currentFn;
  }

  void endFunction() {
    if (!currentFn) {
      error("[elpc] LLVMBridge: endFunction called with no active function");
      return;
    }

    // Verify the function — catches malformed IR early
    std::string err;
    llvm::raw_string_ostream errStream(err);
    if (llvm::verifyFunction(*currentFn, &errStream)) {
      error("[elpc] LLVM function verification failed: " + errStream.str());
    }

    popScope();
    currentFn = nullptr;
    currentBB = nullptr;
  }

  // ----------------------------------------------------------------
  // Literal emitters
  // ----------------------------------------------------------------

  llvm::Value *emitInt(int value, unsigned bits = 32) {
    return llvm::ConstantInt::get(llvm::IntegerType::get(ctx, bits), value,
                                  true /* signed */);
  }

  llvm::Value *emitInt64(int64_t value) { return emitInt(value, 64); }

  llvm::Value *emitFloat(double value) {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), value);
  }

  llvm::Value *emitFloat32(float value) {
    return llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx), value);
  }

  llvm::Value *emitBool(bool value) {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), value ? 1 : 0);
  }

  // ----------------------------------------------------------------
  // Integer arithmetic
  // ----------------------------------------------------------------

  llvm::Value *emitAdd(llvm::Value *lhs, llvm::Value *rhs,
                       const std::string &name = "") {
    return builder.CreateAdd(lhs, rhs, name);
  }

  llvm::Value *emitSub(llvm::Value *lhs, llvm::Value *rhs,
                       const std::string &name = "") {
    return builder.CreateSub(lhs, rhs, name);
  }

  llvm::Value *emitMul(llvm::Value *lhs, llvm::Value *rhs,
                       const std::string &name = "") {
    return builder.CreateMul(lhs, rhs, name);
  }

  llvm::Value *emitDiv(llvm::Value *lhs, llvm::Value *rhs,
                       const std::string &name = "", SourceLocation loc = {}) {
    // Check for constant zero divisor at compile time
    if (auto *constRhs = llvm::dyn_cast<llvm::ConstantInt>(rhs)) {
      if (constRhs->isZero()) {
        error("Division by zero", loc);
        return emitInt(0);
      }
    }
    return builder.CreateSDiv(lhs, rhs, name);
  }

  // ----------------------------------------------------------------
  // Float arithmetic
  // ----------------------------------------------------------------

  llvm::Value *emitFAdd(llvm::Value *lhs, llvm::Value *rhs,
                        const std::string &name = "") {
    return builder.CreateFAdd(lhs, rhs, name);
  }

  llvm::Value *emitFSub(llvm::Value *lhs, llvm::Value *rhs,
                        const std::string &name = "") {
    return builder.CreateFSub(lhs, rhs, name);
  }

  llvm::Value *emitFMul(llvm::Value *lhs, llvm::Value *rhs,
                        const std::string &name = "") {
    return builder.CreateFMul(lhs, rhs, name);
  }

  llvm::Value *emitFDiv(llvm::Value *lhs, llvm::Value *rhs,
                        const std::string &name = "", SourceLocation loc = {}) {
    if (auto *constRhs = llvm::dyn_cast<llvm::ConstantFP>(rhs)) {
      if (constRhs->isZero()) {
        error("Float division by zero", loc);
        return emitFloat(0.0);
      }
    }
    return builder.CreateFDiv(lhs, rhs, name);
  }

  // ----------------------------------------------------------------
  // Comparison
  // ----------------------------------------------------------------

  llvm::Value *emitICmpEQ(llvm::Value *lhs, llvm::Value *rhs,
                          const std::string &name = "") {
    return builder.CreateICmpEQ(lhs, rhs, name);
  }

  llvm::Value *emitICmpNE(llvm::Value *lhs, llvm::Value *rhs,
                          const std::string &name = "") {
    return builder.CreateICmpNE(lhs, rhs, name);
  }

  llvm::Value *emitICmpLT(llvm::Value *lhs, llvm::Value *rhs,
                          const std::string &name = "") {
    return builder.CreateICmpSLT(lhs, rhs, name);
  }

  llvm::Value *emitICmpGT(llvm::Value *lhs, llvm::Value *rhs,
                          const std::string &name = "") {
    return builder.CreateICmpSGT(lhs, rhs, name);
  }

  // ----------------------------------------------------------------
  // Memory
  // ----------------------------------------------------------------

  llvm::Value *emitAlloca(llvm::Type *type, const std::string &name = "") {
    // Always insert allocas at the top of the entry block
    auto &entryBlock = currentFn->getEntryBlock();
    llvm::IRBuilder<> allocaBuilder(&entryBlock,
                                    entryBlock.getFirstInsertionPt());
    return allocaBuilder.CreateAlloca(type, nullptr, name);
  }

  void emitStore(llvm::Value *val, llvm::Value *ptr) {
    builder.CreateStore(val, ptr);
  }

  llvm::Value *emitLoad(llvm::Type *type, llvm::Value *ptr,
                        const std::string &name = "") {
    return builder.CreateLoad(type, ptr, name);
  }

  // ----------------------------------------------------------------
  // Control flow
  // ----------------------------------------------------------------

  llvm::Value *emitCall(llvm::Function *fn, std::vector<llvm::Value *> args,
                        const std::string &name = "") {
    return builder.CreateCall(fn, args, name);
  }

  void emitReturn(llvm::Value *value) { builder.CreateRet(value); }

  void emitReturnVoid() { builder.CreateRetVoid(); }

  llvm::BasicBlock *emitBlock(const std::string &name = "block") {
    auto *bb = llvm::BasicBlock::Create(ctx, name, currentFn);
    return bb;
  }

  void setInsertBlock(llvm::BasicBlock *bb) {
    currentBB = bb;
    builder.SetInsertPoint(bb);
  }

  void emitBranch(llvm::BasicBlock *dest) { builder.CreateBr(dest); }

  void emitCondBranch(llvm::Value *cond, llvm::BasicBlock *trueBB,
                      llvm::BasicBlock *falseBB) {
    builder.CreateCondBr(cond, trueBB, falseBB);
  }

  // ----------------------------------------------------------------
  // Output
  // ----------------------------------------------------------------

  // Print LLVM IR to stdout
  void dumpIR() const { mod.print(llvm::outs(), nullptr); }

  // Write LLVM IR as a .ll text file
  void writeIR(const std::string &path) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
    if (ec) {
      llvm::errs() << "[elpc] Failed to open file: " << ec.message() << "\n";
      return;
    }
    mod.print(out, nullptr);
  }

  // Write native object file (.o)
  void writeObject(const std::string &path) {
    llvm::Triple targetTriple(llvm::sys::getDefaultTargetTriple());
    mod.setTargetTriple(targetTriple);

    std::string lookupError;
    auto *target =
        llvm::TargetRegistry::lookupTarget(targetTriple, lookupError);
    if (!target) {
      error("[elpc] LLVM target lookup failed: " + lookupError);
      return;
    }

    auto cpu = llvm::sys::getHostCPUName();
    auto features = "";
    llvm::TargetOptions opt;
    auto *targetMachine = target->createTargetMachine(
        targetTriple, cpu, features, opt, llvm::Reloc::PIC_);

    mod.setDataLayout(targetMachine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
      error("[elpc] Failed to open output file: " + ec.message());
      return;
    }

    llvm::legacy::PassManager pm;
    if (targetMachine->addPassesToEmitFile(pm, out, nullptr,
                                           llvm::CodeGenFileType::ObjectFile)) {
      error("[elpc] LLVM cannot emit object file for this target");
      return;
    }

    pm.run(mod);
  }
};

} // namespace elpc

#endif // ELPC_ENABLE_LLVM
