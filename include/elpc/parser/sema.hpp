/*
   SEMA.HPP
   This file contains the Sema base class for semantic analysis. It is
   designed to be subclassed by the user, who implements their own analysis
   logic for their language. Sema provides diagnostic helpers and leaves
   all AST traversal and symbol table structure to the user.

   Example usage:
     class MyAnalyzer : public elpc::Sema {
       elpc::SymbolTable<std::string, MySymbol> symbols;
     public:
       MyAnalyzer(elpc::DiagnosticEngine &diag) : elpc::Sema(diag) {}

       void analyzeExit(const ExitNode &node) {
         if (node.value < 0)
           warning("Exit code is negative", {});
       }
     };
*/

#pragma once

#include "elpc/core/loc.hpp"
#include <elpc/core/table.hpp>
#include <elpc/diagnostics/diagnosticEngine.hpp>

namespace elpc {

class Sema {
  DiagnosticEngine &diag;

protected:
  void error(const std::string &message, SourceLocation loc = {}) {
    diag.error(message, loc);
  }

  void warning(const std::string &message, SourceLocation loc = {}) {
    diag.warning(message, loc);
  }

  void note(const std::string &message, SourceLocation loc = {}) {
    diag.note(message, loc);
  }

  bool hasErrors() const { return diag.hasErrors(); }

  DiagnosticEngine &engine() { return diag; }

public:
  explicit Sema(DiagnosticEngine &diag) : diag(diag) {}

  Sema(const Sema &) = delete;
  Sema &operator=(const Sema &) = delete;
};

} // namespace elpc
