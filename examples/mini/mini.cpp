// =====================================================================
// Mini — A complete example language built with ELPC
//
// Demonstrates the full pipeline:
//   Source → Lexer → Parser → Sema → Codegen → C output
//
// Features: integers, booleans, variables, if/else, while, print, blocks
//
// Build:  g++ -std=c++20 -I../../include mini.cpp -o mini
// Usage:  echo "let x = 42; print x;" | ./mini
//         ./mini programs/hello.mini
// =====================================================================

#include <elpc/elpc.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// =====================================================================
// 1. Token types
// =====================================================================

enum class TokenType {
  // Literals
  INT_LIT,
  BOOL_LIT,
  IDENT,
  // Keywords
  LET,
  IF,
  ELSE,
  WHILE,
  PRINT,
  TRUE,
  FALSE,
  INT_TYPE,
  BOOL_TYPE,
  // Operators
  PLUS,
  MINUS,
  STAR,
  SLASH,
  EQ,
  NE,
  LT,
  GT,
  LE,
  GE,
  AND,
  OR,
  NOT,
  ASSIGN,
  // Delimiters
  LPAREN,
  RPAREN,
  LBRACE,
  RBRACE,
  SEMI,
  COLON,
  COMMA,
  // Special
  ERROR,
  EOF_
};

// =====================================================================
// 2. Helper: build a SourceManager from CLI args
// =====================================================================

static elpc::SourceManager loadSource(int argc, char** argv) {
  if (argc >= 2) {
    return elpc::SourceManager::fromFile(argv[1]);
  }
  // Read stdin
  std::string src, line;
  while (std::getline(std::cin, line)) {
    src += line + '\n';
  }
  return elpc::SourceManager::fromString("<stdin>", src);
}

// =====================================================================
// 3. Lexer
// =====================================================================

static elpc::Lexer<TokenType> createLexer(const elpc::SourceManager& src) {
  elpc::Lexer<TokenType> lexer(src);

  // Keywords (order matters: keywords before IDENT for priority)
  lexer.addRule(TokenType::LET, "\\blet\\b");
  lexer.addRule(TokenType::IF, "\\bif\\b");
  lexer.addRule(TokenType::ELSE, "\\belse\\b");
  lexer.addRule(TokenType::WHILE, "\\bwhile\\b");
  lexer.addRule(TokenType::PRINT, "\\bprint\\b");
  lexer.addRule(TokenType::TRUE, "\\btrue\\b");
  lexer.addRule(TokenType::FALSE, "\\bfalse\\b");
  lexer.addRule(TokenType::INT_TYPE, "\\bint\\b");
  lexer.addRule(TokenType::BOOL_TYPE, "\\bbool\\b");

  // Identifiers
  lexer.addRule(TokenType::IDENT, "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b");

  // Integer literals
  lexer.addRule(TokenType::INT_LIT, "\\b[0-9]+\\b");

  // Two-character operators (check before single-char)
  lexer.addRule(TokenType::EQ, "==");
  lexer.addRule(TokenType::NE, "!=");
  lexer.addRule(TokenType::LE, "<=");
  lexer.addRule(TokenType::GE, ">=");
  lexer.addRule(TokenType::AND, "&&");
  lexer.addRule(TokenType::OR, "\\|\\|");

  // Single-character operators
  lexer.addRule(TokenType::PLUS, "\\+");
  lexer.addRule(TokenType::MINUS, "-");
  lexer.addRule(TokenType::STAR, "\\*");
  lexer.addRule(TokenType::SLASH, "/");
  lexer.addRule(TokenType::LT, "<");
  lexer.addRule(TokenType::GT, ">");
  lexer.addRule(TokenType::NOT, "!");
  lexer.addRule(TokenType::ASSIGN, "=");

  // Delimiters
  lexer.addRule(TokenType::LPAREN, "\\(");
  lexer.addRule(TokenType::RPAREN, "\\)");
  lexer.addRule(TokenType::LBRACE, "\\{");
  lexer.addRule(TokenType::RBRACE, "\\}");
  lexer.addRule(TokenType::SEMI, ";");
  lexer.addRule(TokenType::COLON, ":");
  lexer.addRule(TokenType::COMMA, ",");

  // Skip whitespace and comments
  lexer.addSkip("\\s+");
  lexer.addSkip("//[^\\n]*");                       // line comments
  lexer.addSkip("/\\*[^*]*\\*+([^/*][^*]*\\*+)*/"); // block comments

  return lexer;
}

// =====================================================================
// 4. Parser
//    Builds an AST using elpc::ast nodes
// =====================================================================

class MiniParser : public elpc::Parser<TokenType> {
public:
  MiniParser(const std::vector<elpc::Token<TokenType>>& tokens) : elpc::Parser<TokenType>(tokens) {}

  // program → declaration*
  std::unique_ptr<elpc::BlockStmt> parseProgram() {
    auto block = std::make_unique<elpc::BlockStmt>();
    while (!isAtEnd()) {
      block->stmts.push_back(parseDeclaration());
    }
    return block;
  }

private:
  std::unique_ptr<elpc::ASTNode> parseDeclaration() {
    if (match(TokenType::LET))
      return parseVarDecl();
    return parseStatement();
  }

  // varDecl → "let" IDENT (":" TYPE)? ("=" expression)? ";"
  std::unique_ptr<elpc::ASTNode> parseVarDecl() {
    auto tok = expect(TokenType::IDENT, "expected variable name");
    auto decl = std::make_unique<elpc::VarDecl>(std::string(tok.lexeme));

    if (match(TokenType::COLON)) {
      if (match(TokenType::INT_TYPE))
        decl->typeName = "int";
      else if (match(TokenType::BOOL_TYPE))
        decl->typeName = "bool";
      else
        expectError("expected type (int or bool)");
    }

    if (match(TokenType::ASSIGN)) {
      decl->initializer = parseExpression();
    }

    expect(TokenType::SEMI, "expected ';' after variable declaration");
    return decl;
  }

  std::unique_ptr<elpc::ASTNode> parseStatement() {
    if (match(TokenType::PRINT))
      return parsePrintStmt();
    if (match(TokenType::IF))
      return parseIfStmt();
    if (match(TokenType::WHILE))
      return parseWhileStmt();
    if (match(TokenType::LBRACE))
      return parseBlock();
    if (check(TokenType::IDENT) && peekNextIsAssign()) {
      return parseAssignStmt();
    }
    return parseExprStmt();
  }

  bool peekNextIsAssign() {
    if (isAtEnd())
      return false;
    // peek(1) checks the token after current position
    // If at end+1, tokens.back() is returned (safe)
    return peek(1).is(TokenType::ASSIGN);
  }

  // printStmt → "print" expression ";"
  std::unique_ptr<elpc::ASTNode> parsePrintStmt() {
    auto printTok = previous();
    auto expr = parseExpression();
    expect(TokenType::SEMI, "expected ';' after print");
    // Wrap the expression in an ExprStmt — the codegen handles print specially
    // Actually, let's mark it with a node type. We'll use ExprStmt as a generic wrapper.
    // Better: we don't have a PrintStmt in elpc::ast. Let's handle it in codegen by
    // looking at the parent context. For now, wrap in ExprStmt with the print token.
    auto exprStmt = std::make_unique<elpc::ExprStmt>(std::move(expr));
    // Store the fact this is a print in the location (hack for simplicity)
    exprStmt->location = printTok.location;
    return exprStmt;
  }

  // ifStmt → "if" "(" expression ")" statement ("else" statement)?
  std::unique_ptr<elpc::ASTNode> parseIfStmt() {
    expect(TokenType::LPAREN, "expected '(' after if");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "expected ')' after if condition");
    auto thenBranch = parseStatement();
    std::unique_ptr<elpc::ASTNode> elseBranch;
    if (match(TokenType::ELSE)) {
      elseBranch = parseStatement();
    }
    return std::make_unique<elpc::IfStmt>(std::move(cond), std::move(thenBranch),
                                          std::move(elseBranch));
  }

  // whileStmt → "while" "(" expression ")" statement
  std::unique_ptr<elpc::ASTNode> parseWhileStmt() {
    expect(TokenType::LPAREN, "expected '(' after while");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "expected ')' after while condition");
    auto body = parseStatement();
    return std::make_unique<elpc::WhileStmt>(std::move(cond), std::move(body));
  }

  // block → "{" declaration* "}"
  std::unique_ptr<elpc::ASTNode> parseBlock() {
    auto block = std::make_unique<elpc::BlockStmt>();
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
      block->stmts.push_back(parseDeclaration());
    }
    expect(TokenType::RBRACE, "expected '}' to close block");
    return block;
  }

  // assignStmt → IDENT "=" expression ";"
  std::unique_ptr<elpc::ASTNode> parseAssignStmt() {
    auto tok = consume(); // consume IDENT
    expect(TokenType::ASSIGN, "expected '=' in assignment");
    auto value = parseExpression();
    expect(TokenType::SEMI, "expected ';' after assignment");
    return std::make_unique<elpc::AssignExpr>(std::string(tok.lexeme), std::move(value));
  }

  // exprStmt → expression ";"
  std::unique_ptr<elpc::ASTNode> parseExprStmt() {
    auto expr = parseExpression();
    expect(TokenType::SEMI, "expected ';' after expression");
    return std::make_unique<elpc::ExprStmt>(std::move(expr));
  }

  // --- Expression parsing (Pratt-style) ---

  int getPrecedence(TokenType type) {
    switch (type) {
    case TokenType::OR:
      return 1;
    case TokenType::AND:
      return 2;
    case TokenType::EQ:
    case TokenType::NE:
      return 3;
    case TokenType::LT:
    case TokenType::GT:
    case TokenType::LE:
    case TokenType::GE:
      return 4;
    case TokenType::PLUS:
    case TokenType::MINUS:
      return 5;
    case TokenType::STAR:
    case TokenType::SLASH:
      return 6;
    default:
      return 0;
    }
  }

  elpc::BinaryOp toBinaryOp(TokenType type) {
    switch (type) {
    case TokenType::PLUS:
      return elpc::BinaryOp::ADD;
    case TokenType::MINUS:
      return elpc::BinaryOp::SUB;
    case TokenType::STAR:
      return elpc::BinaryOp::MUL;
    case TokenType::SLASH:
      return elpc::BinaryOp::DIV;
    case TokenType::EQ:
      return elpc::BinaryOp::EQ;
    case TokenType::NE:
      return elpc::BinaryOp::NE;
    case TokenType::LT:
      return elpc::BinaryOp::LT;
    case TokenType::GT:
      return elpc::BinaryOp::GT;
    case TokenType::LE:
      return elpc::BinaryOp::LE;
    case TokenType::GE:
      return elpc::BinaryOp::GE;
    case TokenType::AND:
      return elpc::BinaryOp::AND;
    case TokenType::OR:
      return elpc::BinaryOp::OR;
    default:
      return elpc::BinaryOp::ADD;
    }
  }

  std::unique_ptr<elpc::ASTNode> parseExpression(int minPrec = 0) {
    auto left = parsePrimary();

    while (true) {
      auto tokType = peek().type;
      int prec = getPrecedence(tokType);
      if (prec == 0 || prec <= minPrec)
        break;

      consume(); // consume operator
      auto right = parseExpression(prec);
      left = std::make_unique<elpc::BinaryExpr>(toBinaryOp(tokType), std::move(left),
                                                std::move(right));
    }

    return left;
  }

  std::unique_ptr<elpc::ASTNode> parsePrimary() {
    if (match(TokenType::INT_LIT)) {
      auto val = previous().toInt().value_or(0);
      return std::make_unique<elpc::LiteralExpr>(val, previous().location);
    }
    if (match(TokenType::TRUE)) {
      return std::make_unique<elpc::BoolExpr>(true, previous().location);
    }
    if (match(TokenType::FALSE)) {
      return std::make_unique<elpc::BoolExpr>(false, previous().location);
    }
    if (match(TokenType::IDENT)) {
      return std::make_unique<elpc::VarExpr>(std::string(previous().lexeme), previous().location);
    }
    if (match(TokenType::MINUS)) {
      // Unary minus
      auto operand = parsePrimary();
      return std::make_unique<elpc::UnaryExpr>(elpc::UnaryOp::NEG, std::move(operand));
    }
    if (match(TokenType::NOT)) {
      auto operand = parsePrimary();
      return std::make_unique<elpc::UnaryExpr>(elpc::UnaryOp::NOT, std::move(operand));
    }
    if (match(TokenType::LPAREN)) {
      auto expr = parseExpression();
      expect(TokenType::RPAREN, "expected ')' after expression");
      return expr;
    }

    auto t = peek();
    throw std::runtime_error(std::string("unexpected token '") + std::string(t.lexeme) +
                             "' at line " + std::to_string(t.location.line));
  }

  void expectError(const std::string& msg) {
    auto t = peek();
    throw std::runtime_error("[mini] " + msg + " at line " + std::to_string(t.location.line));
  }
};

// =====================================================================
// 5. Semantic analysis
// =====================================================================

struct MiniSema {
  elpc::DiagnosticEngine& diag;
  elpc::SymbolTable<std::string, std::string> symbols; // name → type

  MiniSema(elpc::DiagnosticEngine& diag) : diag(diag) {}

  bool analyze(elpc::BlockStmt& block) {
    analyzeBlock(block);
    return !diag.hasErrors();
  }

  void analyzeBlock(elpc::BlockStmt& block) {
    symbols.pushScope();
    for (auto& stmt : block.stmts) {
      analyzeNode(*stmt);
    }
    symbols.popScope();
  }

  void analyzeNode(elpc::ASTNode& node) {
    switch (node.nodeKind()) {
    case elpc::NodeKind::VAR_DECL:
      analyzeVarDecl(static_cast<elpc::VarDecl&>(node));
      break;
    case elpc::NodeKind::ASSIGN:
      analyzeAssign(static_cast<elpc::AssignExpr&>(node));
      break;
    case elpc::NodeKind::EXPR_STMT:
      analyzeExpr(*static_cast<elpc::ExprStmt&>(node).expr);
      break;
    case elpc::NodeKind::IF:
      analyzeIf(static_cast<elpc::IfStmt&>(node));
      break;
    case elpc::NodeKind::WHILE:
      analyzeWhile(static_cast<elpc::WhileStmt&>(node));
      break;
    case elpc::NodeKind::BLOCK:
      analyzeBlock(static_cast<elpc::BlockStmt&>(node));
      break;
    default:
      analyzeExpr(node);
      break;
    }
  }

  void analyzeVarDecl(elpc::VarDecl& decl) {
    std::string declaredType = decl.typeName;
    std::string initType;

    if (decl.initializer) {
      initType = analyzeExpr(*decl.initializer);
    }

    if (!declaredType.empty() && !initType.empty() && declaredType != initType) {
      diag.error("type mismatch: declared '" + declaredType + "' but initializer is '" + initType +
                     "'",
                 decl.location);
    }

    std::string finalType = declaredType.empty() ? initType : declaredType;
    if (finalType.empty())
      finalType = "void";

    if (!symbols.define(decl.name, finalType)) {
      diag.error("duplicate variable '" + decl.name + "'", decl.location);
    }
  }

  void analyzeAssign(elpc::AssignExpr& assign) {
    auto var = symbols.lookup(assign.target);
    if (!var) {
      diag.error("undefined variable '" + assign.target + "'", assign.location);
      return;
    }
    auto valType = analyzeExpr(*assign.value);
    if (valType != *var) {
      diag.error("cannot assign '" + valType + "' to '" + *var + "'", assign.location);
    }
  }

  std::string analyzeExpr(elpc::ASTNode& node) {
    switch (node.nodeKind()) {
    case elpc::NodeKind::LITERAL_INT:
      return "int";
    case elpc::NodeKind::LITERAL_BOOL:
      return "bool";
    case elpc::NodeKind::VAR: {
      auto& v = static_cast<elpc::VarExpr&>(node);
      auto type = symbols.lookup(v.name);
      if (!type) {
        diag.error("undefined variable '" + v.name + "'", node.location);
        return "error";
      }
      return *type;
    }
    case elpc::NodeKind::UNARY:
      return analyzeExpr(*static_cast<elpc::UnaryExpr&>(node).operand);
    case elpc::NodeKind::BINARY: {
      auto& b = static_cast<elpc::BinaryExpr&>(node);
      auto l = analyzeExpr(*b.left);
      auto r = analyzeExpr(*b.right);
      if (l == "error" || r == "error")
        return "error";
      // Arithmetic operators require int
      if (b.op == elpc::BinaryOp::ADD || b.op == elpc::BinaryOp::SUB ||
          b.op == elpc::BinaryOp::MUL || b.op == elpc::BinaryOp::DIV) {
        if (l != "int" || r != "int") {
          diag.error("arithmetic requires int operands", node.location);
          return "error";
        }
        return "int";
      }
      // Comparison operators return bool
      if (b.op == elpc::BinaryOp::EQ || b.op == elpc::BinaryOp::NE || b.op == elpc::BinaryOp::LT ||
          b.op == elpc::BinaryOp::GT || b.op == elpc::BinaryOp::LE || b.op == elpc::BinaryOp::GE) {
        return "bool";
      }
      // Logical operators require bool
      if (b.op == elpc::BinaryOp::AND || b.op == elpc::BinaryOp::OR) {
        if (l != "bool" || r != "bool") {
          diag.error("logical operator requires bool operands", node.location);
          return "error";
        }
        return "bool";
      }
      return "error";
    }
    default:
      return "void";
    }
  }

  void analyzeIf(elpc::IfStmt& node) {
    auto condType = analyzeExpr(*node.condition);
    if (condType != "bool") {
      diag.error("if condition must be bool", node.condition->location);
    }
    analyzeNode(*node.thenBranch);
    if (node.elseBranch)
      analyzeNode(*node.elseBranch);
  }

  void analyzeWhile(elpc::WhileStmt& node) {
    auto condType = analyzeExpr(*node.condition);
    if (condType != "bool") {
      diag.error("while condition must be bool", node.condition->location);
    }
    analyzeNode(*node.body);
  }
};

// =====================================================================
// 6. Code generation — emits C code
// =====================================================================

struct MiniCodegen : elpc::ASTVisitor<void> {
  using elpc::ASTVisitor<void>::visit;

  std::ostringstream out;
  int indent = 0;

  std::string generate(elpc::BlockStmt& block) {
    out << "#include <stdio.h>\n\n";
    out << "int main() {\n";
    indent = 1;
    for (auto& stmt : block.stmts) {
      visit(*stmt);
    }
    out << "    return 0;\n";
    out << "}\n";
    return out.str();
  }

  void emitLine(std::string s) {
    for (int i = 0; i < indent; ++i)
      out << "    ";
    out << s << "\n";
  }

  // --- Expressions ---

  void visit(elpc::LiteralExpr& n) override { out << n.intValue; }

  void visit(elpc::FloatExpr& n) override { out << n.floatValue; }

  void visit(elpc::BoolExpr& n) override { out << (n.boolValue ? "1" : "0"); }

  void visit(elpc::VarExpr& n) override { out << n.name; }

  void visit(elpc::UnaryExpr& n) override {
    if (n.op == elpc::UnaryOp::NEG)
      out << "-";
    else if (n.op == elpc::UnaryOp::NOT)
      out << "!";
    out << "(";
    visit(*n.operand);
    out << ")";
  }

  void visit(elpc::BinaryExpr& n) override {
    out << "(";
    visit(*n.left);
    switch (n.op) {
    case elpc::BinaryOp::ADD:
      out << " + ";
      break;
    case elpc::BinaryOp::SUB:
      out << " - ";
      break;
    case elpc::BinaryOp::MUL:
      out << " * ";
      break;
    case elpc::BinaryOp::DIV:
      out << " / ";
      break;
    case elpc::BinaryOp::EQ:
      out << " == ";
      break;
    case elpc::BinaryOp::NE:
      out << " != ";
      break;
    case elpc::BinaryOp::LT:
      out << " < ";
      break;
    case elpc::BinaryOp::GT:
      out << " > ";
      break;
    case elpc::BinaryOp::LE:
      out << " <= ";
      break;
    case elpc::BinaryOp::GE:
      out << " >= ";
      break;
    case elpc::BinaryOp::AND:
      out << " && ";
      break;
    case elpc::BinaryOp::OR:
      out << " || ";
      break;
    default:
      out << " ? ";
      break;
    }
    visit(*n.right);
    out << ")";
  }

  // --- Statements ---

  void visit(elpc::ExprStmt& n) override {
    if (n.location.line != 0 && n.location.column != 0) {
      // Print statements: expression at the same location as the 'print' keyword
      emitLine("printf(\"%d\\n\", " + exprToString(*n.expr) + ");");
    } else {
      emitLine(exprToString(*n.expr) + ";");
    }
  }

  void visit(elpc::BlockStmt& n) override {
    if (n.stmts.empty()) {
      emitLine("{}");
      return;
    }
    emitLine("{");
    indent++;
    for (auto& stmt : n.stmts)
      visit(*stmt);
    indent--;
    emitLine("}");
  }

  void visit(elpc::IfStmt& n) override {
    out << std::string(static_cast<size_t>(indent) * 4, ' ');
    out << "if (";
    visit(*n.condition);
    out << ") ";

    // Check if thenBranch is a block — if so, use the block visitor
    if (n.thenBranch->nodeKind() == elpc::NodeKind::BLOCK) {
      visit(*n.thenBranch);
    } else {
      out << "\n";
      indent++;
      visit(*n.thenBranch);
      indent--;
    }

    if (n.elseBranch) {
      out << std::string(static_cast<size_t>(indent) * 4, ' ') << "else ";
      if (n.elseBranch->nodeKind() == elpc::NodeKind::BLOCK) {
        visit(*n.elseBranch);
      } else {
        out << "\n";
        indent++;
        visit(*n.elseBranch);
        indent--;
      }
    }
  }

  void visit(elpc::WhileStmt& n) override {
    out << std::string(static_cast<size_t>(indent) * 4, ' ');
    out << "while (";
    visit(*n.condition);
    out << ") ";
    if (n.body->nodeKind() == elpc::NodeKind::BLOCK) {
      visit(*n.body);
    } else {
      out << "\n";
      indent++;
      visit(*n.body);
      indent--;
    }
  }

  void visit(elpc::VarDecl& n) override {
    std::string type = n.typeName.empty() ? "int" : n.typeName;
    if (type == "bool")
      type = "int"; // C doesn't have bool without stdbool.h
    out << std::string(static_cast<size_t>(indent) * 4, ' ');
    out << type << " " << n.name;
    if (n.initializer) {
      out << " = ";
      visit(*n.initializer);
    }
    out << ";\n";
  }

  void visit(elpc::AssignExpr& n) override {
    emitLine(n.target + " = " + exprToString(*n.value) + ";");
  }

private:
  std::string exprToString(elpc::ASTNode& node) {
    std::ostringstream saved;
    saved << out.str();
    out.str("");
    out.clear();

    visit(node);
    std::string result = out.str();
    out.str("");
    out.clear();
    out << saved.str();
    return result;
  }
};

// =====================================================================
// 7. Main driver
// =====================================================================

int main(int argc, char** argv) {
  try {
    // Load source
    auto src = loadSource(argc, argv);
    std::cout << "=== Source ===\n" << src.text() << "\n";

    // Lex
    auto lexer = createLexer(src);
    auto tokens = lexer.tokenize();
    std::cout << "=== Tokens (" << tokens.size() << ") ===\n";
    for (auto& t : tokens) {
      std::cout << "  " << t << "\n";
    }
    std::cout << "\n";

    // Parse
    MiniParser parser(tokens);
    auto ast = parser.parseProgram();
    std::cout << "=== AST ===\n";
    elpc::ASTPrinter printer;
    printer.visit(*ast);
    std::cout << printer.out << "\n";

    // Semantic analysis
    elpc::DiagnosticEngine diag;
    MiniSema sema(diag);
    bool ok = sema.analyze(*ast);
    if (!ok) {
      std::cerr << "=== Semantic errors ===\n";
      diag.reportWithSource(std::cerr, src);
      return 1;
    }
    std::cout << "=== Semantic analysis: OK ===\n\n";

    // Code generation
    MiniCodegen codegen;
    std::string cCode = codegen.generate(*ast);
    std::cout << "=== Generated C code ===\n" << cCode << "\n";

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
