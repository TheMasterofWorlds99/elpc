/*
   INTERNEDSTRING.HPP
   String interning — deduplicates strings so that identical strings share
   one allocation and can be compared by pointer equality (O(1)).

   Usage:
     elpc::InternedStringPool pool;
     auto a = pool.intern("hello");
     auto b = pool.intern("hello");
     auto c = pool.intern("world");

     a == b;  // true (pointer equality)
     a == c;  // false
     a.view(); // "hello"
*/

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace elpc {

/// A handle to an interned string — O(1) comparison, no allocation on copy.
class InternedString {
public:
  InternedString() = default;

  std::string_view view() const noexcept {
    return str ? std::string_view(*str) : std::string_view{};
  }

  bool empty() const noexcept { return !str || str->empty(); }

  explicit operator bool() const noexcept { return str != nullptr; }

  friend bool operator==(const InternedString &a, const InternedString &b) {
    return a.str == b.str;
  }
  friend bool operator!=(const InternedString &a, const InternedString &b) {
    return a.str != b.str;
  }
  friend bool operator<(const InternedString &a, const InternedString &b) {
    return a.view() < b.view();
  }

private:
  friend class InternedStringPool;
  explicit InternedString(const std::string *s) : str(s) {}
  const std::string *str = nullptr;
};

/// Pool that owns all interned strings and hands out InternedString handles.
/// Interned strings live until the pool is destroyed.
class InternedStringPool {
public:
  /// Intern a string — returns an InternedString handle.
  /// Accepts string_view, const char*, and std::string.
  /// If the string was already interned, returns the existing handle (O(1)).
  InternedString intern(std::string_view s) {
    auto [it, inserted] = storage.emplace(s);
    return InternedString(&(*it));
  }

  /// Intern a string by moving an existing std::string.
  /// The moved-from string is left in a valid-but-unspecified state.
  InternedString internOwned(std::string &&s) {
    auto [it, inserted] = storage.emplace(std::move(s));
    return InternedString(&(*it));
  }

  /// Number of unique strings in the pool.
  size_t size() const noexcept { return storage.size(); }

  /// Clear all interned strings.
  void clear() { storage.clear(); }

private:
  struct StringHash {
    using is_transparent = void;
    size_t operator()(const std::string &s) const noexcept {
      return std::hash<std::string>{}(s);
    }
    size_t operator()(std::string_view s) const noexcept {
      return std::hash<std::string_view>{}(s);
    }
  };

  struct StringEqual {
    using is_transparent = void;
    bool operator()(const std::string &a, const std::string &b) const noexcept {
      return a == b;
    }
    bool operator()(const std::string &a, std::string_view b) const noexcept {
      return a == b;
    }
  };

  std::unordered_set<std::string, StringHash, StringEqual> storage;
};

} // namespace elpc
