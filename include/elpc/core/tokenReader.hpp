/*
   TOKENREADER.HPP
   This file contains all the logic for the tokenreader (duh). This token reader
   is what goes over all the tokens for the parser, which then constructs the
   key structure the user wants for there langauge, like an AST
*/

#pragma once

#include <cassert>
#include <elpc/core/token.hpp>
#include <vector>

namespace elpc {
template <typename TokenType> class TokenReader {
  const std::vector<Token<TokenType>> &tokens;
  size_t pos = 0;

public:
  TokenReader(const std::vector<Token<TokenType>> &t) : tokens(t) {}

  const Token<TokenType> &peek() const {
    if (isAtEnd())
      return tokens.back();
    return tokens[pos];
  }

  const Token<TokenType> &previous() const {
    assert(pos > 0 && "previous() called before advance!");
    return tokens[pos - 1];
  }

  const Token<TokenType> &consume() {
    if (!isAtEnd())
      pos++;
    return previous();
  }

  bool check(TokenType type) const { return (!isAtEnd() && peek().is(type)); }

  bool match(TokenType type) {
    if (check(type)) {
      consume();
      return true;
    }
    return false;
  }

  void discardUntil(TokenType t) {
    while (!isAtEnd() && !peek().is(t)) {
      consume();
    }
  }

  bool isAtEnd() const { return pos >= tokens.size(); }
};
} // namespace elpc
