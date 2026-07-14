#include <elpc/core/sourceManager.hpp>
#include <elpc/parser/prattParser.hpp>
#include <cassert>
#include <iostream>
#include <string>

enum class Tk {
  INT_LIT, PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN, EOF_
};

// Simple AST node
struct ExprNode {
  enum Kind { INT, BINARY };
  Kind kind;
  int value = 0;
  char op = 0;
  ExprNode *left = nullptr;
  ExprNode *right = nullptr;

  ExprNode() : kind(INT), value(0) {}
  ExprNode(int v) : kind(INT), value(v) {}
  ExprNode(char op, ExprNode *l, ExprNode *r)
    : kind(BINARY), value(0), op(op), left(l), right(r) {}
};

class ExprParser : public elpc::PrattParser<Tk, ExprNode*> {
public:
  ExprParser(const std::vector<elpc::Token<Tk>> &tokens)
    : elpc::PrattParser<Tk, ExprNode*>(tokens) {

    registerPrefix(Tk::INT_LIT, [this](const elpc::Token<Tk> &tok) {
      return new ExprNode(tok.toInt().value_or(0));
    });

    registerPrefix(Tk::LPAREN, [this](const elpc::Token<Tk> &) {
      auto *expr = parseExpression(elpc::Precedence::NONE);
      this->expect(Tk::RPAREN, "expected ')'");
      return expr;
    });

    registerInfix(Tk::PLUS, elpc::Precedence::TERM,
      [this](const elpc::Token<Tk> &, ExprNode *lhs) {
        auto *rhs = parseExpression(elpc::Precedence::TERM);
        return new ExprNode('+', lhs, rhs);
      });

    registerInfix(Tk::MINUS, elpc::Precedence::TERM,
      [this](const elpc::Token<Tk> &, ExprNode *lhs) {
        auto *rhs = parseExpression(elpc::Precedence::TERM);
        return new ExprNode('-', lhs, rhs);
      });

    registerInfix(Tk::STAR, elpc::Precedence::FACTOR,
      [this](const elpc::Token<Tk> &, ExprNode *lhs) {
        auto *rhs = parseExpression(elpc::Precedence::FACTOR);
        return new ExprNode('*', lhs, rhs);
      });

    registerInfix(Tk::SLASH, elpc::Precedence::FACTOR,
      [this](const elpc::Token<Tk> &, ExprNode *lhs) {
        auto *rhs = parseExpression(elpc::Precedence::FACTOR);
        return new ExprNode('/', lhs, rhs);
      });
  }

  ExprNode *parse() {
    return parseExpression(elpc::Precedence::NONE);
  }
};

static int eval(ExprNode *node) {
  if (node->kind == ExprNode::INT)
    return node->value;

  int l = eval(node->left);
  int r = eval(node->right);

  switch (node->op) {
    case '+': return l + r;
    case '-': return l - r;
    case '*': return l * r;
    case '/': return l / r;
    default: return 0;
  }
}

static void freeNode(ExprNode *node) {
  if (!node) return;
  if (node->kind == ExprNode::BINARY) {
    freeNode(node->left);
    freeNode(node->right);
  }
  delete node;
}

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static std::vector<elpc::Token<Tk>> lex(const elpc::SourceManager &src) {
  auto text = src.text();
  std::vector<elpc::Token<Tk>> tokens;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == ' ') continue;
    switch (c) {
      case '+': tokens.emplace_back(Tk::PLUS, src.slice(i, 1)); break;
      case '-': tokens.emplace_back(Tk::MINUS, src.slice(i, 1)); break;
      case '*': tokens.emplace_back(Tk::STAR, src.slice(i, 1)); break;
      case '/': tokens.emplace_back(Tk::SLASH, src.slice(i, 1)); break;
      case '(': tokens.emplace_back(Tk::LPAREN, src.slice(i, 1)); break;
      case ')': tokens.emplace_back(Tk::RPAREN, src.slice(i, 1)); break;
      default:
        if (c >= '0' && c <= '9') {
          size_t start = i;
          while (i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '9')
            ++i;
          tokens.emplace_back(Tk::INT_LIT, src.slice(start, i - start + 1));
        }
        break;
    }
  }
  return tokens;
}

static int testSimpleAddition() {
  auto src = elpc::SourceManager::fromString("test", "1+2");
  auto tokens = lex(src);
  ExprParser parser(tokens);
  auto *ast = parser.parse();

  CHECK(eval(ast) == 3, "1+2 should equal 3");

  freeNode(ast);
  std::cout << "  PASS simpleAddition\n";
  return 0;
}

static int testPrecedence() {
  auto src = elpc::SourceManager::fromString("test", "1+2*3");
  auto tokens = lex(src);
  ExprParser parser(tokens);
  auto *ast = parser.parse();

  CHECK(eval(ast) == 7, "1+2*3 should equal 7 (not 9)");

  freeNode(ast);
  std::cout << "  PASS precedence\n";
  return 0;
}

static int testParentheses() {
  auto src = elpc::SourceManager::fromString("test", "(1+2)*3");
  auto tokens = lex(src);
  ExprParser parser(tokens);
  auto *ast = parser.parse();

  CHECK(eval(ast) == 9, "(1+2)*3 should equal 9");

  freeNode(ast);
  std::cout << "  PASS parentheses\n";
  return 0;
}

static int testAssociativity() {
  auto src = elpc::SourceManager::fromString("test", "10-3-2");
  auto tokens = lex(src);
  ExprParser parser(tokens);
  auto *ast = parser.parse();

  CHECK(eval(ast) == 5, "10-3-2 should equal 5 (left-associative)");

  freeNode(ast);
  std::cout << "  PASS associativity\n";
  return 0;
}

static int testComplexExpression() {
  auto src = elpc::SourceManager::fromString("test", "2+3*4-10/2");
  auto tokens = lex(src);
  ExprParser parser(tokens);
  auto *ast = parser.parse();

  // 2 + (3*4) - (10/2) = 2 + 12 - 5 = 9
  CHECK(eval(ast) == 9, "2+3*4-10/2 should equal 9");

  freeNode(ast);
  std::cout << "  PASS complexExpression\n";
  return 0;
}

static int testNestedParentheses() {
  auto src = elpc::SourceManager::fromString("test", "((1+2)*(3+4))");
  auto tokens = lex(src);
  ExprParser parser(tokens);
  auto *ast = parser.parse();

  // (1+2)*(3+4) = 3*7 = 21
  CHECK(eval(ast) == 21, "((1+2)*(3+4)) should equal 21");

  freeNode(ast);
  std::cout << "  PASS nestedParentheses\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testSimpleAddition();
  failures += testPrecedence();
  failures += testParentheses();
  failures += testAssociativity();
  failures += testComplexExpression();
  failures += testNestedParentheses();

  if (failures == 0)
    std::cout << "All TestPratt tests passed!\n";
  else
    std::cerr << failures << " TestPratt test(s) failed!\n";

  return failures;
}
