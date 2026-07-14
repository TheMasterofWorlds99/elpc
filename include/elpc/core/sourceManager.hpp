/*
   SOURCEMANAGER.HPP
   Owns the source text and hands out string_view slices and line/column
   lookups. Outlives any Token whose lexeme references its text.

   Usage:
     auto src = elpc::SourceManager::fromString("test.elpc", "exit 42;");
     elpc::Lexer<Tk> lexer(src);
     auto tokens = lexer.tokenize();  // tokens hold string_views into src
*/

#pragma once

#include <algorithm>
#include <cstddef>
#include <elpc/core/loc.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace elpc {

class SourceManager {
  std::string filename_;
  std::string source_;
  std::vector<size_t> lineStarts_;

public:
  /// Create from a string (useful for tests and REPLs).
  static SourceManager fromString(std::string_view name,
                                  std::string_view source) {
    SourceManager sm;
    sm.filename_ = name;
    sm.source_ = source;
    sm.buildLineStarts();
    return sm;
  }

  /// Load from a file on disk.
  static SourceManager fromFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
      throw std::runtime_error("[elpc] Failed to open file: " + path.string());

    SourceManager sm;
    sm.filename_ = path.string();

    auto size = file.tellg();
    file.seekg(0);
    sm.source_.resize(static_cast<size_t>(size));
    file.read(sm.source_.data(), size);
    sm.buildLineStarts();

    return sm;
  }

  /// Full source text as a string_view.
  [[nodiscard]] std::string_view text() const noexcept {
    return source_;
  }

  /// Filename (or label) of this source.
  [[nodiscard]] const std::string &filename() const noexcept {
    return filename_;
  }

  /// Convert a zero-based byte offset to a SourceLocation.
  [[nodiscard]] SourceLocation location(size_t offset) const noexcept {
    if (source_.empty() || lineStarts_.empty())
      return {1, 1, filename_};

    // Binary search for the last line start <= offset
    auto it = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), offset);
    if (it == lineStarts_.begin())
      return {1, 1, filename_};
    --it;

    size_t line = 1 + static_cast<size_t>(it - lineStarts_.begin());
    size_t column = 1 + (offset - *it);
    return {line, column, filename_};
  }

  /// Return a string_view slice into the source (no copy).
  [[nodiscard]] std::string_view slice(size_t offset,
                                       size_t length) const noexcept {
    if (offset + length > source_.size())
      return {};
    return std::string_view(source_).substr(offset, length);
  }

  /// Raw null-terminated C string of the full source (for C interop).
  [[nodiscard]] const char *c_str() const noexcept {
    return source_.c_str();
  }

  /// Total bytes of source text.
  [[nodiscard]] size_t size() const noexcept { return source_.size(); }

private:
  SourceManager() = default;

  void buildLineStarts() {
    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (size_t i = 0; i < source_.size(); ++i) {
      if (source_[i] == '\n')
        lineStarts_.push_back(i + 1);
    }
  }
};

} // namespace elpc
