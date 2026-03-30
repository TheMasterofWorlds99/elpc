/*
   DIAGNOSTICENGINE.HPP
   This file contains the DiagnosticEngine, which collects and reports
   diagnostics emitted during lexing, parsing, or semantic analysis.
   It is opt-in — the default parser and lexer still throw std::runtime_error
   for quick prototyping. Use DiagnosticEngine when you want structured,
   multi-error reporting with full control over output.
*/

#pragma once

#include <elpc/diagnostics/diagnostic.hpp>
#include <ostream>
#include <string>
#include <vector>

namespace elpc {

struct DiagnosticEngine {
private:
  std::vector<Diagnostic> diagnostics;
  bool errorSeen = false;

public:
  void error(const std::string &message, SourceLocation loc = {}) {
    diagnostics.push_back({Severity::ERROR, message, loc});
    errorSeen = true;
  }

  void warning(const std::string &message, SourceLocation loc = {}) {
    diagnostics.push_back({Severity::WARNING, message, loc});
  }

  void note(const std::string &message, SourceLocation loc = {}) {
    diagnostics.push_back({Severity::NOTE, message, loc});
  }

  bool hasErrors() const { return errorSeen; }
  bool empty() const { return diagnostics.empty(); }
  size_t count() const { return diagnostics.size(); }

  const std::vector<Diagnostic> &all() const { return diagnostics; }

  void reportDiagnostics(std::ostream &out) const {
    for (const auto &d : diagnostics) {
      out << severityLabel(d.severity) << " [";

      if (!d.location.filename.empty())
        out << d.location.filename << " ";

      out << d.location.line << ":" << d.location.column << "] " << d.message
          << "\n";
    }
  }

  void clear() {
    diagnostics.clear();
    errorSeen = false;
  }

private:
  static const char *severityLabel(Severity s) {
    switch (s) {
    case Severity::ERROR:
      return "[error]  ";
    case Severity::WARNING:
      return "[warning]";
    case Severity::NOTE:
      return "[note]   ";
    }
    return "[unknown]";
  }
};

} // namespace elpc
