/*
   RULE.HPP
   This file contains the Rule struct used by the Lexer to define token
   matching patterns. A rule with no type is implicitly a skip rule.
*/

#pragma once

#include <optional>
#include <regex>

namespace elpc {

template <typename TokenType> struct Rule {
  std::optional<TokenType> type;
  std::regex pattern;

  // Normal Token Rule
  Rule(TokenType t, std::regex pattern)
      : type(t), pattern(std::move(pattern)) {}

  // Skip Rule
  Rule(std::regex pattern) : type(std::nullopt), pattern(std::move(pattern)) {}

  bool isSkip() const { return !type.has_value(); }
};

} // namespace elpc
