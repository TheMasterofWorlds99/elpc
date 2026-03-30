// tests/TestPratt.cpp
#include "TestHelpers.hpp"
#include <cmath>
#include <elpc/parser/prattParser.hpp>

// ----------------------------------------------------------------
// Token types for expression parsing
// ----------------------------------------------------------------

enum class ExprToken {
  INT_LIT,
  FLOAT_LIT,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  PERCENT,
  CARET,
  BANG,
  LEFT_PAREN,
  RIGHT_PAREN,
  END
};

inline elpc::Lexer<ExprToken> makeExprLexer(const std::string &src) {
  elpc::Lexer<ExprToken> lexer(src);
  lexer.addRule(ExprToken::FLOAT_LIT, "\\b[0-9]+\\.[0-9]+\\b");
  lexer.addRule(ExprToken::INT_LIT, "\\b[0-9]+\\b");
  lexer.addRule(ExprToken::PLUS, "\\+");
  lexer.addRule(ExprToken::MINUS, "\\-");
  lexer.addRule(ExprToken::STAR, "\\*");
  lexer.addRule(ExprToken::SLASH, "\\/");
  lexer.addRule(ExprToken::PERCENT, "\\%");
  lexer.addRule(ExprToken::CARET, "\\^");
  lexer.addRule(ExprToken::BANG, "\\!");
  lexer.addRule(ExprToken::LEFT_PAREN, "\\(");
  lexer.addRule(ExprToken::RIGHT_PAREN, "\\)");
  lexer.addSkip("\\s+");
  return lexer;
}

// ----------------------------------------------------------------
// Expression Node
// ----------------------------------------------------------------

struct ExprNode {
  double value = 0.0;
  std::string repr;

  ExprNode() = default;
  ExprNode(double v, std::string r) : value(v), repr(std::move(r)) {}
};

// ----------------------------------------------------------------
// Expression Parser
// ----------------------------------------------------------------

class ExprParser : public elpc::PrattParser<ExprToken, ExprNode> {
public:
  ExprParser(const std::vector<elpc::Token<ExprToken>> &tokens)
      : elpc::PrattParser<ExprToken, ExprNode>(tokens) {
    using Prec = elpc::Precedence;
    using Tok = elpc::Token<ExprToken>;

    // --- Prefix parselets ---

    // Integer literal
    registerPrefix(ExprToken::INT_LIT, [](const Tok &tok) {
      int v = tok.toInt().value_or(0);
      return ExprNode((double)v, std::to_string(v));
    });

    // Float literal
    registerPrefix(ExprToken::FLOAT_LIT, [](const Tok &tok) {
      double v = std::stod(std::string(tok.lexeme));
      return ExprNode(v, std::string(tok.lexeme));
    });

    // Unary minus
    registerPrefix(ExprToken::MINUS, [this](const Tok &tok) {
      auto operand = parseExpression(Prec::UNARY);
      return ExprNode(-operand.value, "(-" + operand.repr + ")");
    });

    // Unary bang
    registerPrefix(ExprToken::BANG, [this](const Tok &tok) {
      auto operand = parseExpression(Prec::UNARY);
      return ExprNode(operand.value == 0.0 ? 1.0 : 0.0,
                      "(!" + operand.repr + ")");
    });

    // Grouping
    registerPrefix(ExprToken::LEFT_PAREN, [this](const Tok &tok) {
      auto inner = parseExpression(Prec::NONE);
      expect(ExprToken::RIGHT_PAREN, "Expected ')' after grouped expression");
      return ExprNode(inner.value, "(" + inner.repr + ")");
    });

    // --- Infix parselets ---

    // Addition
    registerInfix(ExprToken::PLUS, Prec::TERM,
                  [this](const Tok &tok, ExprNode lhs) {
                    auto rhs = parseExpression(Prec::TERM);
                    return ExprNode(lhs.value + rhs.value,
                                    "(" + lhs.repr + " + " + rhs.repr + ")");
                  });

    // Subtraction
    registerInfix(ExprToken::MINUS, Prec::TERM,
                  [this](const Tok &tok, ExprNode lhs) {
                    auto rhs = parseExpression(Prec::TERM);
                    return ExprNode(lhs.value - rhs.value,
                                    "(" + lhs.repr + " - " + rhs.repr + ")");
                  });

    // Multiplication
    registerInfix(ExprToken::STAR, Prec::FACTOR,
                  [this](const Tok &tok, ExprNode lhs) {
                    auto rhs = parseExpression(Prec::FACTOR);
                    return ExprNode(lhs.value * rhs.value,
                                    "(" + lhs.repr + " * " + rhs.repr + ")");
                  });

    // Division
    registerInfix(ExprToken::SLASH, Prec::FACTOR,
                  [this](const Tok &tok, ExprNode lhs) {
                    auto rhs = parseExpression(Prec::FACTOR);
                    if (rhs.value == 0.0)
                      throw std::runtime_error("[elpc] Division by zero");
                    return ExprNode(lhs.value / rhs.value,
                                    "(" + lhs.repr + " / " + rhs.repr + ")");
                  });

    // Modulo — explicit types to allow std::function conversion
    registerInfix(ExprToken::PERCENT, Prec::FACTOR,
                  [this](const Tok &tok, ExprNode lhs) {
                    auto rhs = parseExpression(Prec::FACTOR);
                    return ExprNode(std::fmod(lhs.value, rhs.value),
                                    "(" + lhs.repr + " % " + rhs.repr + ")");
                  });

    // Exponentiation — right associative
    registerInfix(
        ExprToken::CARET, Prec::UNARY, [this](const Tok &tok, ExprNode lhs) {
          auto rhs =
              parseExpression(static_cast<int>(elpc::Precedence::UNARY) - 1);
          return ExprNode(std::pow(lhs.value, rhs.value),
                          "(" + lhs.repr + " ^ " + rhs.repr + ")");
        });
  }

  ExprNode parseExpr() { return parseExpression(elpc::Precedence::NONE); }
};

// ----------------------------------------------------------------
// Test runner
// ----------------------------------------------------------------

void runExprTest(const std::string &label, const std::string &src,
                 double expected) {
  std::cout << "\n=== " << label << " ===\n";
  std::cout << "expr:     " << src << "\n";

  try {
    auto tokens = makeExprLexer(src).tokenize();
    ExprParser parser(tokens);
    auto result = parser.parseExpr();

    std::cout << "repr:     " << result.repr << "\n";
    std::cout << "value:    " << result.value << "\n";
    std::cout << "expected: " << expected << "\n";
    std::cout << "pass:     " << (result.value == expected ? "YES" : "NO")
              << "\n";

  } catch (const std::exception &e) {
    std::cerr << "[error] " << e.what() << "\n";
  }
}

int main() {
  // Basic arithmetic
  runExprTest("INTEGER LITERAL", "42", 42.0);
  runExprTest("ADDITION", "1 + 2", 3.0);
  runExprTest("SUBTRACTION", "10 - 3", 7.0);
  runExprTest("MULTIPLICATION", "4 * 5", 20.0);
  runExprTest("DIVISION", "10 / 4", 2.5);
  runExprTest("MODULO", "10 % 3", 1.0);

  // Precedence
  runExprTest("PRECEDENCE: * before +", "2 + 3 * 4", 14.0);
  runExprTest("PRECEDENCE: / before -", "10 - 6 / 2", 7.0);
  runExprTest("PRECEDENCE: mixed", "2 + 3 * 4 - 1", 13.0);

  // Grouping
  runExprTest("GROUPING: overrides *", "(2 + 3) * 4", 20.0);
  runExprTest("GROUPING: nested", "(2 + (3 * 4))", 14.0);
  runExprTest("GROUPING: deep", "((2 + 3) * (4 - 1))", 15.0);

  // Unary
  runExprTest("UNARY MINUS", "-5", -5.0);
  runExprTest("UNARY MINUS expr", "-(2 + 3)", -5.0);
  runExprTest("UNARY MINUS in expr", "10 + -3", 7.0);
  runExprTest("UNARY BANG: false", "!0", 1.0);
  runExprTest("UNARY BANG: true", "!1", 0.0);

  // Right associativity
  runExprTest("EXPONENT: right assoc", "2 ^ 3 ^ 2", 512.0);
  runExprTest("EXPONENT: simple", "2 ^ 10", 1024.0);

  // Float
  runExprTest("FLOAT LITERAL", "3.14", 3.14);
  runExprTest("FLOAT ARITHMETIC", "1.5 + 2.5", 4.0);

  // Error cases
  runExprTest("DIVISION BY ZERO", "10 / 0", 0.0);
  runExprTest("MISSING PAREN", "(1 + 2", 0.0);
}
