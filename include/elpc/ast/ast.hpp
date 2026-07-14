/*
   AST.HPP
   AST building blocks — reusable node types, visitor pattern, and
   a built-in debug printer.

   Design:
     - Every node carries a SourceLocation.
     - Nodes own their children via std::unique_ptr.
     - The visitor dispatches via a NodeKind enum + static_cast,
       avoiding virtual dispatch overhead on every node.
     - Override visit(const FooNode&) for types you care about,
       or visitDefault() as a catch-all.

   Usage:
     auto expr = elpc::makeNode<elpc::BinaryExpr>(
         elpc::BinaryOp::ADD,
         elpc::makeNode<elpc::LiteralExpr>(42),
         elpc::makeNode<elpc::LiteralExpr>(7)
     );

     struct Eval : elpc::ASTVisitor<int> {
       int visit(const elpc::LiteralExpr &n) override { return n.intValue; }
       int visit(const elpc::BinaryExpr &n) override {
         auto l = visit(*n.left), r = visit(*n.right);
         if (n.op == elpc::BinaryOp::ADD) return l + r;
         if (n.op == elpc::BinaryOp::SUB) return l - r;
         if (n.op == elpc::BinaryOp::MUL) return l * r;
         return l / r;
       }
     };
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <elpc/core/loc.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace elpc {

// -----------------------------------------------------------------------
// BumpAllocator — fast arena allocator for AST nodes
// All allocations are freed at once when the pool is destroyed.
// -----------------------------------------------------------------------

class BumpAllocator {
  struct Block {
    size_t capacity;
    size_t used = 0;
    std::unique_ptr<std::byte[]> data;
    explicit Block(size_t cap)
      : capacity(cap), data(std::make_unique<std::byte[]>(cap)) {}
  };
  std::vector<Block> blocks_;
  static constexpr size_t BLOCK_SIZE = 64 * 1024; // 64KB

  Block &currentBlock() {
    if (blocks_.empty() || blocks_.back().used >= blocks_.back().capacity)
      blocks_.emplace_back(BLOCK_SIZE);
    return blocks_.back();
  }

public:
  BumpAllocator() { blocks_.emplace_back(BLOCK_SIZE); }

  template <typename T, typename... Args>
  T *alloc(Args &&...args) {
    auto &block = currentBlock();
    size_t sz = sizeof(T);
    size_t align = alignof(T);
    // Align the current offset
    size_t start = (block.used + align - 1) & ~(align - 1);
    if (start + sz > block.capacity) {
      blocks_.emplace_back(std::max(sz + align, BLOCK_SIZE));
      return alloc<T>(std::forward<Args>(args)...);
    }
    block.used = start + sz;
    T *ptr = reinterpret_cast<T*>(block.data.get() + start);
    std::construct_at(ptr, std::forward<Args>(args)...);
    return ptr;
  }

  void reset() {
    blocks_.clear();
    blocks_.emplace_back(BLOCK_SIZE);
  }
};

// -----------------------------------------------------------------------
// Node kind enum — used for type-safe dispatch
// -----------------------------------------------------------------------

enum class NodeKind : uint8_t {
  // Expressions
  LITERAL_INT,
  LITERAL_FLOAT,
  LITERAL_BOOL,
  LITERAL_STRING,
  VAR,
  UNARY,
  BINARY,
  CALL,
  ASSIGN,
  // Statements
  BLOCK,
  IF,
  WHILE,
  FOR,
  RETURN,
  VAR_DECL,
  EXPR_STMT,
};

// -----------------------------------------------------------------------
// Operators
// -----------------------------------------------------------------------

enum class UnaryOp : uint8_t { NEG, NOT, BIT_NOT, PRE_INC, PRE_DEC, POST_INC, POST_DEC };

enum class BinaryOp : uint8_t {
  ADD, SUB, MUL, DIV, MOD,
  EQ, NE, LT, GT, LE, GE,
  AND, OR,
  BIT_AND, BIT_OR, BIT_XOR,
  SHL, SHR,
  ASSIGN,
};

// -----------------------------------------------------------------------
// ASTNode base
// -----------------------------------------------------------------------

struct ASTNode {
  SourceLocation location;

  explicit ASTNode(SourceLocation loc = {}) : location(loc) {}
  virtual ~ASTNode() = default;

  virtual NodeKind nodeKind() const = 0;
  virtual std::string_view nodeName() const = 0;
};

/// makeNode — convenience factory that returns a unique_ptr.
template <typename T, typename... Args>
std::unique_ptr<T> makeNode(Args &&...args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

// =====================================================================
// Expression nodes
// =====================================================================

/// Integer literal (e.g. 42)
struct LiteralExpr : ASTNode {
  int64_t intValue = 0;

  LiteralExpr() = default;
  explicit LiteralExpr(int64_t val, SourceLocation loc = {})
    : ASTNode(loc), intValue(val) {}

  NodeKind nodeKind() const override { return NodeKind::LITERAL_INT; }
  std::string_view nodeName() const override { return "LiteralExpr"; }
};

/// Floating-point literal (e.g. 3.14)
struct FloatExpr : ASTNode {
  double floatValue = 0.0;

  FloatExpr() = default;
  explicit FloatExpr(double val, SourceLocation loc = {})
    : ASTNode(loc), floatValue(val) {}

  NodeKind nodeKind() const override { return NodeKind::LITERAL_FLOAT; }
  std::string_view nodeName() const override { return "FloatExpr"; }
};

/// Boolean literal (true / false)
struct BoolExpr : ASTNode {
  bool boolValue = false;

  BoolExpr() = default;
  explicit BoolExpr(bool val, SourceLocation loc = {})
    : ASTNode(loc), boolValue(val) {}

  NodeKind nodeKind() const override { return NodeKind::LITERAL_BOOL; }
  std::string_view nodeName() const override { return "BoolExpr"; }
};

/// String literal
struct StringExpr : ASTNode {
  std::string stringValue;

  StringExpr() = default;
  explicit StringExpr(std::string val, SourceLocation loc = {})
    : ASTNode(loc), stringValue(std::move(val)) {}

  NodeKind nodeKind() const override { return NodeKind::LITERAL_STRING; }
  std::string_view nodeName() const override { return "StringExpr"; }
};

/// Variable reference
struct VarExpr : ASTNode {
  std::string name;

  VarExpr() = default;
  VarExpr(std::string name, SourceLocation loc = {})
    : ASTNode(loc), name(std::move(name)) {}

  NodeKind nodeKind() const override { return NodeKind::VAR; }
  std::string_view nodeName() const override { return "VarExpr"; }
};

/// Unary operation (e.g. -x, !flag)
struct UnaryExpr : ASTNode {
  UnaryOp op = UnaryOp::NEG;
  std::unique_ptr<ASTNode> operand;

  UnaryExpr() = default;
  UnaryExpr(UnaryOp op, std::unique_ptr<ASTNode> operand, SourceLocation loc = {})
    : ASTNode(loc), op(op), operand(std::move(operand)) {}

  NodeKind nodeKind() const override { return NodeKind::UNARY; }
  std::string_view nodeName() const override { return "UnaryExpr"; }
};

/// Binary operation (e.g. a + b, x == y)
struct BinaryExpr : ASTNode {
  BinaryOp op = BinaryOp::ADD;
  std::unique_ptr<ASTNode> left;
  std::unique_ptr<ASTNode> right;

  BinaryExpr() = default;
  BinaryExpr(BinaryOp op,
             std::unique_ptr<ASTNode> left,
             std::unique_ptr<ASTNode> right,
             SourceLocation loc = {})
    : ASTNode(loc), op(op), left(std::move(left)), right(std::move(right)) {}

  NodeKind nodeKind() const override { return NodeKind::BINARY; }
  std::string_view nodeName() const override { return "BinaryExpr"; }
};

/// Function call (e.g. foo(a, b))
struct CallExpr : ASTNode {
  std::unique_ptr<ASTNode> callee;  // VarExpr or another expression
  std::vector<std::unique_ptr<ASTNode>> args;

  CallExpr() = default;
  CallExpr(std::unique_ptr<ASTNode> callee,
           std::vector<std::unique_ptr<ASTNode>> args,
           SourceLocation loc = {})
    : ASTNode(loc), callee(std::move(callee)), args(std::move(args)) {}

  NodeKind nodeKind() const override { return NodeKind::CALL; }
  std::string_view nodeName() const override { return "CallExpr"; }
};

/// Assignment (x = expr)
struct AssignExpr : ASTNode {
  std::string target;  // variable name being assigned to
  std::unique_ptr<ASTNode> value;

  AssignExpr() = default;
  AssignExpr(std::string target, std::unique_ptr<ASTNode> value,
             SourceLocation loc = {})
    : ASTNode(loc), target(std::move(target)), value(std::move(value)) {}

  NodeKind nodeKind() const override { return NodeKind::ASSIGN; }
  std::string_view nodeName() const override { return "AssignExpr"; }
};

// =====================================================================
// Statement nodes
// =====================================================================

/// Block — a sequence of statements
struct BlockStmt : ASTNode {
  std::vector<std::unique_ptr<ASTNode>> stmts;

  BlockStmt() = default;
  explicit BlockStmt(std::vector<std::unique_ptr<ASTNode>> stmts,
                     SourceLocation loc = {})
    : ASTNode(loc), stmts(std::move(stmts)) {}

  NodeKind nodeKind() const override { return NodeKind::BLOCK; }
  std::string_view nodeName() const override { return "BlockStmt"; }
};

/// If statement
struct IfStmt : ASTNode {
  std::unique_ptr<ASTNode> condition;
  std::unique_ptr<ASTNode> thenBranch;
  std::unique_ptr<ASTNode> elseBranch;  // nullptr if no else

  IfStmt() = default;
  IfStmt(std::unique_ptr<ASTNode> condition,
         std::unique_ptr<ASTNode> thenBranch,
         std::unique_ptr<ASTNode> elseBranch = nullptr,
         SourceLocation loc = {})
    : ASTNode(loc),
      condition(std::move(condition)),
      thenBranch(std::move(thenBranch)),
      elseBranch(std::move(elseBranch)) {}

  NodeKind nodeKind() const override { return NodeKind::IF; }
  std::string_view nodeName() const override { return "IfStmt"; }
};

/// While loop
struct WhileStmt : ASTNode {
  std::unique_ptr<ASTNode> condition;
  std::unique_ptr<ASTNode> body;

  WhileStmt() = default;
  WhileStmt(std::unique_ptr<ASTNode> condition,
            std::unique_ptr<ASTNode> body,
            SourceLocation loc = {})
    : ASTNode(loc), condition(std::move(condition)), body(std::move(body)) {}

  NodeKind nodeKind() const override { return NodeKind::WHILE; }
  std::string_view nodeName() const override { return "WhileStmt"; }
};

/// For loop
struct ForStmt : ASTNode {
  std::unique_ptr<ASTNode> init;     // VarDecl, AssignExpr, or nullptr
  std::unique_ptr<ASTNode> condition; // expression or nullptr
  std::unique_ptr<ASTNode> increment; // expression or nullptr
  std::unique_ptr<ASTNode> body;

  ForStmt() = default;
  ForStmt(std::unique_ptr<ASTNode> init,
          std::unique_ptr<ASTNode> condition,
          std::unique_ptr<ASTNode> increment,
          std::unique_ptr<ASTNode> body,
          SourceLocation loc = {})
    : ASTNode(loc),
      init(std::move(init)),
      condition(std::move(condition)),
      increment(std::move(increment)),
      body(std::move(body)) {}

  NodeKind nodeKind() const override { return NodeKind::FOR; }
  std::string_view nodeName() const override { return "ForStmt"; }
};

/// Return statement
struct ReturnStmt : ASTNode {
  std::unique_ptr<ASTNode> value;  // nullptr for bare 'return'

  ReturnStmt() = default;
  explicit ReturnStmt(std::unique_ptr<ASTNode> value,
                      SourceLocation loc = {})
    : ASTNode(loc), value(std::move(value)) {}

  NodeKind nodeKind() const override { return NodeKind::RETURN; }
  std::string_view nodeName() const override { return "ReturnStmt"; }
};

/// Variable declaration (let x = expr or int x = expr etc.)
struct VarDecl : ASTNode {
  std::string name;
  std::string typeName;             // empty if inferred
  std::unique_ptr<ASTNode> initializer;  // nullptr if no initializer

  VarDecl() = default;
  VarDecl(std::string name,
          std::unique_ptr<ASTNode> initializer = nullptr,
          std::string typeName = {},
          SourceLocation loc = {})
    : ASTNode(loc),
      name(std::move(name)),
      typeName(std::move(typeName)),
      initializer(std::move(initializer)) {}

  NodeKind nodeKind() const override { return NodeKind::VAR_DECL; }
  std::string_view nodeName() const override { return "VarDecl"; }
};

/// Expression used as a statement (e.g. foo(); or x = 5;)
struct ExprStmt : ASTNode {
  std::unique_ptr<ASTNode> expr;

  ExprStmt() = default;
  explicit ExprStmt(std::unique_ptr<ASTNode> expr, SourceLocation loc = {})
    : ASTNode(loc), expr(std::move(expr)) {}

  NodeKind nodeKind() const override { return NodeKind::EXPR_STMT; }
  std::string_view nodeName() const override { return "ExprStmt"; }
};

// =====================================================================
// Visitor base
// =====================================================================

/// Base class for AST visitors.
/// Dispatch is via NodeKind + static_cast on the base ASTNode& entry point.
/// Override the typed visit() methods you need; visitDefault() handles
/// everything else.
template <typename Ret = void>
struct ASTVisitor {
  virtual ~ASTVisitor() = default;

  /// Entry point — dispatches to the right typed visit() via NodeKind.
  Ret visit(ASTNode &node) {
    switch (node.nodeKind()) {
      case NodeKind::LITERAL_INT:    return visit(static_cast<LiteralExpr&>(node));
      case NodeKind::LITERAL_FLOAT:  return visit(static_cast<FloatExpr&>(node));
      case NodeKind::LITERAL_BOOL:   return visit(static_cast<BoolExpr&>(node));
      case NodeKind::LITERAL_STRING: return visit(static_cast<StringExpr&>(node));
      case NodeKind::VAR:            return visit(static_cast<VarExpr&>(node));
      case NodeKind::UNARY:          return visit(static_cast<UnaryExpr&>(node));
      case NodeKind::BINARY:         return visit(static_cast<BinaryExpr&>(node));
      case NodeKind::CALL:           return visit(static_cast<CallExpr&>(node));
      case NodeKind::ASSIGN:         return visit(static_cast<AssignExpr&>(node));
      case NodeKind::BLOCK:          return visit(static_cast<BlockStmt&>(node));
      case NodeKind::IF:             return visit(static_cast<IfStmt&>(node));
      case NodeKind::WHILE:          return visit(static_cast<WhileStmt&>(node));
      case NodeKind::FOR:            return visit(static_cast<ForStmt&>(node));
      case NodeKind::RETURN:         return visit(static_cast<ReturnStmt&>(node));
      case NodeKind::VAR_DECL:       return visit(static_cast<VarDecl&>(node));
      case NodeKind::EXPR_STMT:      return visit(static_cast<ExprStmt&>(node));
    }
    return visitDefault(node);
  }

  // --- Override these ---

  // Expressions
  virtual Ret visit(LiteralExpr &n)    { return visitDefault(n); }
  virtual Ret visit(FloatExpr &n)      { return visitDefault(n); }
  virtual Ret visit(BoolExpr &n)       { return visitDefault(n); }
  virtual Ret visit(StringExpr &n)     { return visitDefault(n); }
  virtual Ret visit(VarExpr &n)        { return visitDefault(n); }
  virtual Ret visit(UnaryExpr &n)      { return visitDefault(n); }
  virtual Ret visit(BinaryExpr &n)     { return visitDefault(n); }
  virtual Ret visit(CallExpr &n)       { return visitDefault(n); }
  virtual Ret visit(AssignExpr &n)     { return visitDefault(n); }

  // Statements
  virtual Ret visit(BlockStmt &n)      { return visitDefault(n); }
  virtual Ret visit(IfStmt &n)         { return visitDefault(n); }
  virtual Ret visit(WhileStmt &n)      { return visitDefault(n); }
  virtual Ret visit(ForStmt &n)        { return visitDefault(n); }
  virtual Ret visit(ReturnStmt &n)     { return visitDefault(n); }
  virtual Ret visit(VarDecl &n)        { return visitDefault(n); }
  virtual Ret visit(ExprStmt &n)       { return visitDefault(n); }

  /// Catch-all for unhandled node types.
  virtual Ret visitDefault(ASTNode &)  { return Ret{}; }
};

// =====================================================================
// Built-in: ASTPrinter — indented tree dump for debugging
// =====================================================================

struct ASTPrinter : ASTVisitor<void> {
  using ASTVisitor<void>::visit; // bring base dispatcher into scope
  int indent = 0;
  std::string out;

  void println(std::string_view s) {
    out.append(static_cast<size_t>(indent) * 2, ' ');
    out += s;
    out += '\n';
  }

  void visit(LiteralExpr &n) override {
    println("LiteralExpr(" + std::to_string(n.intValue) + ")");
  }

  void visit(FloatExpr &n) override {
    println("FloatExpr(" + std::to_string(n.floatValue) + ")");
  }

  void visit(BoolExpr &n) override {
    println("BoolExpr(" + std::string(n.boolValue ? "true" : "false") + ")");
  }

  void visit(StringExpr &n) override {
    println("StringExpr(\"" + n.stringValue + "\")");
  }

  void visit(VarExpr &n) override {
    println("VarExpr(" + n.name + ")");
  }

  void visit(UnaryExpr &n) override {
    const char *opStr = "";
    switch (n.op) {
      case UnaryOp::NEG: opStr = "-"; break;
      case UnaryOp::NOT: opStr = "!"; break;
      default: opStr = "?"; break;
    }
    println("UnaryExpr(" + std::string(opStr) + ")");
    indent++;
    visit(*n.operand);
    indent--;
  }

  void visit(BinaryExpr &n) override {
    const char *opStr = "";
    switch (n.op) {
      case BinaryOp::ADD: opStr = "+"; break;
      case BinaryOp::SUB: opStr = "-"; break;
      case BinaryOp::MUL: opStr = "*"; break;
      case BinaryOp::DIV: opStr = "/"; break;
      case BinaryOp::EQ:  opStr = "=="; break;
      case BinaryOp::NE:  opStr = "!="; break;
      case BinaryOp::LT:  opStr = "<"; break;
      case BinaryOp::GT:  opStr = ">"; break;
      case BinaryOp::LE:  opStr = "<="; break;
      case BinaryOp::GE:  opStr = ">="; break;
      case BinaryOp::AND: opStr = "&&"; break;
      case BinaryOp::OR:  opStr = "||"; break;
      case BinaryOp::MOD: opStr = "%"; break;
      case BinaryOp::SHL: opStr = "<<"; break;
      case BinaryOp::SHR: opStr = ">>"; break;
      default: opStr = "?"; break;
    }
    println("BinaryExpr(" + std::string(opStr) + ")");
    indent++;
    visit(*n.left);
    visit(*n.right);
    indent--;
  }

  void visit(CallExpr &n) override {
    println("CallExpr");
    indent++;
    visit(*n.callee);
    for (auto &arg : n.args)
      visit(*arg);
    indent--;
  }

  void visit(AssignExpr &n) override {
    println("AssignExpr(" + n.target + ")");
    indent++;
    visit(*n.value);
    indent--;
  }

  void visit(BlockStmt &n) override {
    println("BlockStmt");
    indent++;
    for (auto &stmt : n.stmts)
      visit(*stmt);
    indent--;
  }

  void visit(IfStmt &n) override {
    println("IfStmt");
    indent++;
    println("condition:");
    indent++;
    visit(*n.condition);
    indent--;
    println("then:");
    indent++;
    visit(*n.thenBranch);
    indent--;
    if (n.elseBranch) {
      println("else:");
      indent++;
      visit(*n.elseBranch);
      indent--;
    }
    indent--;
  }

  void visit(WhileStmt &n) override {
    println("WhileStmt");
    indent++;
    println("condition:");
    indent++;
    visit(*n.condition);
    indent--;
    println("body:");
    indent++;
    visit(*n.body);
    indent--;
    indent--;
  }

  void visit(ReturnStmt &n) override {
    if (n.value) {
      println("ReturnStmt");
      indent++;
      visit(*n.value);
      indent--;
    } else {
      println("ReturnStmt(void)");
    }
  }

  void visit(VarDecl &n) override {
    std::string s = "VarDecl(" + n.name;
    if (!n.typeName.empty()) s += ": " + n.typeName;
    s += ")";
    println(s);
    if (n.initializer) {
      indent++;
      visit(*n.initializer);
      indent--;
    }
  }

  void visit(ExprStmt &n) override {
    visit(*n.expr);
  }
};

// =====================================================================
// ASTTransform — AST-to-AST transformation base class
//
// Override transform*() methods for node types you want to rewrite.
// Default implementation clones the node and recursively transforms
// children. Return nullptr to delete a node.
// =====================================================================

template <typename Derived>
struct ASTTransform {
  Derived &derived() { return static_cast<Derived&>(*this); }

  // --- Entry point ---
  std::unique_ptr<ASTNode> transform(ASTNode &node) {
    switch (node.nodeKind()) {
      case NodeKind::LITERAL_INT:    return transformLiteral(static_cast<LiteralExpr&>(node));
      case NodeKind::LITERAL_FLOAT:  return transformFloat(static_cast<FloatExpr&>(node));
      case NodeKind::LITERAL_BOOL:   return transformBool(static_cast<BoolExpr&>(node));
      case NodeKind::LITERAL_STRING: return transformString(static_cast<StringExpr&>(node));
      case NodeKind::VAR:            return transformVar(static_cast<VarExpr&>(node));
      case NodeKind::UNARY:          return transformUnary(static_cast<UnaryExpr&>(node));
      case NodeKind::BINARY:         return transformBinary(static_cast<BinaryExpr&>(node));
      case NodeKind::CALL:           return transformCall(static_cast<CallExpr&>(node));
      case NodeKind::ASSIGN:         return transformAssign(static_cast<AssignExpr&>(node));
      case NodeKind::BLOCK:          return transformBlock(static_cast<BlockStmt&>(node));
      case NodeKind::IF:             return transformIf(static_cast<IfStmt&>(node));
      case NodeKind::WHILE:          return transformWhile(static_cast<WhileStmt&>(node));
      case NodeKind::FOR:            return transformFor(static_cast<ForStmt&>(node));
      case NodeKind::RETURN:         return transformReturn(static_cast<ReturnStmt&>(node));
      case NodeKind::VAR_DECL:       return transformVarDecl(static_cast<VarDecl&>(node));
      case NodeKind::EXPR_STMT:      return transformExprStmt(static_cast<ExprStmt&>(node));
    }
    return nullptr;
  }

  // --- Default implementations: clone recursively ---

  virtual std::unique_ptr<ASTNode> transformLiteral(LiteralExpr &n) {
    return makeNode<LiteralExpr>(n.intValue, n.location);
  }
  virtual std::unique_ptr<ASTNode> transformFloat(FloatExpr &n) {
    return makeNode<FloatExpr>(n.floatValue, n.location);
  }
  virtual std::unique_ptr<ASTNode> transformBool(BoolExpr &n) {
    return makeNode<BoolExpr>(n.boolValue, n.location);
  }
  virtual std::unique_ptr<ASTNode> transformString(StringExpr &n) {
    return makeNode<StringExpr>(n.stringValue, n.location);
  }
  virtual std::unique_ptr<ASTNode> transformVar(VarExpr &n) {
    return makeNode<VarExpr>(n.name, n.location);
  }
  virtual std::unique_ptr<ASTNode> transformUnary(UnaryExpr &n) {
    auto op = derived().transform(*n.operand);
    if (!op) return nullptr;
    return makeNode<UnaryExpr>(n.op, std::move(op), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformBinary(BinaryExpr &n) {
    auto l = derived().transform(*n.left);
    auto r = derived().transform(*n.right);
    if (!l || !r) return nullptr;
    return makeNode<BinaryExpr>(n.op, std::move(l), std::move(r), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformCall(CallExpr &n) {
    auto callee = derived().transform(*n.callee);
    if (!callee) return nullptr;
    std::vector<std::unique_ptr<ASTNode>> args;
    for (auto &a : n.args) {
      auto ta = derived().transform(*a);
      if (ta) args.push_back(std::move(ta));
    }
    return makeNode<CallExpr>(std::move(callee), std::move(args), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformAssign(AssignExpr &n) {
    auto v = derived().transform(*n.value);
    if (!v) return nullptr;
    return makeNode<AssignExpr>(n.target, std::move(v), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformBlock(BlockStmt &n) {
    std::vector<std::unique_ptr<ASTNode>> stmts;
    for (auto &s : n.stmts) {
      auto ts = derived().transform(*s);
      if (ts) stmts.push_back(std::move(ts));
    }
    return makeNode<BlockStmt>(std::move(stmts), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformIf(IfStmt &n) {
    auto c = derived().transform(*n.condition);
    auto t = derived().transform(*n.thenBranch);
    std::unique_ptr<ASTNode> e;
    if (n.elseBranch) e = derived().transform(*n.elseBranch);
    if (!c || !t) return nullptr;
    return makeNode<IfStmt>(std::move(c), std::move(t), std::move(e), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformWhile(WhileStmt &n) {
    auto c = derived().transform(*n.condition);
    auto b = derived().transform(*n.body);
    if (!c || !b) return nullptr;
    return makeNode<WhileStmt>(std::move(c), std::move(b), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformFor(ForStmt &n) {
    auto i = n.init ? derived().transform(*n.init) : nullptr;
    auto c = n.condition ? derived().transform(*n.condition) : nullptr;
    auto inc = n.increment ? derived().transform(*n.increment) : nullptr;
    auto b = derived().transform(*n.body);
    if (!b) return nullptr;
    return makeNode<ForStmt>(std::move(i), std::move(c), std::move(inc),
                              std::move(b), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformReturn(ReturnStmt &n) {
    auto v = n.value ? derived().transform(*n.value) : nullptr;
    return makeNode<ReturnStmt>(std::move(v), n.location);
  }
  virtual std::unique_ptr<ASTNode> transformVarDecl(VarDecl &n) {
    auto init = n.initializer ? derived().transform(*n.initializer) : nullptr;
    return makeNode<VarDecl>(n.name, std::move(init), n.typeName, n.location);
  }
  virtual std::unique_ptr<ASTNode> transformExprStmt(ExprStmt &n) {
    auto e = derived().transform(*n.expr);
    if (!e) return nullptr;
    return makeNode<ExprStmt>(std::move(e), n.location);
  }
};

} // namespace elpc
