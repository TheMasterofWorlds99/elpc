/*
   TOKEN.HPP
   Tokens are lightweight views into source text — the lexeme is a
   string_view pointing to memory owned by a SourceManager that outlives
   the token.
*/

#pragma once

#include <charconv>
#include <cstdlib>
#include <elpc/core/loc.hpp>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>

namespace elpc {

template <typename TokenType> struct Token {
  TokenType type; // A Token has a type, that the user makes via, for example an
                  // enum class
  std::string_view lexeme; // View into SourceManager-owned memory — zero-copy
  SourceLocation
      location; // This is the location of the token in the file or src code.
                // Helps with debugging so you know where to look

  constexpr Token() = default;

  constexpr Token(TokenType type, std::string_view lexeme, SourceLocation loc = {})
      : type(type), lexeme(lexeme), location(loc) {}

  // Helper Functions
  [[nodiscard]] constexpr bool is(TokenType t) const { return type == t; }
  [[nodiscard]] constexpr bool isNot(TokenType t) const { return type != t; }

  template <typename... Args> [[nodiscard]] constexpr bool isOneOf(Args... types) const {
    return ((type == types) || ...);
  }

  // Converters
  [[nodiscard]] std::optional<int> toInt() const {
    int value;
    auto [ptr, ec] =
        std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);

    if (ec == std::errc()) {
      return value;
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<float> toFloat() const {
    float value;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 202306
    auto [ptr, ec] =
        std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
    if (ec == std::errc())
      return value;
#else
    // strtof requires null-termination — copy to temporary on fallback path
    std::string tmp(lexeme);
    char *end = nullptr;
    value = std::strtof(tmp.c_str(), &end);
    if (end != tmp.c_str() && *end == '\0')
      return value;
#endif
    return std::nullopt;
  }
  [[nodiscard]] std::optional<double> toDouble() const {
    double value;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 202306
    auto [ptr, ec] =
        std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
    if (ec == std::errc())
      return value;
#else
    std::string tmp(lexeme);
    char *end = nullptr;
    value = std::strtod(tmp.c_str(), &end);
    if (end != tmp.c_str() && *end == '\0')
      return value;
#endif
    return std::nullopt;
  }
  // Bool can be done later
};

// Utility functions
template <typename TokenType>
constexpr bool operator==(const Token<TokenType> &a, const Token<TokenType> &b) {
  return a.type == b.type && a.lexeme == b.lexeme;
}

template <typename TokenType>
constexpr bool operator!=(const Token<TokenType> &a, const Token<TokenType> &b) {
  return !(a == b);
}

template <typename TokenType>
std::ostream &operator<<(std::ostream &os, const Token<TokenType> &token) {
  return os << "Token(type=" << static_cast<int>(token.type) << ", lexeme=\""
            << token.lexeme << "\")";
}

} // namespace elpc
