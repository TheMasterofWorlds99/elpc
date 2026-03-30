/*
   LEXER.HPP
   This file contains the building blocks for creating a lexer based on the
   token logic in TOKEN.HPP
*/

#pragma once

#include <cstdint>
#include <elpc/core/token.hpp>
#include <elpc/lexer/rule.hpp>
#include <string_view>

namespace elpc {

template <typename TokenType> struct Lexer {
  std::string input;
  std::vector<Rule<TokenType>> rules; // Grows as the user puts in more rules
  size_t pos = 0;                     // Position in the lexing process
  size_t line = 1;                    // Line number in the lexing process
  size_t column = 1;                  // Column number in the lexing process

  Lexer() = default;
  Lexer(std::string_view src) : input(src) {}

  // Main functions
  std::vector<Token<TokenType>> tokenize() {
    // Ensure the position is reset for multiple tokensises
    reset();
    std::vector<Token<TokenType>> tokens;

    while (pos < input.size()) {
      size_t bestLength = 0;
      size_t bestIndex = SIZE_MAX;

      auto begin = input.cbegin() + pos;
      auto end = input.cend();

      for (size_t i = 0; i < rules.size(); ++i) {
        std::match_results<std::string::const_iterator> match;

        if (std::regex_search(begin, end, match, rules[i].pattern,
                              std::regex_constants::match_continuous)) {
          size_t len = match.length();
          if (len == 0)
            continue;
          if (len > bestLength) {
            bestLength = len;
            bestIndex = i;
          }
        }
      }

      if (bestIndex == SIZE_MAX) {
        throw std::runtime_error("[elpc] Lexer error at line " +
                                 std::to_string(line) + ", column " +
                                 std::to_string(column));
      }

      const auto &bestRule = rules[bestIndex];
      std::string_view lexeme(input.data() + pos, bestLength);

      SourceLocation loc{line, column};

      for (size_t i = 0; i < bestLength; ++i) {
        if (input[pos] == '\n') {
          line++;
          column = 1;
        } else {
          column++;
        }
        pos++;
      }

      if (bestRule.isSkip())
        continue;

      tokens.emplace_back(*bestRule.type, std::string(lexeme), loc);
    }

    return tokens;
  }

  // Helper Functions
  void addRule(TokenType type, std::string pattern) {
    rules.emplace_back(type, std::regex(pattern));
  }
  void addSkip(std::string pattern) { rules.emplace_back(std::regex(pattern)); }

  void setInput(std::string_view src) {
    input = src;
    reset();
  }

  void reserveRules(size_t n) { rules.reserve(n); }

  std::string_view remaining() const {
    return std::string_view(input).substr(pos);
  }

  void reset() {
    // Reset the lexer for either re-use or due to an error
    pos = 0;
    line = 1;
    column = 1;
  }
};

} // namespace elpc
