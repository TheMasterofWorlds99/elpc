/*
   PARSER.HPP
   Recursive descent parser base class with structured error recovery.

   Error handling modes:
     - Throw mode (default): expect()/expectOneOf() throw on failure.
     - Diagnostic mode: call setDiagnostics(engine). Errors are reported
       via the engine and the parser performs panic-mode recovery by
       skipping to a safe sync point (;, }, or user-defined tokens).

   Subclass and add your grammar rules using peek(), consume(), match(),
   expect(), and synchronize().
*/

#pragma once

#include <elpc/core/tokenReader.hpp>
#include <elpc/diagnostics/diagnosticEngine.hpp>
#include <stdexcept>
#include <vector>

namespace elpc {

template <typename TokenType> class Parser {
private:
  TokenReader<TokenType> reader;
  DiagnosticEngine *diagnostics = nullptr;

protected:
  [[nodiscard]] const Token<TokenType> &peek(size_t offset = 0) const {
    return reader.peek(offset);
  }

  const Token<TokenType> &consume() { return reader.consume(); }

  const Token<TokenType> &previous() const {
    return reader.previous();
  }

  [[nodiscard]] bool check(TokenType type) const { return reader.check(type); }

  bool match(TokenType type) { return reader.match(type); }

  [[nodiscard]] bool isAtEnd() const { return reader.isAtEnd(); }

  template <typename... Types> bool matchAny(Types... types) {
    if ((check(types) || ...)) {
      consume();
      return true;
    }
    return false;
  }

  /// Expect a token of the given type.
  /// In diagnostic mode, reports the error and runs recover().
  const Token<TokenType> &expect(TokenType type, const std::string &message) {
    if (check(type))
      return consume();

    if (diagnostics) {
      diagnostics->error(message, peek().location);
      // Panic-mode recovery: skip to the next sync point
      recover(type);
      // After recovery, return the current token (caller should re-check)
      return peek();
    }

    const auto &t = peek();
    throw std::runtime_error(
        "[elpc] Syntax Error at line " + std::to_string(t.location.line) +
        ", col " + std::to_string(t.location.column) + ": " + message);
  }

  /// Expect one of the given types.
  template <typename... Types>
  const Token<TokenType> &expectOneOf(const std::string &message,
                                      Types... types) {
    if ((check(types) || ...))
      return consume();

    if (diagnostics) {
      diagnostics->error(message, peek().location);
      recover(types...);
      return peek();
    }

    const auto &t = peek();
    throw std::runtime_error(
        "[elpc] Syntax Error at line " + std::to_string(t.location.line) +
        ", col " + std::to_string(t.location.column) + ": " + message);
  }

  /// Panic-mode recovery: skip tokens until one of the sync points.
  /// Each skipped token is reported as a note when a DiagnosticEngine
  /// is attached.
  template <typename... Types> void recover(Types... syncPoints) {
    while (!isAtEnd()) {
      if ((check(syncPoints) || ...))
        return;
      if (diagnostics) {
        diagnostics->note("skipping '" + std::string(peek().lexeme) + "'",
                          peek().location);
      }
      consume();
    }
  }

  /// Skip tokens until one of the sync points is found (silent version).
  template <typename... Types> void synchronize(Types... syncPoints) {
    while (!isAtEnd()) {
      if ((check(syncPoints) || ...))
        return;
      consume();
    }
  }

public:
  Parser(const std::vector<Token<TokenType>> &tokens) : reader(tokens) {}

  void setDiagnostics(DiagnosticEngine &diag) { diagnostics = &diag; }
  void clearDiagnostics() { diagnostics = nullptr; }
};

} // namespace elpc
