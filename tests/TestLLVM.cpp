// tests/TestLLVM.cpp
#ifdef ELPC_ENABLE_LLVM

#include "TestHelpers.hpp"
#include <elpc/ir/llvmBridge.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

// ----------------------------------------------------------------
// Test runner
// ----------------------------------------------------------------

void runLLVMTest(
    const std::string &label,
    std::function<void(elpc::LLVMBridge &, llvm::LLVMContext &)> fn) {
  std::cout << "\n=== " << label << " ===\n";

  llvm::LLVMContext ctx;
  auto mod = std::make_unique<llvm::Module>(label, ctx);

  elpc::DiagnosticEngine diag;
  elpc::LLVMBridge bridge(ctx, *mod, diag);

  try {
    fn(bridge, ctx);
  } catch (const std::exception &e) {
    std::cerr << "[exception] " << e.what() << "\n";
    return;
  }

  if (!diag.empty()) {
    std::cout << "-- Diagnostics --\n";
    diag.reportDiagnostics(std::cout);
  }

  if (!diag.hasErrors()) {
    std::cout << "-- LLVM IR --\n";
    bridge.dumpIR();
  }
}

int main() {
  // Test 1: simple function returning a constant
  runLLVMTest("CONSTANT RETURN", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("main", llvm::Type::getInt32Ty(ctx), {});

    auto *val = bridge.emitInt(42);
    bridge.emitReturn(val);
    bridge.endFunction();
  });

  // Test 2: integer arithmetic
  runLLVMTest("INTEGER ARITHMETIC", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("compute", llvm::Type::getInt32Ty(ctx), {});

    auto *a = bridge.emitInt(10);
    auto *b = bridge.emitInt(3);
    auto *sum = bridge.emitAdd(a, b, "sum");     // 13
    auto *prod = bridge.emitMul(sum, b, "prod"); // 39
    bridge.emitReturn(prod);
    bridge.endFunction();
  });

  // Test 3: float arithmetic
  runLLVMTest("FLOAT ARITHMETIC", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("floatCompute", llvm::Type::getDoubleTy(ctx), {});

    auto *a = bridge.emitFloat(1.5);
    auto *b = bridge.emitFloat(2.5);
    auto *sum = bridge.emitFAdd(a, b, "fsum"); // 4.0
    bridge.emitReturn(sum);
    bridge.endFunction();
  });

  // Test 4: alloca, store, load — local variable
  runLLVMTest("LOCAL VARIABLE", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("withLocal", llvm::Type::getInt32Ty(ctx), {});

    // int x = 7;
    auto *xPtr = bridge.emitAlloca(llvm::Type::getInt32Ty(ctx), "x");
    bridge.emitStore(bridge.emitInt(7), xPtr);

    // return x + 1;
    auto *xVal = bridge.emitLoad(llvm::Type::getInt32Ty(ctx), xPtr, "xval");
    auto *result = bridge.emitAdd(xVal, bridge.emitInt(1), "result");
    bridge.emitReturn(result);
    bridge.endFunction();
  });

  // Test 5: comparison and conditional branch
  runLLVMTest("CONDITIONAL BRANCH", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("conditional", llvm::Type::getInt32Ty(ctx), {});

    auto *val = bridge.emitInt(10);
    auto *thresh = bridge.emitInt(5);
    auto *cond = bridge.emitICmpGT(val, thresh, "cmp"); // 10 > 5

    auto *trueBB = bridge.emitBlock("if_true");
    auto *falseBB = bridge.emitBlock("if_false");
    auto *mergeBB = bridge.emitBlock("merge");

    bridge.emitCondBranch(cond, trueBB, falseBB);

    // if_true: return 1
    bridge.setInsertBlock(trueBB);
    bridge.emitBranch(mergeBB);

    // if_false: return 0
    bridge.setInsertBlock(falseBB);
    bridge.emitBranch(mergeBB);

    // merge: phi and return
    bridge.setInsertBlock(mergeBB);
    auto *phi = llvm::PHINode::Create(llvm::Type::getInt32Ty(ctx), 2, "result",
                                      mergeBB);
    phi->addIncoming(bridge.emitInt(1), trueBB);
    phi->addIncoming(bridge.emitInt(0), falseBB);
    bridge.emitReturn(phi);
    bridge.endFunction();
  });

  // Test 6: division by zero — caught at compile time
  runLLVMTest("DIVISION BY ZERO", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("badDiv", llvm::Type::getInt32Ty(ctx), {});

    auto *a = bridge.emitInt(10);
    auto *zero = bridge.emitInt(0);
    auto *bad = bridge.emitDiv(a, zero, "bad", {1, 1, "test.elpc"});
    bridge.emitReturn(bad);
    bridge.endFunction();
  });

  // Test 7: write IR to file
  runLLVMTest("WRITE IR FILE", [](auto &bridge, auto &ctx) {
    bridge.beginFunction("main", llvm::Type::getInt32Ty(ctx), {});
    bridge.emitReturn(bridge.emitInt(0));
    bridge.endFunction();
    bridge.writeIR("/tmp/elpc_test_out.ll");
    std::cout << "IR written to /tmp/elpc_test_out.ll\n";
  });
}

#endif // ELPC_ENABLE_LLVM
