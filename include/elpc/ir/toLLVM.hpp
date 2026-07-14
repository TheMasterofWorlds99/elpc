/*
   TOLLVM.HPP
   Translates an IRModule into LLVM IR using the LLVMBridge.
   Requires ELPC_ENABLE_LLVM=ON and LLVM 14+.

   Usage:
     #ifdef ELPC_ENABLE_LLVM
     elpc::LLVMIRTranslator translator(ctx, mod);
     translator.translate(irModule);
     translator.module().print(llvm::outs(), nullptr);
     #endif
*/

#pragma once

#ifdef ELPC_ENABLE_LLVM

#include <elpc/ir/ir.hpp>
#include <elpc/ir/llvmBridge.hpp>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <unordered_map>

namespace elpc {

/// Translates an ELPC IRModule into LLVM IR.
class LLVMIRTranslator {
  llvm::LLVMContext &ctx;
  llvm::Module &llvmMod;
  elpc::DiagnosticEngine diag;
  elpc::LLVMBridge bridge;
  std::unordered_map<const IRFunction*, llvm::Function*> fnMap_;
  std::unordered_map<const IRBasicBlock*, llvm::BasicBlock*> blockMap_;

  llvm::Type *toLLVMType(IRType t) {
    switch (t) {
      case IRType::VOID:   return llvm::Type::getVoidTy(ctx);
      case IRType::INT1:   return llvm::Type::getInt1Ty(ctx);
      case IRType::INT8:   return llvm::Type::getInt8Ty(ctx);
      case IRType::INT32:  return llvm::Type::getInt32Ty(ctx);
      case IRType::INT64:  return llvm::Type::getInt64Ty(ctx);
      case IRType::FLOAT:  return llvm::Type::getFloatTy(ctx);
      case IRType::DOUBLE: return llvm::Type::getDoubleTy(ctx);
      case IRType::PTR:    return llvm::PointerType::get(ctx, 0);
    }
    return llvm::Type::getVoidTy(ctx);
  }

public:
  LLVMIRTranslator(llvm::LLVMContext &ctx, llvm::Module &mod)
    : ctx(ctx), llvmMod(mod), bridge(ctx, mod, diag) {}

  llvm::Module &module() { return llvmMod; }

  void translate(elpc::IRModule &irMod) {
    // First pass: create all LLVM functions
    for (auto &fn : irMod.functions) {
      std::vector<llvm::Type*> paramTypes;
      for (auto &p : fn->params)
        paramTypes.push_back(toLLVMType(p.second));

      auto *llvmFn = bridge.beginFunction(
        fn->name, toLLVMType(fn->returnType), paramTypes);
      fnMap_[fn.get()] = llvmFn;
    }

    // Second pass: create basic blocks and emit instructions
    for (auto &fn : irMod.functions) {
      auto *llvmFn = fnMap_[fn.get()];
      blockMap_.clear();

      // Create all blocks first
      for (auto &block : fn->blocks) {
        auto *llvmBB = llvm::BasicBlock::Create(ctx, block->name, llvmFn);
        blockMap_[block.get()] = llvmBB;
      }

      // Emit instructions into each block
      for (auto &block : fn->blocks) {
        auto *llvmBB = blockMap_[block.get()];
        bridge.getBuilder().SetInsertPoint(llvmBB);

        for (auto &inst : block->instructions) {
          translateInst(*inst);
        }
      }
    }
  }

  void translateInst(IRInstruction &inst) {
    switch (inst.op) {
      case IROp::RET: {
        if (inst.operands.empty()) {
          bridge.getBuilder().CreateRetVoid();
        } else {
          // Would need value tracking — for now, placeholder
          bridge.getBuilder().CreateRet(llvm::ConstantInt::get(ctx,
            llvm::APInt(32, 0)));
        }
        break;
      }
      case IROp::CONST_INT:
        // Constants are handled inline
        break;
      default:
        break;
    }
  }
};

} // namespace elpc

#endif // ELPC_ENABLE_LLVM
