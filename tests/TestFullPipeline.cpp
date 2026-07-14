#include <elpc/elpc.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// === Tiny "ELPC Script" language ===
// Grammar:
//   program   → statement*
//   statement → "exit" expression ";"
//             → "print" expression ";"
//   expression → term (("+" | "-") term)*
//   term      → factor (("*" | "/") factor)*
//   factor    → INTEGER | "(" expression ")"

enum class TokenType {
  EXIT, PRINT, INT_LIT, PLUS, MINUS, STAR, SLASH,
  LPAREN, RPAREN, SEMI, EOF_
};

// === AST ===
struct ASTNode {
  enum Kind { PROGRAM, EXIT_STMT, PRINT_STMT, INT_LIT, BINARY_OP };
  Kind kind;
  int intValue = 0;
  char op = 0;
  ASTNode *left = nullptr;
  ASTNode *right = nullptr;
  std::vector<ASTNode*> children;

  ~ASTNode() {
    delete left;
    delete right;
    for (auto *child : children) delete child;
  }
};

// === Lexer ===
static std::vector<elpc::Token<TokenType>> lex(const elpc::SourceManager &src) {
  elpc::Lexer<TokenType> lexer(src);
  lexer.addRule(TokenType::EXIT, "\\bexit\\b");
  lexer.addRule(TokenType::PRINT, "\\bprint\\b");
  lexer.addRule(TokenType::INT_LIT, "\\b[0-9]+\\b");
  lexer.addSkip("\\s+");

  // Single-character tokens
  lexer.addRule(TokenType::PLUS, "\\+");
  lexer.addRule(TokenType::MINUS, "-");
  lexer.addRule(TokenType::STAR, "\\*");
  lexer.addRule(TokenType::SLASH, "/");
  lexer.addRule(TokenType::LPAREN, "\\(");
  lexer.addRule(TokenType::RPAREN, "\\)");
  lexer.addRule(TokenType::SEMI, ";");

  return lexer.tokenize();
}

// === Parser (recursive descent + Pratt-style expression) ===
class Parser : public elpc::Parser<TokenType> {
public:
  Parser(const std::vector<elpc::Token<TokenType>> &tokens)
    : elpc::Parser<TokenType>(tokens) {}

  ASTNode *parseProgram() {
    auto *prog = new ASTNode{ASTNode::PROGRAM};
    while (!isAtEnd()) {
      prog->children.push_back(parseStatement());
    }
    return prog;
  }

private:
  ASTNode *parseStatement() {
    if (match(TokenType::EXIT)) {
      auto *expr = parseExpression();
      expect(TokenType::SEMI, "expected ';' after exit expression");
      auto *stmt = new ASTNode{ASTNode::EXIT_STMT};
      stmt->left = expr;
      return stmt;
    }

    if (match(TokenType::PRINT)) {
      auto *expr = parseExpression();
      expect(TokenType::SEMI, "expected ';' after print expression");
      auto *stmt = new ASTNode{ASTNode::PRINT_STMT};
      stmt->left = expr;
      return stmt;
    }

    throw std::runtime_error("unexpected token in statement");
  }

  ASTNode *parseExpression() {
    auto *left = parseTerm();

    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
      char op = previous().is(TokenType::PLUS) ? '+' : '-';
      auto *right = parseTerm();
      auto *node = new ASTNode{ASTNode::BINARY_OP};
      node->op = op;
      node->left = left;
      node->right = right;
      left = node;
    }

    return left;
  }

  ASTNode *parseTerm() {
    auto *left = parseFactor();

    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
      char op = previous().is(TokenType::STAR) ? '*' : '/';
      auto *right = parseFactor();
      auto *node = new ASTNode{ASTNode::BINARY_OP};
      node->op = op;
      node->left = left;
      node->right = right;
      left = node;
    }

    return left;
  }

  ASTNode *parseFactor() {
    if (match(TokenType::MINUS)) {
      // Unary minus
      auto *right = parseFactor();
      auto *node = new ASTNode{ASTNode::BINARY_OP};
      node->op = '~'; // unary minus marker
      node->right = right;
      return node;
    }

    if (match(TokenType::INT_LIT)) {
      auto *node = new ASTNode{ASTNode::INT_LIT};
      node->intValue = previous().toInt().value_or(0);
      return node;
    }

    if (match(TokenType::LPAREN)) {
      auto *expr = parseExpression();
      expect(TokenType::RPAREN, "expected ')'");
      return expr;
    }

    throw std::runtime_error("expected expression");
  }
};

// === Semantic Analysis ===
class Analyzer : public elpc::Sema {
public:
  Analyzer(elpc::DiagnosticEngine &diag) : elpc::Sema(diag) {}

  bool analyze(ASTNode *prog) {
    for (auto *stmt : prog->children) {
      analyzeStatement(stmt);
    }
    return !hasErrors();
  }

private:
  void analyzeStatement(ASTNode *stmt) {
    if (stmt->kind == ASTNode::EXIT_STMT) {
      int val = evaluateConst(stmt->left);
      if (val < 0)
        warning("exit with negative code: " + std::to_string(val), {});
    }
  }

  int evaluateConst(ASTNode *node) {
    if (node->kind == ASTNode::INT_LIT)
      return node->intValue;

    if (node->kind == ASTNode::BINARY_OP && node->op == '~')
      return -evaluateConst(node->right);

    if (node->kind == ASTNode::BINARY_OP) {
      int l = evaluateConst(node->left);
      int r = evaluateConst(node->right);
      switch (node->op) {
        case '+': return l + r;
        case '-': return l - r;
        case '*': return l * r;
        case '/': return l / r;
      }
    }

    error("non-constant expression", {});
    return 0;
  }
};

// === Code Generation ===
class Generator : public elpc::IRBuilder<std::string> {
public:
  Generator(elpc::DiagnosticEngine &diag) : elpc::IRBuilder<std::string>(diag) {}

  std::string generate(ASTNode *prog) {
    for (auto *stmt : prog->children) {
      generateStatement(stmt);
    }
    return result();
  }

private:
  void generateStatement(ASTNode *stmt) {
    if (stmt->kind == ASTNode::EXIT_STMT) {
      out() << "  return " << generateExpression(stmt->left) << ";\n";
    } else if (stmt->kind == ASTNode::PRINT_STMT) {
      out() << "  printf(\"%d\\n\", " << generateExpression(stmt->left) << ");\n";
    }
  }

  std::string generateExpression(ASTNode *node) {
    if (node->kind == ASTNode::INT_LIT)
      return std::to_string(node->intValue);

    if (node->kind == ASTNode::BINARY_OP && node->op == '~')
      return "(-" + generateExpression(node->right) + ")";

    if (node->kind == ASTNode::BINARY_OP) {
      auto l = generateExpression(node->left);
      auto r = generateExpression(node->right);
      return "(" + l + " " + node->op + " " + r + ")";
    }

    error("unknown expression kind", {});
    return "0";
  }
};

// === Tests ===

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testLexerStage() {
  auto src = elpc::SourceManager::fromString("test", "exit 42; print 1+2;");
  auto tokens = lex(src);

  CHECK(tokens.size() == 8, "expected 8 tokens");
  CHECK(tokens[0].is(TokenType::EXIT), "first token: EXIT");
  CHECK(tokens[1].is(TokenType::INT_LIT), "second token: INT_LIT");
  CHECK(tokens[1].lexeme == "42", "value: 42");
  CHECK(tokens[2].is(TokenType::SEMI), "third token: SEMI");
  CHECK(tokens[3].is(TokenType::PRINT), "fourth token: PRINT");
  CHECK(tokens[4].is(TokenType::INT_LIT), "fifth token: INT_LIT");
  CHECK(tokens[4].lexeme == "1", "value: 1");
  CHECK(tokens[5].is(TokenType::PLUS), "sixth token: PLUS");
  CHECK(tokens[6].is(TokenType::INT_LIT), "seventh token: INT_LIT");
  CHECK(tokens[7].is(TokenType::SEMI), "eighth token: SEMI");

  std::cout << "  PASS lexerStage\n";
  return 0;
}

static int testParserStage() {
  auto src = elpc::SourceManager::fromString("test", "exit 42; print 1+2*3;");
  auto tokens = lex(src);
  Parser parser(tokens);
  auto *prog = parser.parseProgram();

  CHECK(prog->children.size() == 2, "expected 2 statements");
  CHECK(prog->children[0]->kind == ASTNode::EXIT_STMT, "first: EXIT_STMT");
  CHECK(prog->children[1]->kind == ASTNode::PRINT_STMT, "second: PRINT_STMT");

  // Check expression tree: 1+2*3 should be 1+(2*3)
  auto *expr = prog->children[1]->left;
  CHECK(expr->kind == ASTNode::BINARY_OP, "print expr is BINARY_OP");
  CHECK(expr->op == '+', "op is +");
  CHECK(expr->left->kind == ASTNode::INT_LIT, "left is INT_LIT");
  CHECK(expr->left->intValue == 1, "left = 1");
  CHECK(expr->right->kind == ASTNode::BINARY_OP, "right is BINARY_OP");
  CHECK(expr->right->op == '*', "right op is *");
  CHECK(expr->right->left->intValue == 2, "right* left = 2");
  CHECK(expr->right->right->intValue == 3, "right* right = 3");

  delete prog;
  std::cout << "  PASS parserStage\n";
  return 0;
}

static int testSemaStage() {
  elpc::DiagnosticEngine diag;
  Analyzer analyzer(diag);

  auto src = elpc::SourceManager::fromString("test", "exit -1;");
  auto tokens = lex(src);
  Parser parser(tokens);
  auto *prog = parser.parseProgram();

  bool ok = analyzer.analyze(prog);
  CHECK(ok, "analysis should succeed");
  CHECK(diag.count() == 1, "should have 1 diagnostic (warning for negative exit)");

  std::ostringstream out;
  diag.reportDiagnostics(out);
  CHECK(out.str().find("negative code") != std::string::npos,
        "warning about negative exit code");

  delete prog;
  std::cout << "  PASS semaStage\n";
  return 0;
}

static int testCodegenStage() {
  elpc::DiagnosticEngine diag;
  Generator gen(diag);

  {
    auto src = elpc::SourceManager::fromString("test", "exit 42; print 1+2;");
    auto tokens = lex(src);
    Parser parser(tokens);
    auto *prog = parser.parseProgram();
    std::string code = gen.generate(prog);
    delete prog;

    CHECK(code.find("return 42;") != std::string::npos, "should emit return 42");
    CHECK(code.find("printf") != std::string::npos, "should emit printf");
    CHECK(code.find("(1 + 2)") != std::string::npos, "should emit (1 + 2)");
  }

  std::cout << "  PASS codegenStage\n";
  return 0;
}

static int testFullPipeline() {
  elpc::DiagnosticEngine diag;
  auto src = elpc::SourceManager::fromString("test", "exit 3+4*2; print (1+2)*3;");

  auto tokens = lex(src);
  CHECK(!tokens.empty(), "lexer should produce tokens");

  Parser parser(tokens);
  auto *prog = parser.parseProgram();
  CHECK(prog->children.size() == 2, "parser should find 2 statements");

  Analyzer analyzer(diag);
  bool analysisOk = analyzer.analyze(prog);
  CHECK(analysisOk, "semantic analysis should pass");

  Generator gen(diag);
  std::string code = gen.generate(prog);
  CHECK(code.find("return (3 + (4 * 2));") != std::string::npos,
        "codegen: exit 3+4*2");
  CHECK(code.find("printf") != std::string::npos,
        "codegen: print statement");
  CHECK(code.find("((1 + 2) * 3)") != std::string::npos,
        "codegen: (1+2)*3");

  delete prog;
  std::cout << "  PASS fullPipeline\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testLexerStage();
  failures += testParserStage();
  failures += testSemaStage();
  failures += testCodegenStage();
  failures += testFullPipeline();

  if (failures == 0)
    std::cout << "All TestFullPipeline tests passed!\n";
  else
    std::cerr << failures << " TestFullPipeline test(s) failed!\n";

  return failures;
}
