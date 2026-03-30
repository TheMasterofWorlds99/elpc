/*
   TABLE.HPP
   This file contains all the logic for various tables the user might use in
   there project, like a symbol table for a semantic analyzer, or a keyword
   table for something else
*/

#pragma once

#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace elpc {

template <typename KeyType, typename ValueType> class SymbolTable {
  using Scope = std::unordered_map<KeyType, ValueType>;
  std::vector<Scope> scopes;

public:
  SymbolTable() { pushScope(); }

  void pushScope() { scopes.emplace_back(); }

  void popScope() {
    if (scopes.size() <= 1)
      throw std::runtime_error(
          "[elpc] SymbolTable: cannot pop the global scope");
    scopes.pop_back();
  }

  size_t depth() const { return scopes.size(); }

  // Define a symbol is the current scope.
  // Returns false if the key is already defined in the current scope
  bool define(const KeyType &key, ValueType value) {
    auto &current = scopes.back();
    if (current.count(key)) {
      return false;
    }
    current.emplace(key, std::move(value));
    return true;
  }

  void defineOrReplace(const KeyType &key, ValueType value) {
    scopes.back()[key] = std::move(value);
  }

  std::optional<ValueType> lookup(const KeyType &key) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto found = it->find(key);
      if (found != it->end())
        return found->second;
    }
    return std::nullopt;
  }

  std::optional<ValueType> lookupCurrent(const KeyType &key) const {
    const auto &current = scopes.back();
    auto found = current.find(key);
    if (found != current.end())
      return found->second;
    return std::nullopt;
  }

  bool isDefined(const KeyType &key) const { return lookup(key).has_value(); }

  bool isDefinedCurrent(const KeyType &key) const {
    return lookupCurrent(key).has_value();
  }

  void reset() {
    scopes.clear();
    pushScope();
  }
};

} // namespace elpc
