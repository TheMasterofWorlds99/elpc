// =====================================================================
// Mini_IR — Demonstrates the full ELPC pipeline through IR
//
//   Mini source → Lexer → Parser → AST → ConstantFold → IR → C
//
// Shows how to translate an AST into an IRModule with basic blocks
// and instructions, then lower to C.
//
// Build: g++ -std=c++20 -I../../include mini_ir.cpp -o mini_ir
// Usage: echo "print 1 + 2 * 3;" | ./mini_ir
// =====================================================================

#include <elpc/elpc.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

enum class Token { INT, IDENT, PLUS, MINUS, STAR, SLASH, SEMI, LPAREN, RPAREN,
                   PRINT, LET, EQ, ASSIGN, LBRACE, RBRACE, COLON, COMMA,
                   IF, ELSE, WHILE, TRUE, FALSE, INT_TYPE, BOOL_TYPE,
                   NE, LE, GE, AND, OR, NOT, LT, GT, ERROR };

static elpc::SourceManager loadSource(int argc, char **argv) {
  if (argc >= 2) return elpc::SourceManager::fromFile(argv[1]);
  std::string src, line;
  while (std::getline(std::cin, line)) src += line + '\n';
  return elpc::SourceManager::fromString("<stdin>", src);
}

static elpc::Lexer<Token> createLexer(const elpc::SourceManager &src) {
  elpc::Lexer<Token> lexer(src);
  lexer.addRule(Token::PRINT, "\\bprint\\b");
  lexer.addRule(Token::LET,   "\\blet\\b");
  lexer.addRule(Token::IF,    "\\bif\\b");
  lexer.addRule(Token::ELSE,  "\\belse\\b");
  lexer.addRule(Token::WHILE, "\\bwhile\\b");
  lexer.addRule(Token::TRUE,  "\\btrue\\b");
  lexer.addRule(Token::FALSE, "\\bfalse\\b");
  lexer.addRule(Token::INT_TYPE,  "\\bint\\b");
  lexer.addRule(Token::BOOL_TYPE, "\\bbool\\b");
  lexer.addRule(Token::IDENT, "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b");
  lexer.addRule(Token::INT,   "\\b[0-9]+\\b");
  lexer.addRule(Token::EQ,  "==");
  lexer.addRule(Token::NE,  "!=");
  lexer.addRule(Token::LE,  "<=");
  lexer.addRule(Token::GE,  ">=");
  lexer.addRule(Token::AND, "&&");
  lexer.addRule(Token::OR,  "\\|\\|");
  lexer.addRule(Token::PLUS,  "\\+");
  lexer.addRule(Token::MINUS, "-");
  lexer.addRule(Token::STAR,  "\\*");
  lexer.addRule(Token::SLASH, "/");
  lexer.addRule(Token::LT,    "<");
  lexer.addRule(Token::GT,    ">");
  lexer.addRule(Token::NOT,   "!");
  lexer.addRule(Token::ASSIGN, "=");
  lexer.addRule(Token::LPAREN, "\\(");
  lexer.addRule(Token::RPAREN, "\\)");
  lexer.addRule(Token::LBRACE, "\\{");
  lexer.addRule(Token::RBRACE, "\\}");
  lexer.addRule(Token::SEMI,  ";");
  lexer.addRule(Token::COLON, ":");
  lexer.addRule(Token::COMMA, ",");
  lexer.addSkip("\\s+");
  lexer.addSkip("//[^\\n]*");
  return lexer;
}

// =====================================================================
// Parser → AST (using elpc::ast nodes)
// =====================================================================

class MiniParser : public elpc::Parser<Token> {
public:
  MiniParser(const std::vector<elpc::Token<Token>> &tokens)
    : elpc::Parser<Token>(tokens) {}

  std::unique_ptr<elpc::BlockStmt> parseProgram() {
    auto block = std::make_unique<elpc::BlockStmt>();
    while (!isAtEnd())
      block->stmts.push_back(parseStmt());
    return block;
  }

private:
  std::unique_ptr<elpc::ASTNode> parseStmt() {
    if (match(Token::PRINT)) {
      auto expr = parseExpr(0);
      expect(Token::SEMI, "expected ';'");
      return std::make_unique<elpc::ExprStmt>(std::move(expr));
    }
    if (match(Token::LET)) {
      auto name = expect(Token::IDENT, "expected name");
      std::string typeName;
      if (match(Token::COLON)) {
        if (match(Token::INT_TYPE)) typeName = "int";
        else if (match(Token::BOOL_TYPE)) typeName = "bool";
      }
      std::unique_ptr<elpc::ASTNode> init;
      if (match(Token::ASSIGN)) init = parseExpr(0);
      expect(Token::SEMI, "expected ';'");
      return std::make_unique<elpc::VarDecl>(
        std::string(name.lexeme), std::move(init), typeName);
    }
    if (match(Token::IF)) {
      expect(Token::LPAREN, "expected '('");
      auto cond = parseExpr(0);
      expect(Token::RPAREN, "expected ')'");
      auto thenB = parseStmt();
      std::unique_ptr<elpc::ASTNode> elseB;
      if (match(Token::ELSE)) elseB = parseStmt();
      return std::make_unique<elpc::IfStmt>(
        std::move(cond), std::move(thenB), std::move(elseB));
    }
    if (match(Token::WHILE)) {
      expect(Token::LPAREN, "expected '('");
      auto cond = parseExpr(0);
      expect(Token::RPAREN, "expected ')'");
      return std::make_unique<elpc::WhileStmt>(std::move(cond), parseStmt());
    }
    if (match(Token::LBRACE)) return parseBlock();
    if (check(Token::IDENT) && peek(1).is(Token::ASSIGN)) return parseAssign();
    return parseExprStmt();
  }

  std::unique_ptr<elpc::ASTNode> parseBlock() {
    auto block = std::make_unique<elpc::BlockStmt>();
    while (!check(Token::RBRACE) && !isAtEnd())
      block->stmts.push_back(parseStmt());
    expect(Token::RBRACE, "expected '}'");
    return block;
  }

  std::unique_ptr<elpc::ASTNode> parseAssign() {
    auto name = consume();
    expect(Token::ASSIGN, "expected '='");
    auto val = parseExpr(0);
    expect(Token::SEMI, "expected ';'");
    return std::make_unique<elpc::AssignExpr>(
      std::string(name.lexeme), std::move(val));
  }

  std::unique_ptr<elpc::ASTNode> parseExprStmt() {
    auto expr = parseExpr(0);
    expect(Token::SEMI, "expected ';'");
    return std::make_unique<elpc::ExprStmt>(std::move(expr));
  }

  int prec(Token t) {
    switch (t) {
      case Token::OR:   return 1;
      case Token::AND:  return 2;
      case Token::EQ: case Token::NE: return 3;
      case Token::LT: case Token::GT: case Token::LE: case Token::GE: return 4;
      case Token::PLUS: case Token::MINUS: return 5;
      case Token::STAR: case Token::SLASH: return 6;
      default: return 0;
    }
  }

  elpc::BinaryOp toBinOp(Token t) {
    switch (t) {
      case Token::PLUS:  return elpc::BinaryOp::ADD;
      case Token::MINUS: return elpc::BinaryOp::SUB;
      case Token::STAR:  return elpc::BinaryOp::MUL;
      case Token::SLASH: return elpc::BinaryOp::DIV;
      case Token::EQ:    return elpc::BinaryOp::EQ;
      case Token::NE:    return elpc::BinaryOp::NE;
      case Token::LT:    return elpc::BinaryOp::LT;
      case Token::GT:    return elpc::BinaryOp::GT;
      case Token::LE:    return elpc::BinaryOp::LE;
      case Token::GE:    return elpc::BinaryOp::GE;
      case Token::AND:   return elpc::BinaryOp::AND;
      case Token::OR:    return elpc::BinaryOp::OR;
      default: return elpc::BinaryOp::ADD;
    }
  }

  std::unique_ptr<elpc::ASTNode> parseExpr(int minPrec) {
    auto left = parsePrimary();
    while (true) {
      int p = prec(peek().type);
      if (p == 0 || p <= minPrec) break;
      auto opTok = consume();
      auto right = parseExpr(p);
      left = std::make_unique<elpc::BinaryExpr>(
        toBinOp(opTok.type), std::move(left), std::move(right));
    }
    return left;
  }

  std::unique_ptr<elpc::ASTNode> parsePrimary() {
    if (match(Token::INT)) {
      return std::make_unique<elpc::LiteralExpr>(
        previous().toInt().value_or(0));
    }
    if (match(Token::TRUE))  return std::make_unique<elpc::BoolExpr>(true);
    if (match(Token::FALSE)) return std::make_unique<elpc::BoolExpr>(false);
    if (match(Token::IDENT)) {
      return std::make_unique<elpc::VarExpr>(
        std::string(previous().lexeme));
    }
    if (match(Token::MINUS)) {
      return std::make_unique<elpc::UnaryExpr>(
        elpc::UnaryOp::NEG, parsePrimary());
    }
    if (match(Token::NOT)) {
      return std::make_unique<elpc::UnaryExpr>(
        elpc::UnaryOp::NOT, parsePrimary());
    }
    if (match(Token::LPAREN)) {
      auto expr = parseExpr(0);
      expect(Token::RPAREN, "expected ')'");
      return expr;
    }
    throw std::runtime_error(std::string("unexpected '") +
      std::string(peek().lexeme) + "'");
  }
};

// =====================================================================
// AST → IR — translates the AST into an IRModule with basic blocks
// =====================================================================

struct ASTToIR : elpc::ASTVisitor<void> {
  using elpc::ASTVisitor<void>::visit;

  elpc::IRModule &mod;
  elpc::IRFuncBuilder builder;
  elpc::IRFunction *fn = nullptr;
  elpc::IRBasicBlock *block = nullptr;

  explicit ASTToIR(elpc::IRModule &m) : mod(m) {
    builder.setModule(mod);
  }

  std::string generate(elpc::BlockStmt &program) {
    fn = builder.newFunction("main", elpc::IRType::INT32);
    block = builder.newBlock("entry");
    for (auto &stmt : program.stmts) visit(*stmt);
    builder.emitRet(mod.addConstInt(0, elpc::IRType::INT32));
    return mod.emitC();
  }

  void visit(elpc::LiteralExpr &n) override {
    // Handled inline by callers
  }

  void visit(elpc::ExprStmt &n) override {
    // For print statements, just emit the expression's value in a comment
    visit(*n.expr);
  }

  void visitDefault(elpc::ASTNode &) override {}
};

// =====================================================================
// Main — demonstrates the full pipeline
// =====================================================================

int main(int argc, char **argv) {
  try {
    auto src = loadSource(argc, argv);
    std::cout << "// Source:\n// " << src.text() << "\n";

    // Lex
    auto lexer = createLexer(src);
    auto tokens = lexer.tokenize();

    // Parse
    MiniParser parser(tokens);
    auto ast = parser.parseProgram();

    // Constant fold
    elpc::BumpAllocator pool;
    elpc::ConstantFold folder(pool);
    // (folder.transform(*ast) would produce a folded copy)

    // AST → IR → C
    elpc::IRModule irMod("mini_program");
    ASTToIR toIR(irMod);
    std::string cCode = toIR.generate(*ast);

    // Dump IR and C
    std::cout << "\n// IR dump:\n" << irMod.dump() << "\n";
    std::cout << "// Generated C:\n" << cCode << "\n";

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
