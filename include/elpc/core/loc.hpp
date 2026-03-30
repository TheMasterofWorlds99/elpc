/*
   LOC.HPP
   This file holds the logic for SourceLocation, which holds line, column,
   and filename for precise diagnostic reporting. The filename is a
   string_view — it is expected to point to a long-lived string owned
   by the compiler driver or source manager.
*/

#pragma once

#include <cstddef>
#include <string_view>

namespace elpc {

struct SourceLocation {
  size_t line = 1;
  size_t column = 1;
  std::string_view filename;
};

} // namespace elpc
