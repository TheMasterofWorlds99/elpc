# AST module

Reusable AST node types, a visitor pattern, and a built-in debug printer.

## Node types

All nodes inherit from `elpc::ASTNode` and carry a `SourceLocation`. Nodes own their children via `std::unique_ptr`.

### Expressions

| Node | Fields | Example |
|------|--------|---------|
| `LiteralExpr` | `int64_t intValue` | `42` |
| `FloatExpr` | `double floatValue` | `3.14` |
| `BoolExpr` | `bool boolValue` | `true` |
| `StringExpr` | `std::string stringValue` | `"hello"` |
| `VarExpr` | `std::string name` | `myVar` |
| `UnaryExpr` | `UnaryOp op`, `unique_ptr operand` | `-x`, `!flag` |
| `BinaryExpr` | `BinaryOp op`, `unique_ptr left/right` | `a + b` |
| `CallExpr` | `unique_ptr callee`, `vector<unique_ptr> args` | `foo(1, 2)` |
| `AssignExpr` | `string target`, `unique_ptr value` | `x = 42` |

### Statements

| Node | Fields | Example |
|------|--------|---------|
| `BlockStmt` | `vector<unique_ptr> stmts` | `{ let x = 1; }` |
| `IfStmt` | `unique_ptr cond/thenBranch/elseBranch` | `if (x) { ... } else { ... }` |
| `WhileStmt` | `unique_ptr cond/body` | `while (x < 10) { ... }` |
| `ForStmt` | `unique_ptr init/cond/increment/body` | `for (let i=0; i<n; i++)` |
| `ReturnStmt` | `unique_ptr value` (nullable) | `return x;` |
| `VarDecl` | `string name/typeName`, `unique_ptr init` | `let x: int = 42;` |
| `ExprStmt` | `unique_ptr expr` | `foo();` |

### Operators

```cpp
enum class UnaryOp { NEG, NOT, BIT_NOT, PRE_INC, PRE_DEC, POST_INC, POST_DEC };

enum class BinaryOp {
  ADD, SUB, MUL, DIV, MOD,
  EQ, NE, LT, GT, LE, GE,
  AND, OR,
  BIT_AND, BIT_OR, BIT_XOR,
  SHL, SHR,
  ASSIGN,
};
```

## Building nodes

```cpp
using namespace elpc;

auto expr = makeNode<BinaryExpr>(
    BinaryOp::ADD,
    makeNode<LiteralExpr>(1),
    makeNode<VarExpr>("x")
);
```

## Visitor pattern

Define a visitor by subclassing `ASTVisitor<Ret>` and overriding the `visit()` methods you need. The `visitDefault()` catch-all handles the rest.

```cpp
struct Eval : elpc::ASTVisitor<int> {
  using elpc::ASTVisitor<int>::visit;  // required due to name hiding

  int visit(elpc::LiteralExpr &n) override { return n.intValue; }
  int visit(elpc::BinaryExpr &n) override {
    int l = visit(*n.left);
    int r = visit(*n.right);
    switch (n.op) {
      case elpc::BinaryOp::ADD: return l + r;
      case elpc::BinaryOp::SUB: return l - r;
      case elpc::BinaryOp::MUL: return l * r;
      case elpc::BinaryOp::DIV: return l / r;
      default: return 0;
    }
  }
  int visitDefault(elpc::ASTNode &) override { return 0; }
};

Eval eval;
int result = eval.visit(*expr);
```

## Debug printer

```cpp
elpc::ASTPrinter printer;
printer.visit(*ast);
std::cout << printer.out;
// BlockStmt
//   VarDecl(x: int)
//     LiteralExpr(1)
//   WhileStmt
//     condition:
//       BinaryExpr(<=)
//         VarExpr(x)
//         LiteralExpr(10)
```
