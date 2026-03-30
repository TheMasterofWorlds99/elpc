/*
   IRBUILDER.HPP
   This file contains the IRBuilder base class. Users subclass this and
   implement their own emit methods for whatever target they want —
   assembly, C, C++, bytecode, or anything else.

   The base provides common infrastructure every backend needs:
   variable storage via a scoped SymbolTable, scope management,
   diagnostic access, and an output buffer for text-based backends.

   Example:
     class MyCBackend : public elpc::IRBuilder<std::string> {
     public:
       MyCBackend(elpc::DiagnosticEngine &diag)
           : elpc::IRBuilder<std::string>(diag) {}

       std::string emitAdd(std::string lhs, std::string rhs) {
         return "(" + lhs + " + " + rhs + ")";
       }

       std::string emitInt(int value) {
         return std::to_string(value);
       }
     };
*/

#pragma once

#include <elpc/core/table.hpp>
#include <elpc/diagnostics/diagnosticEngine.hpp>
#include <sstream>
#include <string>

namespace elpc {

template <typename ValueType> class IRBuilder {
  DiagnosticEngine &diag;
  SymbolTable<std::string, ValueType> symbols;
  std::ostringstream buffer;

public:
  explicit IRBuilder(DiagnosticEngine &diag) : diag(diag) {}

  IRBuilder(const IRBuilder &) = delete;
  IRBuilder &operator=(const IRBuilder &) = delete;

  void pushScope() { symbols.pushScope(); }
  void popScope() { symbols.popScope(); }

  bool defineVar(const std::string &name, ValueType value,
                 SourceLocation loc = {}) {
    if (!symbols.define(name, std::move(value))) {
      diag.error("Redefinition of variable '" + name + "'", loc);
      return false;
    }
    return true;
  }

  std::optional<ValueType> lookupVar(const std::string &name,
                                     SourceLocation loc = {}) {
    auto val = symbols.lookup(name);
    if (!val.has_value())
      diag.error("Undefined variable '" + name + "'", loc);
    return val;
  }

  void error(const std::string &msg, SourceLocation loc = {}) {
    diag.error(msg, loc);
  }
  void warning(const std::string &msg, SourceLocation loc = {}) {
    diag.warning(msg, loc);
  }
  void note(const std::string &msg, SourceLocation loc = {}) {
    diag.note(msg, loc);
  }

  bool hasErrors() const { return diag.hasErrors(); }

  DiagnosticEngine &engine() { return diag; }

  std::ostringstream &out() { return buffer; }

  std::string result() const { return buffer.str(); }

  void clearBuffer() {
    buffer.str("");
    buffer.clear();
  }
};

} // namespace elpc
