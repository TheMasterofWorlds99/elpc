// =====================================================================
// REPL — A simple read-eval-print loop demonstrating interactive use
//
// Build: g++ -std=c++20 -I../../include repl.cpp -o repl
// Usage: ./repl
// =====================================================================

#include <elpc/elpc.hpp>

#include <cctype>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

// Token types for our REPL language
enum class Token {
  INT_LIT, IDENT,
  PLUS, MINUS, STAR, SLASH, SEMI, LPAREN, RPAREN,
  PRINT, LET, EQ, ASSIGN, ERROR, EOF_
};

elpc::Lexer<Token> createLexer(const elpc::SourceManager &src) {
  elpc::Lexer<Token> lexer(src);
  lexer.addRule(Token::PRINT, "\\bprint\\b");
  lexer.addRule(Token::LET,   "\\blet\\b");
  lexer.addRule(Token::IDENT, "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b");
  lexer.addRule(Token::INT_LIT, "\\b[0-9]+\\b");
  lexer.addRule(Token::EQ,  "==");
  lexer.addRule(Token::PLUS,  "\\+");
  lexer.addRule(Token::MINUS, "-");
  lexer.addRule(Token::STAR,  "\\*");
  lexer.addRule(Token::SLASH, "/");
  lexer.addRule(Token::ASSIGN, "=");
  lexer.addRule(Token::LPAREN, "\\(");
  lexer.addRule(Token::RPAREN, "\\)");
  lexer.addRule(Token::SEMI,  ";");
  lexer.addSkip("\\s+");
  return lexer;
}

// Evaluate a token stream and print the result
void evalAndPrint(const std::string &line, int lineNum) {
  try {
    auto src = elpc::SourceManager::fromString(
        "<repl:" + std::to_string(lineNum) + ">", line);
    auto lexer = createLexer(src);
    auto tokens = lexer.tokenize();

    if (tokens.empty()) return;

    // Simple expression evaluator: just prints tokens
    for (auto &t : tokens) {
      std::cout << "  " << t << "\n";
    }
  } catch (const std::exception &e) {
    std::cout << "  Error: " << e.what() << "\n";
  }
}

int main() {
  std::cout << "ELPC REPL — type expressions; press Ctrl+D to exit\n\n";

  int lineNum = 0;
  std::string line;

  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) break;
    lineNum++;

    if (line.empty()) continue;
    if (line == ":q" || line == ":quit") break;

    evalAndPrint(line, lineNum);
  }

  std::cout << "\nGoodbye!\n";
  return 0;
}
