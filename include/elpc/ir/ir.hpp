/*
   IR.HPP
   Lightweight intermediate representation with basic blocks,
   SSA-like values, and a control flow graph.

   Designed to be:
     - Analyzable (walk the instruction list, inspect uses)
     - Transformable (insert / replace instructions)
     - Printable (dot output for CFG visualization)
     - Lowerable (to C code or LLVM IR)

   This is the IR that a serious compiler built on ELPC would target,
   replacing the simpler text-based IRBuilder for production use.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace elpc {

// -----------------------------------------------------------------------
// IR types
// -----------------------------------------------------------------------

enum class IRType : uint8_t {
  VOID,
  INT1,    // bool
  INT8,
  INT16,
  INT32,
  INT64,
  FLOAT,
  DOUBLE,
  PTR,
};

inline std::string irTypeToString(IRType t) {
  switch (t) {
    case IRType::VOID:   return "void";
    case IRType::INT1:   return "i1";
    case IRType::INT8:   return "i8";
    case IRType::INT16:  return "i16";
    case IRType::INT32:  return "i32";
    case IRType::INT64:  return "i64";
    case IRType::FLOAT:  return "float";
    case IRType::DOUBLE: return "double";
    case IRType::PTR:    return "ptr";
  }
  return "?";
}

// -----------------------------------------------------------------------
// IR values — each instruction produces a value
// -----------------------------------------------------------------------

enum class IROp : uint8_t {
  // Terminators
  RET,       // ret VALUE
  BR,        // br LABEL
  BR_COND,   // br COND, TRUE_LABEL, FALSE_LABEL

  // Memory
  ALLOCA,    // alloca TYPE
  LOAD,      // load PTR
  STORE,     // store VALUE, PTR

  // Arithmetic
  ADD, SUB, MUL, SDIV, SMOD,
  FADD, FSUB, FMUL, FDIV,

  // Comparison
  EQ, NE, SLT, SLE, SGT, SGE,

  // Logical
  AND, OR, XOR, NOT,

  // Conversion
  SEXT,      // sign extend (i32 → i64)
  TRUNC,     // truncate (i64 → i32)
  FTOI,      // float to int
  ITOF,      // int to float
  BITCAST,   // reinterpret

  // Other
  CALL,
  PHI,
  GEP,       // get element pointer
  CONST_INT,
  CONST_FLOAT,
  CONST_BOOL,
};

// -----------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------

struct IRValue;
struct IRInstruction;
struct IRBasicBlock;
struct IRFunction;
struct IRModule;

// -----------------------------------------------------------------------
// IRValue — a typed value (produced by an instruction or constant)
// -----------------------------------------------------------------------

struct IRValue {
  IRType type;
  IRInstruction *def = nullptr;  // instruction that defines this, or nullptr
  std::string name;              // for printing

  IRValue(IRType t, std::string_view n = "")
    : type(t), name(n) {}
  virtual ~IRValue() = default;
};

struct IRConstInt : IRValue {
  int64_t value;
  IRConstInt(int64_t v, IRType t = IRType::INT64)
    : IRValue(t), value(v) {}
};

struct IRConstFloat : IRValue {
  double value;
  IRConstFloat(double v) : IRValue(IRType::DOUBLE), value(v) {}
};

struct IRConstBool : IRValue {
  bool value;
  IRConstBool(bool v) : IRValue(IRType::INT1), value(v) {}
};

// -----------------------------------------------------------------------
// IRInstruction — a single operation
// -----------------------------------------------------------------------

struct IRInstruction {
  IROp op;
  IRType type;
  std::vector<IRValue*> operands;
  std::vector<IRBasicBlock*> blockTargets;  // branch/succ targets
  IRBasicBlock *parent = nullptr;
  std::string comment;

  IRInstruction(IROp op, IRType t, std::vector<IRValue*> ops = {})
    : op(op), type(t), operands(std::move(ops)) {}
};

// -----------------------------------------------------------------------
// IRBasicBlock — a sequence of instructions with a terminator
// -----------------------------------------------------------------------

struct IRBasicBlock {
  std::string name;
  std::vector<std::unique_ptr<IRInstruction>> instructions;
  IRFunction *parent = nullptr;

  explicit IRBasicBlock(std::string_view n = "") : name(n) {}

  /// Add an instruction to the block.
  IRInstruction *addInst(IROp op, IRType t, std::vector<IRValue*> ops = {}) {
    auto inst = std::make_unique<IRInstruction>(op, t, std::move(ops));
    inst->parent = this;
    auto *ptr = inst.get();
    instructions.push_back(std::move(inst));
    return ptr;
  }

  /// Add a branch instruction with block targets.
  IRInstruction *addBranch(IROp op, std::vector<IRBasicBlock*> targets,
                            std::vector<IRValue*> vals = {}) {
    auto inst = std::make_unique<IRInstruction>(op, IRType::VOID, std::move(vals));
    inst->blockTargets = std::move(targets);
    inst->parent = this;
    auto *ptr = inst.get();
    instructions.push_back(std::move(inst));
    return ptr;
  }

  /// Get the terminator (last instruction), or nullptr.
  IRInstruction *getTerminator() const {
    if (instructions.empty()) return nullptr;
    auto &last = instructions.back();
    switch (last->op) {
      case IROp::RET:
      case IROp::BR:
      case IROp::BR_COND:
        return last.get();
      default:
        return nullptr;
    }
  }

  bool hasTerminator() const { return getTerminator() != nullptr; }

  /// Get successor blocks (from terminators).
  std::vector<IRBasicBlock*> getSuccessors() const {
    auto *term = getTerminator();
    if (!term) return {};
    return term->blockTargets;
  }
};

// -----------------------------------------------------------------------
// IRFunction — a named function with basic blocks
// -----------------------------------------------------------------------

struct IRFunction {
  std::string name;
  IRType returnType;
  std::vector<std::pair<std::string, IRType>> params;
  std::vector<std::unique_ptr<IRBasicBlock>> blocks;
  IRModule *parent = nullptr;

  IRFunction(std::string_view n, IRType ret)
    : name(n), returnType(ret) {}

  IRBasicBlock *addBlock(std::string_view name = "") {
    auto block = std::make_unique<IRBasicBlock>(name);
    block->parent = this;
    auto *ptr = block.get();
    blocks.push_back(std::move(block));
    return ptr;
  }
};

// -----------------------------------------------------------------------
// IRModule — owns all functions, constants, and types
// -----------------------------------------------------------------------

class IRModule {
public:
  std::string name;
  std::vector<std::unique_ptr<IRFunction>> functions;
  std::vector<std::unique_ptr<IRValue>> constants;
  size_t nextId = 0;

  explicit IRModule(std::string_view n = "module") : name(n) {}

  IRFunction *addFunction(std::string_view name, IRType returnType) {
    auto fn = std::make_unique<IRFunction>(name, returnType);
    fn->parent = this;
    auto *ptr = fn.get();
    functions.push_back(std::move(fn));
    return ptr;
  }

  IRValue *addConstInt(int64_t v, IRType t = IRType::INT64) {
    auto c = std::make_unique<IRConstInt>(v, t);
    auto *ptr = c.get();
    constants.push_back(std::move(c));
    return ptr;
  }

  IRValue *addConstFloat(double v) {
    auto c = std::make_unique<IRConstFloat>(v);
    auto *ptr = c.get();
    constants.push_back(std::move(c));
    return ptr;
  }

  IRValue *addConstBool(bool v) {
    auto c = std::make_unique<IRConstBool>(v);
    auto *ptr = c.get();
    constants.push_back(std::move(c));
    return ptr;
  }

  std::string freshName(std::string_view prefix = "%v") {
    return std::string(prefix) + std::to_string(nextId++);
  }

  // -----------------------------------------------------------------------
  // C code emission
  // -----------------------------------------------------------------------

  std::string emitC() const {
    std::string out;
    out += "// Generated by ELPC IR\n\n";
    out += "#include <stdint.h>\n\n";

    for (auto &fn : functions) {
      out += emitCFunction(*fn);
      out += "\n";
    }
    return out;
  }

  std::string emitCFunction(const IRFunction &fn) const {
    std::string out;

    // Function signature
    out += cTypeString(fn.returnType) + " " + fn.name + "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
      if (i > 0) out += ", ";
      out += cTypeString(fn.params[i].second) + " " + fn.params[i].first;
    }
    out += ") {\n";

    // Emit all blocks
    for (auto &block : fn.blocks) {
      out += "  " + block->name + ":\n";
      for (auto &inst : block->instructions) {
        out += "    " + emitCInst(*inst) + ";\n";
      }
    }

    out += "}\n";
    return out;
  }

  std::string emitCInst(const IRInstruction &inst) const {
    switch (inst.op) {
      case IROp::RET: {
        if (inst.operands.empty()) return "return";
        return "return " + valueName(*inst.operands[0]);
      }
      case IROp::CONST_INT: return std::to_string(static_cast<IRConstInt*>(inst.operands[0])->value);
      default: return "/* unhandled */";
    }
  }

  static std::string cTypeString(IRType t) {
    switch (t) {
      case IRType::VOID:   return "void";
      case IRType::INT1:   return "int8_t";
      case IRType::INT8:   return "int8_t";
      case IRType::INT16:  return "int16_t";
      case IRType::INT32:  return "int32_t";
      case IRType::INT64:  return "int64_t";
      case IRType::FLOAT:  return "float";
      case IRType::DOUBLE: return "double";
      case IRType::PTR:    return "void*";
    }
    return "void";
  }

  // -----------------------------------------------------------------------
  // IR text dump (for debugging)
  // -----------------------------------------------------------------------

  std::string dump() const {
    std::string out;
    out += "; ELPC IR module: " + name + "\n\n";
    for (auto &fn : functions) {
      out += dumpFunction(*fn);
    }
    return out;
  }

  std::string dumpFunction(const IRFunction &fn) const {
    std::string out;
    out += "define " + irTypeToString(fn.returnType) + " @" + fn.name + "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
      if (i > 0) out += ", ";
      out += irTypeToString(fn.params[i].second) + " %" + fn.params[i].first;
    }
    out += ") {\n";

    for (auto &block : fn.blocks) {
      out += "  " + block->name + ":\n";
      for (auto &inst : block->instructions) {
        out += "    " + dumpInst(*inst) + "\n";
      }
    }
    out += "}\n";
    return out;
  }

  std::string dumpInst(const IRInstruction &inst) const {
    std::string out;

    // Result value (if any)
    bool hasResult = false;
    switch (inst.op) {
      case IROp::RET:
      case IROp::BR:
      case IROp::BR_COND:
      case IROp::STORE:
        break; // no result
      default:
        hasResult = true;
        break;
    }

    if (hasResult) {
      out += "%r" + std::to_string(reinterpret_cast<uintptr_t>(&inst)) + " = ";
    }

    out += opName(inst.op);

    if (!inst.operands.empty()) {
      out += " ";
      for (size_t i = 0; i < inst.operands.size(); ++i) {
        if (i > 0) out += ", ";
        out += valueName(*inst.operands[i]);
      }
    }

    if (!inst.comment.empty()) {
      out += " ; " + inst.comment;
    }

    return out;
  }

  static std::string valueName(const IRValue &v) {
    if (auto *ci = dynamic_cast<const IRConstInt*>(&v)) {
      return std::to_string(ci->value);
    }
    if (auto *cf = dynamic_cast<const IRConstFloat*>(&v)) {
      return std::to_string(cf->value);
    }
    if (auto *cb = dynamic_cast<const IRConstBool*>(&v)) {
      return cb->value ? "true" : "false";
    }
    if (!v.name.empty()) return "%" + v.name;
    return "%v";
  }

  static std::string opName(IROp op) {
    switch (op) {
      case IROp::RET:      return "ret";
      case IROp::BR:       return "br";
      case IROp::BR_COND:  return "br_cond";
      case IROp::ALLOCA:   return "alloca";
      case IROp::LOAD:     return "load";
      case IROp::STORE:    return "store";
      case IROp::ADD:      return "add";
      case IROp::SUB:      return "sub";
      case IROp::MUL:      return "mul";
      case IROp::SDIV:     return "sdiv";
      case IROp::CALL:     return "call";
      case IROp::CONST_INT:  return "const_int";
      case IROp::CONST_FLOAT: return "const_float";
      case IROp::CONST_BOOL: return "const_bool";
      case IROp::EQ:  return "eq";
      case IROp::NE:  return "ne";
      case IROp::SLT: return "slt";
      case IROp::SLE: return "sle";
      case IROp::SGT: return "sgt";
      case IROp::SGE: return "sge";
      case IROp::AND: return "and";
      case IROp::OR:  return "or";
      case IROp::NOT: return "not";
      default: return "?";
    }
  }
};



// -----------------------------------------------------------------------
// IRFuncBuilder — convenience for building IR functions
// -----------------------------------------------------------------------

class IRFuncBuilder {
  IRModule *module = nullptr;
  IRFunction *currentFn = nullptr;
  IRBasicBlock *currentBlock = nullptr;

public:
  IRFuncBuilder() = default;

  void setModule(IRModule &m) { module = &m; }

  IRFunction *newFunction(std::string_view name, IRType returnType) {
    currentFn = module->addFunction(name, returnType);
    return currentFn;
  }

  IRBasicBlock *newBlock(std::string_view name = "") {
    if (!currentFn) return nullptr;
    if (name.empty()) name = module->freshName("bb");
    currentBlock = currentFn->addBlock(name);
    return currentBlock;
  }

  void setCurrentBlock(IRBasicBlock *block) { currentBlock = block; }
  IRBasicBlock *getCurrentBlock() const { return currentBlock; }

  IRInstruction *emit(IROp op, IRType t, std::vector<IRValue*> ops = {}) {
    if (!currentBlock) return nullptr;
    return currentBlock->addInst(op, t, std::move(ops));
  }

  IRValue *emitConstInt(int64_t v, IRType t = IRType::INT64) {
    return module->addConstInt(v, t);
  }

  void emitRet(IRValue *val = nullptr) {
    if (val) emit(IROp::RET, IRType::VOID, {val});
    else emit(IROp::RET, IRType::VOID);
  }

  void emitAdd(IRValue *lhs, IRValue *rhs) {
    emit(IROp::ADD, lhs->type, {lhs, rhs});
  }

  /// Emit alloca — allocate space on the stack.
  /// Returns the pointer value.
  IRInstruction *emitAlloca(IRType elemType) {
    return emit(IROp::ALLOCA, IRType::PTR);
  }

  /// Emit store — write a value to a pointer.
  void emitStore(IRValue *ptr, IRValue *val) {
    emit(IROp::STORE, IRType::VOID, {ptr, val});
  }

  /// Emit load — read a value from a pointer.
  IRInstruction *emitLoad(IRValue *ptr) {
    // The type is determined by the pointer's pointee
    return emit(IROp::LOAD, IRType::INT32, {ptr});
  }

  /// Emit a branch to a target block.
  void emitBr(IRBasicBlock *target) {
    if (currentBlock)
      currentBlock->addBranch(IROp::BR, {target});
  }

  /// Emit a conditional branch.
  void emitBrCond(IRValue *cond, IRBasicBlock *trueTarget,
                   IRBasicBlock *falseTarget) {
    if (currentBlock)
      currentBlock->addBranch(IROp::BR_COND, {trueTarget, falseTarget}, {cond});
  }
};

} // namespace elpc
