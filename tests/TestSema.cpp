#include "TestHelpers.hpp"

struct VarDecl {
  std::string name;
  int value;
  elpc::SourceLocation location;
};

struct ExitNode {
  std::string varName;
  int literal;
  bool usesVar;
  elpc::SourceLocation location;
};

struct ProgramNode {
  std::vector<VarDecl> decls;
  std::vector<ExitNode> exits;
};

class SemaTestParser : public elpc::Parser<TokenType> {
public:
  SemaTestParser(const std::vector<elpc::Token<TokenType>> &tokens)
      : elpc::Parser<TokenType>(tokens) {}

  ProgramNode parseProgram() {
    ProgramNode program;
    while (!isAtEnd()) {
      if (check(TokenType::LET))
        program.decls.push_back(parseDecl());
      else if (check(TokenType::EXIT))
        program.exits.push_back(parseExit());
      else {
        const auto &t = peek();
        throw std::runtime_error("[elpc] Unexpected token '" +
                                 std::string(t.lexeme) + "'");
      }
    }
    return program;
  }

private:
  VarDecl parseDecl() {
    const auto &letTok = expect(TokenType::LET, "Expected 'let'");
    const auto &name = expect(TokenType::IDENT, "Expected variable name");
    expect(TokenType::ASSIGN, "Expected '='");
    const auto &val = expect(TokenType::INT_LIT, "Expected integer value");
    expect(TokenType::SEMI_COLON, "Expected ';'");

    return VarDecl{std::string(name.lexeme), val.toInt().value_or(0),
                   letTok.location};
  }

  ExitNode parseExit() {
    const auto &exitTok = expect(TokenType::EXIT, "Expected 'exit'");
    expect(TokenType::LEFT_PAREN, "Expected '('");

    // exit can take either a variable name or a literal
    if (check(TokenType::IDENT)) {
      const auto &var = consume();
      expect(TokenType::RIGHT_PAREN, "Expected ')'");
      expect(TokenType::SEMI_COLON, "Expected ';'");
      return ExitNode{std::string(var.lexeme), 0, true, exitTok.location};
    }

    const auto &val =
        expect(TokenType::INT_LIT, "Expected integer or variable");
    expect(TokenType::RIGHT_PAREN, "Expected ')'");
    expect(TokenType::SEMI_COLON, "Expected ';'");
    return ExitNode{"", val.toInt().value_or(0), false, exitTok.location};
  }
};

class SemaAnalyzer : public elpc::Sema {
  elpc::SymbolTable<std::string, VarDecl> symbols;

  // Track which variables were actually used
  std::vector<std::string> used;

public:
  SemaAnalyzer(elpc::DiagnosticEngine &diag) : elpc::Sema(diag) {}

  void analyze(const ProgramNode &program) {
    // First pass — register all declarations
    for (const auto &decl : program.decls)
      analyzeDecl(decl);

    // Second pass — check all exit statements
    for (const auto &exit : program.exits)
      analyzeExit(exit);

    // Third pass — warn about unused variables
    checkUnused(program.decls);
  }

private:
  void analyzeDecl(const VarDecl &decl) {
    if (!symbols.define(decl.name, decl)) {
      error("Duplicate variable declaration '" + decl.name + "'",
            decl.location);
    }
  }

  void analyzeExit(const ExitNode &exit) {
    if (exit.usesVar) {
      // Check variable is defined
      auto sym = symbols.lookup(exit.varName);
      if (!sym.has_value()) {
        error("Undefined variable '" + exit.varName + "'", exit.location);
        return;
      }
      used.push_back(exit.varName);

      // Check the resolved value
      if (sym->value < 0)
        warning("Exit code for '" + exit.varName + "' is negative (" +
                    std::to_string(sym->value) + ")",
                exit.location);
    } else {
      if (exit.literal < 0)
        warning("Exit code " + std::to_string(exit.literal) + " is negative",
                exit.location);
    }
  }

  void checkUnused(const std::vector<VarDecl> &decls) {
    for (const auto &decl : decls) {
      bool wasUsed = false;
      for (const auto &u : used)
        if (u == decl.name) {
          wasUsed = true;
          break;
        }
      if (!wasUsed)
        note("Variable '" + decl.name + "' is declared but never used",
             decl.location);
    }
  }
};

void runSemaTest(const std::string &label, const std::string &src) {
  std::cout << "\n=== " << label << " ===\n";
  std::cout << "src: " << src << "\n";

  try {
    auto tokens = makeLexer(src).tokenize();

    SemaTestParser parser(tokens);
    auto program = parser.parseProgram();

    elpc::DiagnosticEngine engine;
    SemaAnalyzer sema(engine);
    sema.analyze(program);

    if (engine.empty()) {
      std::cout << "-- No diagnostics --\n";
    } else {
      std::cout << "-- Diagnostics (" << engine.count() << ") --\n";
      engine.reportDiagnostics(std::cout);
    }

    std::cout << "-- Has errors: " << (engine.hasErrors() ? "yes" : "no")
              << " --\n";

  } catch (const std::exception &e) {
    std::cerr << "[exception] " << e.what() << "\n";
  }
}

int main() {
  // Clean — no issues
  runSemaTest("CLEAN", "let x = 5; exit(x);");

  // Undefined variable
  runSemaTest("UNDEFINED VARIABLE", "let x = 5; exit(y);");

  // Duplicate declaration
  runSemaTest("DUPLICATE DECLARATION", "let x = 5; let x = 10; exit(x);");

  // Negative exit code via literal
  runSemaTest("NEGATIVE LITERAL EXIT CODE", "exit(-1);");

  // Negative exit code via variable
  runSemaTest("NEGATIVE VARIABLE EXIT CODE", "let code = -1; exit(code);");

  // Unused variable note
  runSemaTest("UNUSED VARIABLE", "let x = 5; let y = 10; exit(x);");

  // Multiple issues at once
  runSemaTest("MULTIPLE ISSUES", "let x = 5; let x = 99; exit(z);");
}
