/*
   TOKEN.HPP
   This file holds all the logic for tokens, which are the variables and
   keywords of your language. A token is comprised of a TokenType and
   std::string value. This will allow the user to define there own enum class
   without worrying about setting up structs and having to worry about specific
   tokens and edge cases
*/

#pragma once

#include <charconv>
#include <elpc/core/loc.hpp>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>

namespace elpc {

template <typename TokenType> struct Token {
  TokenType type; // A Token has a type, that the user makes via, for example an
                  // enum class
  std::string lexeme; // This is the "value" that could hold what the user wants
                      // (A numerical value for an intger literal or a variable)
  SourceLocation
      location; // This is the location of the token in the file or src code.
                // Helps with debugging so you know where to look

  Token() = default;

  Token(TokenType type, std::string_view lexeme, SourceLocation loc = {})
      : type(type), lexeme(lexeme), location(loc) {}

  // Helper Functions
  bool is(TokenType t) const { return type == t; }
  bool isNot(TokenType t) const { return type != t; }

  template <typename... Args> bool isOneOf(Args... types) const {
    return ((type == types) || ...);
  }

  // Converters
  std::optional<int> toInt() const {
    int value;
    // from_chars takes a start and end pointer—perfect for string_view
    auto [ptr, ec] =
        std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);

    if (ec == std::errc()) {
      return value;
    }
    // If we made it here, print an error
    std::cerr << "Error! Failed to convert lexeme to int!\n";
    return std::nullopt;
  }
  std::optional<float> toFloat() const {
    float value;
    auto [ptr, ec] =
        std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);

    if (ec == std::errc()) {
      return value;
    }
    std::cerr << "Error! Failed to convert lexeme to float!\n";
    return std::nullopt;
  }
  std::optional<double> toDouble() const {
    double value;
    auto [ptr, ec] =
        std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);

    if (ec == std::errc()) {
      return value;
    }
    std::cerr << "Error! Failed to convert lexeme to double!\n";
    return std::nullopt;
  }
  // Bool can be done later
};

// Utility functions
template <typename TokenType>
bool operator==(const Token<TokenType> &a, const Token<TokenType> &b) {
  return a.type == b.type && a.lexeme == b.lexeme;
}

template <typename TokenType>
bool operator!=(const Token<TokenType> &a, const Token<TokenType> &b) {
  return !(a == b);
}

template <typename TokenType>
std::ostream &operator<<(std::ostream &os, const Token<TokenType> &token) {
  return os << "Token(type=" << static_cast<int>(token.type) << ", lexeme=\""
            << token.lexeme << "\")";
}

} // namespace elpc
