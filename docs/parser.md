# Parser module

Provides base classes for recursive descent and Pratt (operator-precedence) parsing.

## `Parser<TokenType>` (`elpc/parser/parser.hpp`)

Base class for recursive descent parsers. Subclass it and define your grammar methods using the protected helpers.

```cpp
class MyParser : public elpc::Parser<MyToken> {
public:
  MyParser(const std::vector<elpc::Token<MyToken>> &tokens)
    : elpc::Parser<MyToken>(tokens) {}

  std::unique_ptr<elpc::ASTNode> parseProgram() {
    // ... use peek(), match(), expect(), etc.
  }

private:
  // A simple expression: term (('+' | '-') term)*
  std::unique_ptr<elpc::ASTNode> parseExpr() {
    auto left = parseTerm();
    while (match(MyToken::PLUS) || match(MyToken::MINUS)) {
      auto op = previous().is(MyToken::PLUS)
                ? elpc::BinaryOp::ADD : elpc::BinaryOp::SUB;
      auto right = parseTerm();
      left = std::make_unique<elpc::BinaryExpr>(op, std::move(left),
                                                 std::move(right));
    }
    return left;
  }
};
```

### Protected API

| Method | Returns | Description |
|--------|---------|-------------|
| `peek(offset)` | `const Token&` | Lookahead (default 0 = current) |
| `consume()` | `const Token&` | Advance and return previous |
| `previous()` | `const Token&` | Most recently consumed token |
| `check(type)` | `bool` | True if peek matches type |
| `match(type)` | `bool` | Consume if matches type |
| `matchAny(types...)` | `bool` | Consume if matches any type |
| `expect(type, msg)` | `const Token&` | Consume or throw |
| `expectOneOf(msg, types...)` | `const Token&` | Consume or throw |
| `synchronize(types...)` | `void` | Skip tokens until one of the sync points |
| `isAtEnd()` | `bool` | True if no tokens remain |

### Error handling

By default `expect()` and `expectOneOf()` throw `std::runtime_error`. Attach a DiagnosticEngine to collect errors and continue parsing:

```cpp
elpc::DiagnosticEngine diag;
parser.setDiagnostics(diag);
parser.expect(MyToken::SEMI, "expected ';'");  // reports via diag, continues
```

## `PrattParser<TokenType, NodeType>` (`elpc/parser/prattParser.hpp`)

Extends Parser with an operator-precedence expression parsing engine. Register prefix and infix handlers for each token type.

```cpp
class ExprParser : public elpc::PrattParser<MyToken, ASTNode*> {
public:
  ExprParser(const std::vector<elpc::Token<MyToken>> &tokens)
    : elpc::PrattParser<MyToken, ASTNode*>(tokens) {

    registerPrefix(MyToken::INT, [](const auto &tok) {
      return new ASTNode(tok.toInt().value_or(0));
    });

    registerInfix(MyToken::PLUS, elpc::Precedence::TERM,
      [this](const auto &, auto lhs) {
        auto rhs = parseExpression(elpc::Precedence::TERM);
        return new ASTNode('+', lhs, rhs);
      });
  }

  ASTNode *parse() {
    return parseExpression(elpc::Precedence::NONE);
  }
};
```

### Precedence levels

| Level | Name | Operators |
|-------|------|-----------|
| 0 | NONE | Base |
| 1 | ASSIGNMENT | = |
| 2 | LOGICAL_OR | \|\| |
| 3 | LOGICAL_AND | && |
| 4 | EQUALITY | == != |
| 5 | COMPARISON | < > <= >= |
| 6 | TERM | + - |
| 7 | FACTOR | * / % |
| 8 | UNARY | - ! |
| 9 | PRIMARY | literals, grouping, calls |
