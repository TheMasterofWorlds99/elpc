#include <elpc/ast/ast.hpp>
#include <cassert>
#include <iostream>
#include <string>
#include <memory>

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testLiteralExpr() {
  auto lit = elpc::makeNode<elpc::LiteralExpr>(42, elpc::SourceLocation{1, 1});
  CHECK(lit->nodeKind() == elpc::NodeKind::LITERAL_INT, "kind should be LITERAL_INT");
  CHECK(lit->intValue == 42, "value should be 42");
  CHECK(lit->location.line == 1, "line should be 1");
  CHECK(std::string(lit->nodeName()) == "LiteralExpr", "nodeName should match");

  std::cout << "  PASS literalExpr\n";
  return 0;
}

static int testFloatExpr() {
  auto flt = elpc::makeNode<elpc::FloatExpr>(3.14);
  CHECK(flt->nodeKind() == elpc::NodeKind::LITERAL_FLOAT, "kind should be LITERAL_FLOAT");
  CHECK(flt->floatValue == 3.14, "value should be 3.14");

  std::cout << "  PASS floatExpr\n";
  return 0;
}

static int testBoolExpr() {
  auto b = elpc::makeNode<elpc::BoolExpr>(true);
  CHECK(b->nodeKind() == elpc::NodeKind::LITERAL_BOOL, "kind should be LITERAL_BOOL");
  CHECK(b->boolValue == true, "value should be true");

  auto b2 = elpc::makeNode<elpc::BoolExpr>(false);
  CHECK(b2->boolValue == false, "value should be false");

  std::cout << "  PASS boolExpr\n";
  return 0;
}

static int testStringExpr() {
  auto s = elpc::makeNode<elpc::StringExpr>("hello");
  CHECK(s->nodeKind() == elpc::NodeKind::LITERAL_STRING, "kind should be LITERAL_STRING");
  CHECK(s->stringValue == "hello", "value should be 'hello'");

  std::cout << "  PASS stringExpr\n";
  return 0;
}

static int testVarExpr() {
  auto v = elpc::makeNode<elpc::VarExpr>("myVar");
  CHECK(v->nodeKind() == elpc::NodeKind::VAR, "kind should be VAR");
  CHECK(v->name == "myVar", "name should be 'myVar'");

  std::cout << "  PASS varExpr\n";
  return 0;
}

static int testBinaryExpr() {
  auto left = elpc::makeNode<elpc::LiteralExpr>(1);
  auto right = elpc::makeNode<elpc::LiteralExpr>(2);
  auto bin = elpc::makeNode<elpc::BinaryExpr>(
    elpc::BinaryOp::ADD, std::move(left), std::move(right));

  CHECK(bin->nodeKind() == elpc::NodeKind::BINARY, "kind should be BINARY");
  CHECK(bin->op == elpc::BinaryOp::ADD, "op should be ADD");
  CHECK(bin->left != nullptr, "left should exist");
  CHECK(bin->right != nullptr, "right should exist");
  CHECK(static_cast<elpc::LiteralExpr*>(bin->left.get())->intValue == 1, "left value should be 1");
  CHECK(static_cast<elpc::LiteralExpr*>(bin->right.get())->intValue == 2, "right value should be 2");

  std::cout << "  PASS binaryExpr\n";
  return 0;
}

static int testUnaryExpr() {
  auto op = elpc::makeNode<elpc::LiteralExpr>(5);
  auto un = elpc::makeNode<elpc::UnaryExpr>(elpc::UnaryOp::NEG, std::move(op));

  CHECK(un->nodeKind() == elpc::NodeKind::UNARY, "kind should be UNARY");
  CHECK(un->op == elpc::UnaryOp::NEG, "op should be NEG");
  CHECK(un->operand != nullptr, "operand should exist");

  std::cout << "  PASS unaryExpr\n";
  return 0;
}

static int testCallExpr() {
  auto callee = elpc::makeNode<elpc::VarExpr>("foo");
  std::vector<std::unique_ptr<elpc::ASTNode>> args;
  args.push_back(elpc::makeNode<elpc::LiteralExpr>(1));
  args.push_back(elpc::makeNode<elpc::LiteralExpr>(2));

  auto call = elpc::makeNode<elpc::CallExpr>(std::move(callee), std::move(args));
  CHECK(call->nodeKind() == elpc::NodeKind::CALL, "kind should be CALL");
  CHECK(static_cast<elpc::VarExpr*>(call->callee.get())->name == "foo", "callee should be 'foo'");
  CHECK(call->args.size() == 2, "should have 2 args");

  std::cout << "  PASS callExpr\n";
  return 0;
}

static int testBlockStmt() {
  std::vector<std::unique_ptr<elpc::ASTNode>> stmts;
  stmts.push_back(elpc::makeNode<elpc::LiteralExpr>(1));
  stmts.push_back(elpc::makeNode<elpc::LiteralExpr>(2));

  auto block = elpc::makeNode<elpc::BlockStmt>(std::move(stmts));
  CHECK(block->nodeKind() == elpc::NodeKind::BLOCK, "kind should be BLOCK");
  CHECK(block->stmts.size() == 2, "should have 2 statements");

  std::cout << "  PASS blockStmt\n";
  return 0;
}

static int testIfStmt() {
  auto cond = elpc::makeNode<elpc::LiteralExpr>(1);
  auto thenB = elpc::makeNode<elpc::LiteralExpr>(2);

  auto ifStmt = elpc::makeNode<elpc::IfStmt>(std::move(cond), std::move(thenB));
  CHECK(ifStmt->nodeKind() == elpc::NodeKind::IF, "kind should be IF");
  CHECK(ifStmt->condition != nullptr, "condition should exist");
  CHECK(ifStmt->thenBranch != nullptr, "thenBranch should exist");
  CHECK(ifStmt->elseBranch == nullptr, "elseBranch should be null");

  std::cout << "  PASS ifStmt\n";
  return 0;
}

static int testIfElseStmt() {
  auto cond = elpc::makeNode<elpc::LiteralExpr>(1);
  auto thenB = elpc::makeNode<elpc::LiteralExpr>(2);
  auto elseB = elpc::makeNode<elpc::LiteralExpr>(3);

  auto ifStmt = elpc::makeNode<elpc::IfStmt>(std::move(cond), std::move(thenB), std::move(elseB));
  CHECK(ifStmt->elseBranch != nullptr, "elseBranch should exist");

  std::cout << "  PASS ifElseStmt\n";
  return 0;
}

static int testVisitorEval() {
  // Build: (1 + 2) * (10 - 3)
  using namespace elpc;

  auto ast = makeNode<BinaryExpr>(
    BinaryOp::MUL,
    makeNode<BinaryExpr>(BinaryOp::ADD,
      makeNode<LiteralExpr>(1), makeNode<LiteralExpr>(2)),
    makeNode<BinaryExpr>(BinaryOp::SUB,
      makeNode<LiteralExpr>(10), makeNode<LiteralExpr>(3))
  );

  struct Eval : ASTVisitor<int> {
    using ASTVisitor<int>::visit;
    int visit(LiteralExpr &n) override { return n.intValue; }
    int visit(BinaryExpr &n) override {
      int l = visit(*n.left);
      int r = visit(*n.right);
      switch (n.op) {
        case BinaryOp::ADD: return l + r;
        case BinaryOp::SUB: return l - r;
        case BinaryOp::MUL: return l * r;
        case BinaryOp::DIV: return l / r;
        default: return 0;
      }
    }
  };

  Eval eval;
  int result = eval.visit(*ast);
  CHECK(result == (1 + 2) * (10 - 3), "eval should compute correct result");

  std::cout << "  PASS visitorEval\n";
  return 0;
}

static int testVisitorWalkExprs() {
  using namespace elpc;

  // Build: -x + foo(1, 2)
  auto ast = makeNode<BinaryExpr>(
    BinaryOp::ADD,
    makeNode<UnaryExpr>(UnaryOp::NEG, makeNode<VarExpr>("x")),
    makeNode<CallExpr>(
      makeNode<VarExpr>("foo"),
      []{
        std::vector<std::unique_ptr<ASTNode>> args;
        args.push_back(makeNode<LiteralExpr>(1));
        args.push_back(makeNode<LiteralExpr>(2));
        return args;
      }()
    )
  );

  struct NodeCounter : ASTVisitor<int> {
    int count = 0;
    int visitDefault(ASTNode &) override { count++; return 0; }
  };

  NodeCounter counter;
  counter.visit(*ast);
  CHECK(counter.count > 0, "should visit at least one node");

  std::cout << "  PASS visitorWalkExprs\n";
  return 0;
}

static int testASTPrinter() {
  using namespace elpc;

  auto ast = makeNode<BinaryExpr>(
    BinaryOp::ADD,
    makeNode<LiteralExpr>(1),
    makeNode<VarExpr>("x")
  );

  ASTPrinter printer;
  printer.visit(*ast);
  CHECK(!printer.out.empty(), "printer should produce output");
  CHECK(printer.out.find("LiteralExpr(1)") != std::string::npos, "printer should contain value");
  CHECK(printer.out.find("VarExpr(x)") != std::string::npos, "printer should contain var");

  std::cout << "  PASS astPrinter\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testLiteralExpr();
  failures += testFloatExpr();
  failures += testBoolExpr();
  failures += testStringExpr();
  failures += testVarExpr();
  failures += testBinaryExpr();
  failures += testUnaryExpr();
  failures += testCallExpr();
  failures += testBlockStmt();
  failures += testIfStmt();
  failures += testIfElseStmt();
  failures += testVisitorEval();
  failures += testVisitorWalkExprs();
  failures += testASTPrinter();

  if (failures == 0)
    std::cout << "All TestAST tests passed!\n";
  else
    std::cerr << failures << " TestAST test(s) failed!\n";

  return failures;
}
