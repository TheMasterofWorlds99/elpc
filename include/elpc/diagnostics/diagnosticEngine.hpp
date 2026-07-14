/*
   DIAGNOSTICENGINE.HPP
   Collects and reports diagnostics with optional source-context display.

   Basic usage:
     diag.reportDiagnostics(cerr);  // line:col format
     diag.reportWithSource(cerr, src);  // shows source line + caret
*/

#pragma once

#include <elpc/core/sourceManager.hpp>
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

  [[nodiscard]] bool hasErrors() const { return errorSeen; }
  [[nodiscard]] bool empty() const { return diagnostics.empty(); }
  [[nodiscard]] size_t count() const { return diagnostics.size(); }

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

  /// Report diagnostics with source-context display.
  /// Each diagnostic is shown with:
  ///   [severity] [file line:col] message
  ///    │
  ///  5 │ let x = 42;
  ///  6 │ print bad_var;
  ///    │       ^^^^^^^
  ///  7 │ return 0;
  void reportWithSource(std::ostream &out,
                         const SourceManager &src) const {
    auto text = src.text();

    for (const auto &d : diagnostics) {
      // Header line
      out << severityLabel(d.severity) << " [";
      if (!d.location.filename.empty())
        out << d.location.filename << " ";
      out << d.location.line << ":" << d.location.column << "] "
          << d.message << "\n";

      if (d.location.line == 0)
        continue;

      // Find the start of the error line (1-based → 0-based)
      size_t lineNum = d.location.line;
      size_t colNum = d.location.column;

      // Find line start by scanning line starts or from the beginning
      size_t lineStart = 0;
      size_t currentLine = 1;
      for (size_t i = 0; i < text.size() && currentLine < lineNum; ++i) {
        if (text[i] == '\n') {
          lineStart = i + 1;
          currentLine++;
        }
      }

      // Find line end
      size_t lineEnd = text.find('\n', lineStart);
      if (lineEnd == std::string_view::npos)
        lineEnd = text.size();

      auto line = text.substr(lineStart, lineEnd - lineStart);

      // Line number prefix width
      int lineWidth = static_cast<int>(std::to_string(lineNum + 1).size());
      std::string padding(static_cast<size_t>(lineWidth), ' ');

      // Separator line
      out << padding << "  │\n";

      // Source line
      out << lineNum << " │ " << line << "\n";

      // Caret line
      out << padding << "  │ ";
      if (colNum > 0 && colNum <= line.size()) {
        // Print spaces before the caret
        for (size_t i = 0; i < colNum - 1; ++i)
          out << ' ';
        out << '^';

        // Underline the rest of the token (up to the next space or end)
        for (size_t i = colNum; i < line.size(); ++i) {
          if (line[i] == ' ' || line[i] == ')' || line[i] == ';')
            break;
          out << '~';
        }
      }
      out << "\n\n";
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
