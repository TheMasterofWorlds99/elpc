/*
   PARSER.HPP
   This file contains all the logic for the parser base class that the user will
   use to write there own grammer functions and logic
*/

#pragma once

#include <elpc/core/tokenReader.hpp>
#include <stdexcept>
#include <vector>

namespace elpc {
template <typename TokenType> class Parser {
private:
  TokenReader<TokenType> reader;

protected:
  const Token<TokenType> &peek() const { return reader.peek(); }

  const Token<TokenType> &consume() { return reader.consume(); }

  const Token<TokenType> &previous() const { return reader.previous(); }

  bool check(TokenType type) const { return reader.check(type); }

  bool match(TokenType type) { return reader.match(type); }

  bool isAtEnd() const { return reader.isAtEnd(); }

  template <typename... Types> bool matchAny(Types... types) {
    if ((check(types) || ...)) {
      consume();
      return true;
    }
    return false;
  }

  const Token<TokenType> &expect(TokenType type, const std::string &message) {
    if (check(type))
      return consume();

    const auto &t = peek();
    throw std::runtime_error(
        "[elpc] Syntax Error at line " + std::to_string(t.location.line) +
        ", col " + std::to_string(t.location.column) + ": " + message);
  }

  template <typename... Types>
  const Token<TokenType> &expectOneOf(const std::string &message,
                                      Types... types) {
    if ((check(types) || ...))
      return consume();

    const auto &t = peek();
    throw std::runtime_error(
        "[elpc] Syntax Error at line " + std::to_string(t.location.line) +
        ", col " + std::to_string(t.location.column) + ": " + message);
  }

  template <typename... Types> void synchronize(Types... syncPoints) {
    while (!isAtEnd()) {
      if ((check(syncPoints) || ...))
        return;
      consume();
    }
  }

public:
  Parser(const std::vector<Token<TokenType>> &tokens) : reader(tokens) {}
};

} // namespace elpc
