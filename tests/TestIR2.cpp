#include <elpc/ir/ir.hpp>
#include <iostream>
#include <string>

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testBasicModule() {
  elpc::IRModule mod("test");
  CHECK(mod.name == "test", "module name");

  auto *fn = mod.addFunction("main", elpc::IRType::INT32);
  CHECK(fn->name == "main", "function name");
  CHECK(fn->returnType == elpc::IRType::INT32, "return type");

  auto *block = fn->addBlock("entry");
  CHECK(block->name == "entry", "block name");
  CHECK(block->parent == fn, "block parent");

  std::cout << "  PASS basicModule\n";
  return 0;
}

static int testConstants() {
  elpc::IRModule mod("test");

  auto *ci = mod.addConstInt(42);
  CHECK(ci->type == elpc::IRType::INT64, "const int type");

  auto *cf = mod.addConstFloat(3.14);
  CHECK(cf->type == elpc::IRType::DOUBLE, "const float type");

  auto *cb = mod.addConstBool(true);
  CHECK(cb->type == elpc::IRType::INT1, "const bool type");

  std::cout << "  PASS constants\n";
  return 0;
}

static int testInstructions() {
  elpc::IRModule mod("test");
  auto *fn = mod.addFunction("test", elpc::IRType::VOID);
  auto *block = fn->addBlock("entry");

  auto *ci = mod.addConstInt(42, elpc::IRType::INT32);
  auto *ret = block->addInst(elpc::IROp::RET, elpc::IRType::VOID, {ci});
  CHECK(ret->op == elpc::IROp::RET, "ret op");
  CHECK(ret->operands.size() == 1, "one operand");
  CHECK(ret->parent == block, "instruction parent");
  CHECK(block->hasTerminator(), "block has terminator");

  std::cout << "  PASS instructions\n";
  return 0;
}

static int testDumpOutput() {
  elpc::IRModule mod("test_mod");

  auto *fn = mod.addFunction("add", elpc::IRType::INT32);
  fn->params.push_back({"a", elpc::IRType::INT32});
  fn->params.push_back({"b", elpc::IRType::INT32});

  auto *block = fn->addBlock("entry");
  // Add a return instruction with a constant
  auto *ci = mod.addConstInt(42, elpc::IRType::INT32);
  block->addInst(elpc::IROp::RET, elpc::IRType::VOID, {ci});

  std::string dumped = mod.dump();
  CHECK(dumped.find("test_mod") != std::string::npos, "dump contains module name");
  CHECK(dumped.find("@add") != std::string::npos, "dump contains function name");
  CHECK(dumped.find("i32") != std::string::npos, "dump contains i32");
  CHECK(dumped.find("entry") != std::string::npos, "dump contains block name");

  std::cout << "  PASS dumpOutput\n";
  return 0;
}

static int testCExport() {
  elpc::IRModule mod("test");

  auto *fn = mod.addFunction("add", elpc::IRType::INT32);
  fn->params.push_back({"a", elpc::IRType::INT32});
  fn->params.push_back({"b", elpc::IRType::INT32});

  auto *block = fn->addBlock("entry");
  auto *ci = mod.addConstInt(42, elpc::IRType::INT32);
  block->addInst(elpc::IROp::RET, elpc::IRType::VOID, {ci});

  std::string cCode = mod.emitC();
  CHECK(cCode.find("int32_t add") != std::string::npos, "C code has function sig");
  CHECK(cCode.find("int32_t a") != std::string::npos, "C code has param a");

  std::cout << "  PASS cExport\n";
  return 0;
}

static int testIRBuilder() {
  elpc::IRModule mod("test");
  elpc::IRFuncBuilder builder;
  builder.setModule(mod);

  auto *fn = builder.newFunction("test_fn", elpc::IRType::INT32);
  CHECK(fn != nullptr, "builder creates function");

  auto *block = builder.newBlock("start");
  CHECK(block != nullptr, "builder creates block");

  auto *ci = mod.addConstInt(99, elpc::IRType::INT32);
  builder.emitRet(ci);

  CHECK(block->hasTerminator(), "builder creates terminator");
  CHECK(block->instructions.size() == 1, "one instruction in block");

  std::cout << "  PASS irBuilder\n";
  return 0;
}

static int testControlFlow() {
  elpc::IRModule mod("test_cf");
  elpc::IRFuncBuilder builder;
  builder.setModule(mod);

  auto *fn = builder.newFunction("test_fn", elpc::IRType::INT32);
  auto *entry = builder.newBlock("entry");
  auto *thenBlock = fn->addBlock("then");
  auto *elseBlock = fn->addBlock("else");
  auto *merge = fn->addBlock("merge");

  auto *cond = mod.addConstBool(true);
  builder.emitBrCond(cond, thenBlock, elseBlock);

  // then block
  builder.setCurrentBlock(thenBlock);
  auto *ci = mod.addConstInt(42, elpc::IRType::INT32);
  builder.emitRet(ci);

  // else block
  builder.setCurrentBlock(elseBlock);
  auto *ci2 = mod.addConstInt(0, elpc::IRType::INT32);
  builder.emitRet(ci2);

  CHECK(entry->getSuccessors().size() == 2, "conditional branch has 2 successors");
  CHECK(thenBlock->getSuccessors().empty(), "ret block has no successors");

  // Dump should show blocks and branches
  std::string dumped = mod.dump();
  CHECK(dumped.find("entry") != std::string::npos, "dump has entry");
  CHECK(dumped.find("then") != std::string::npos, "dump has then");
  CHECK(dumped.find("else") != std::string::npos, "dump has else");

  std::cout << "  PASS controlFlow\n";
  return 0;
}

static int testBuilderEmit() {
  elpc::IRModule mod("test_mem");
  elpc::IRFuncBuilder builder;
  builder.setModule(mod);
  builder.newFunction("test_mem", elpc::IRType::INT32);
  builder.newBlock("entry");

  auto *alloca = builder.emitAlloca(elpc::IRType::INT32);
  CHECK(alloca->op == elpc::IROp::ALLOCA, "alloca instruction");

  auto *val = mod.addConstInt(42, elpc::IRType::INT32);
  builder.emitRet(val);
  CHECK(builder.getCurrentBlock()->hasTerminator(), "has terminator");

  std::cout << "  PASS builderEmit\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testBasicModule();
  failures += testConstants();
  failures += testInstructions();
  failures += testDumpOutput();
  failures += testCExport();
  failures += testIRBuilder();
  failures += testControlFlow();
  failures += testBuilderEmit();

  if (failures == 0)
    std::cout << "All TestIR2 tests passed!\n";
  else
    std::cerr << failures << " TestIR2 test(s) failed!\n";

  return failures;
}
