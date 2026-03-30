// tests/TestHelpers.hpp
#pragma once

#include <elpc/elpc.hpp>
#include <iostream>
#include <string>
#include <vector>

enum class TokenType {
  EXIT,
  RETURN,
  LET,
  LEFT_PAREN,
  RIGHT_PAREN,
  LEFT_BRACE,
  RIGHT_BRACE,
  SEMI_COLON,
  ASSIGN,
  INT_LIT,
  FLOAT_LIT,
  MINUS,
  IDENT
};

inline elpc::Lexer<TokenType> makeLexer(const std::string &src) {
  elpc::Lexer<TokenType> lexer(src);
  lexer.addRule(TokenType::EXIT, "\\bexit\\b");
  lexer.addRule(TokenType::RETURN, "\\breturn\\b");
  lexer.addRule(TokenType::LET, "\\blet\\b");
  lexer.addRule(TokenType::LEFT_PAREN, "\\(");
  lexer.addRule(TokenType::RIGHT_PAREN, "\\)");
  lexer.addRule(TokenType::LEFT_BRACE, "\\{");
  lexer.addRule(TokenType::RIGHT_BRACE, "\\}");
  lexer.addRule(TokenType::SEMI_COLON, "\\;");
  lexer.addRule(TokenType::ASSIGN, "\\=");
  lexer.addRule(TokenType::FLOAT_LIT, "\\b[0-9]+\\.[0-9]+\\b");
  lexer.addRule(TokenType::INT_LIT, "\\b[0-9]+\\b");
  lexer.addRule(TokenType::MINUS, "\\-");
  lexer.addRule(TokenType::IDENT, "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b");
  lexer.addSkip("\\s+");
  return lexer;
}
