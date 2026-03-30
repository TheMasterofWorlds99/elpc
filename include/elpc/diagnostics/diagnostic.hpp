/*
   DIAGNOSTIC.HPP
   This file contains the Diagnostic struct and logic. It will be the bases for
   the diagnostic engine, which will be what allows for warning, error and
   normal notes to appear for your compiler or language
*/

#pragma once

#include <elpc/core/loc.hpp>
#include <string>

namespace elpc {

enum class Severity { ERROR, WARNING, NOTE };

struct Diagnostic {
  Severity severity;       // Severity of the diagnostic
  std::string message;     // Diagnostic Message
  SourceLocation location; // Location of diagnostic
};

} // namespace elpc
