/*
   LEXER.HPP
   DFA-based lexer that tokenises source text owned by a SourceManager.
   Tokens hold string_views into the SourceManager's buffer — no per-token
   allocations.

   Error handling:
     - By default, unrecognized characters throw std::runtime_error.
     - Call setDiagnostics(engine) to use a DiagnosticEngine instead.
       In diagnostic mode, errors are reported and the bad character is
       skipped, allowing the lexer to continue and find more errors.
*/

#pragma once

#include <elpc/core/sourceManager.hpp>
#include <elpc/core/token.hpp>
#include <elpc/diagnostics/diagnosticEngine.hpp>
#include <elpc/lexer/dfa.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elpc {

template <typename TokenType> struct Lexer {
  const SourceManager *source = nullptr;
  size_t pos = 0;
  size_t line = 1;
  size_t column = 1;

  Lexer() = default;
  explicit Lexer(const SourceManager &src) : source(&src) {}

  /// Attach a DiagnosticEngine for structured error reporting.
  /// When set, the lexer reports errors via the engine instead of throwing.
  void setDiagnostics(DiagnosticEngine &diag) {
    diagnostics = &diag;
  }

  /// Remove the diagnostic engine — reverts to throwing on error.
  void clearDiagnostics() {
    diagnostics = nullptr;
  }

  // Main functions
  std::vector<Token<TokenType>> tokenize() {
    reset();
    if (!source)
      return {};

    // Build NFA from current rules
    std::vector<std::pair<std::string_view, int>> rulePatterns;
    for (auto &r : storedRules)
      rulePatterns.emplace_back(r.pattern, static_cast<int>(rulePatterns.size()));
    detail::NFA nfa;
    nfa.compile(rulePatterns);

    std::vector<Token<TokenType>> tokens;
    auto text = source->text();

    while (pos < text.size()) {
      auto remaining = text.substr(pos);
      int ruleIndex = -1;
      size_t matchLen = 0;
      nfa.run(remaining, matchLen, ruleIndex);

      if (ruleIndex == -1 || matchLen == 0) {
        size_t errLine = line;
        size_t errCol = column;

        // Skip one character for error recovery
        if (text[pos] == '\n') {
          line++;
          column = 1;
        } else {
          column++;
        }
        pos++;

        if (diagnostics) {
          diagnostics->error(
              "Unexpected character '" + std::string(1, text[pos - 1]) + "'",
              {errLine, errCol, source->filename()});
          continue; // keep going — find more errors
        }

        throw std::runtime_error("[elpc] Lexer error at line " +
                                 std::to_string(errLine) + ", column " +
                                 std::to_string(errCol));
      }

      const auto &bestRule = storedRules[ruleIndex];
      size_t startPos = pos;

      // Advance position tracking
      for (size_t i = 0; i < matchLen; ++i) {
        if (text[pos] == '\n') {
          line++;
          column = 1;
        } else {
          column++;
        }
        pos++;
      }

      if (bestRule.isSkip)
        continue;

      tokens.emplace_back(*bestRule.type,
                          source->slice(startPos, matchLen),
                          SourceLocation{line, column});
    }

    return tokens;
  }

  // Helper Functions
  void addRule(TokenType type, std::string pattern) {
    storedRules.push_back({type, std::move(pattern), false});
  }

  void addSkip(std::string pattern) {
    storedRules.push_back({std::nullopt, std::move(pattern), true});
  }

  void setSource(const SourceManager &src) {
    source = &src;
    reset();
  }

  void reserveRules(size_t n) { storedRules.reserve(n); }

  std::string_view remaining() const {
    if (!source)
      return {};
    return source->text().substr(pos);
  }

  void reset() {
    pos = 0;
    line = 1;
    column = 1;
  }

private:
  struct StoredRule {
    std::optional<TokenType> type;
    std::string pattern;
    bool isSkip = false;
  };

  std::vector<StoredRule> storedRules;
  DiagnosticEngine *diagnostics = nullptr;
};

} // namespace elpc
