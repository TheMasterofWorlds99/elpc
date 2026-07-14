/*
   TYPE.HPP
   Type system framework — a Type hierarchy with deduplication via TypeContext.

   Types are identified by pointer: each unique type has exactly one instance
   in the TypeContext. Type equality is O(1) pointer comparison.

   Usage:
     elpc::TypeContext ctx;
     auto *intTy  = ctx.getPrimitive(elpc::PrimitiveKind::INT);
     auto *boolTy = ctx.getPrimitive(elpc::PrimitiveKind::BOOL);
     auto *fnTy   = ctx.getFunction({intTy, boolTy}, intTy);

     fnTy->kind();        // TypeKind::FUNCTION
     fnTy->toString();    // "(int, bool) -> int"
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace elpc {

// -----------------------------------------------------------------------
// Type kind
// -----------------------------------------------------------------------

enum class TypeKind : uint8_t {
  PRIMITIVE,
  FUNCTION,
  ARRAY,
  NAMED,
};

// -----------------------------------------------------------------------
// Primitive kinds
// -----------------------------------------------------------------------

enum class PrimitiveKind : uint8_t {
  VOID,
  BOOL,
  INT,
  FLOAT,
  STRING,
  NONE,   // error / untyped
};

// -----------------------------------------------------------------------
// Type base
// -----------------------------------------------------------------------

struct Type {
  const TypeKind kind;

  explicit Type(TypeKind k) : kind(k) {}
  virtual ~Type() = default;

  virtual std::string toString() const = 0;
};

// -----------------------------------------------------------------------
// PrimitiveType
// -----------------------------------------------------------------------

struct PrimitiveType final : Type {
  const PrimitiveKind primKind;

  explicit PrimitiveType(PrimitiveKind pk)
    : Type(TypeKind::PRIMITIVE), primKind(pk) {}

  std::string toString() const override {
    switch (primKind) {
      case PrimitiveKind::VOID:   return "void";
      case PrimitiveKind::BOOL:   return "bool";
      case PrimitiveKind::INT:    return "int";
      case PrimitiveKind::FLOAT:  return "float";
      case PrimitiveKind::STRING: return "string";
      case PrimitiveKind::NONE:   return "<error>";
    }
    return "<unknown>";
  }
};

// -----------------------------------------------------------------------
// FunctionType
// -----------------------------------------------------------------------

struct FunctionType final : Type {
  const std::vector<const Type*> paramTypes;
  const Type *returnType;

  FunctionType(std::vector<const Type*> params, const Type *ret)
    : Type(TypeKind::FUNCTION), paramTypes(std::move(params)), returnType(ret) {}

  std::string toString() const override {
    std::string s = "(";
    for (size_t i = 0; i < paramTypes.size(); ++i) {
      if (i > 0) s += ", ";
      s += paramTypes[i]->toString();
    }
    s += ") -> " + returnType->toString();
    return s;
  }
};

// -----------------------------------------------------------------------
// ArrayType
// -----------------------------------------------------------------------

struct ArrayType final : Type {
  const Type *elementType;

  explicit ArrayType(const Type *elem)
    : Type(TypeKind::ARRAY), elementType(elem) {}

  std::string toString() const override {
    return elementType->toString() + "[]";
  }
};

// -----------------------------------------------------------------------
// NamedType — user-defined types (structs, classes, enums)
// -----------------------------------------------------------------------

struct NamedType final : Type {
  const std::string name;
  const std::vector<const Type*> fieldTypes;

  NamedType(std::string n, std::vector<const Type*> fields = {})
    : Type(TypeKind::NAMED), name(std::move(n)), fieldTypes(std::move(fields)) {}

  std::string toString() const override {
    return name;
  }
};

// -----------------------------------------------------------------------
// TypeContext — owns all types and deduplicates them
// -----------------------------------------------------------------------

class TypeContext {
public:
  TypeContext() {
    // Pre-allocate canonical primitives
    primitives_[static_cast<size_t>(PrimitiveKind::VOID)]   = allocPrimitive(PrimitiveKind::VOID);
    primitives_[static_cast<size_t>(PrimitiveKind::BOOL)]   = allocPrimitive(PrimitiveKind::BOOL);
    primitives_[static_cast<size_t>(PrimitiveKind::INT)]    = allocPrimitive(PrimitiveKind::INT);
    primitives_[static_cast<size_t>(PrimitiveKind::FLOAT)]  = allocPrimitive(PrimitiveKind::FLOAT);
    primitives_[static_cast<size_t>(PrimitiveKind::STRING)] = allocPrimitive(PrimitiveKind::STRING);
    primitives_[static_cast<size_t>(PrimitiveKind::NONE)]   = allocPrimitive(PrimitiveKind::NONE);
  }

  const PrimitiveType *getPrimitive(PrimitiveKind pk) const {
    return primitives_[static_cast<size_t>(pk)];
  }

  const Type *getVoid()   const { return getPrimitive(PrimitiveKind::VOID); }
  const Type *getBool()   const { return getPrimitive(PrimitiveKind::BOOL); }
  const Type *getInt()    const { return getPrimitive(PrimitiveKind::INT); }
  const Type *getFloat()  const { return getPrimitive(PrimitiveKind::FLOAT); }
  const Type *getString() const { return getPrimitive(PrimitiveKind::STRING); }
  const Type *getError()  const { return getPrimitive(PrimitiveKind::NONE); }

  /// Get or create a function type. Deduplicates by parameter/return types.
  const FunctionType *getFunction(std::vector<const Type*> params,
                                   const Type *ret) {
    for (auto &ft : functionTypes_) {
      if (ft->returnType == ret && ft->paramTypes.size() == params.size()) {
        bool match = true;
        for (size_t i = 0; i < params.size(); ++i) {
          if (ft->paramTypes[i] != params[i]) { match = false; break; }
        }
        if (match) return ft.get();
      }
    }
    auto ft = std::make_unique<FunctionType>(std::move(params), ret);
    auto *ptr = ft.get();
    functionTypes_.push_back(std::move(ft));
    return ptr;
  }

  /// Get or create an array type.
  const ArrayType *getArray(const Type *elem) {
    for (auto &at : arrayTypes_) {
      if (at->elementType == elem) return at.get();
    }
    auto at = std::make_unique<ArrayType>(elem);
    auto *ptr = at.get();
    arrayTypes_.push_back(std::move(at));
    return ptr;
  }

  /// Get or create a named type.
  const NamedType *getNamed(std::string_view name,
                             std::vector<const Type*> fields = {}) {
    for (auto &nt : namedTypes_) {
      if (nt->name == name) return nt.get();
    }
    auto nt = std::make_unique<NamedType>(std::string(name), std::move(fields));
    auto *ptr = nt.get();
    namedTypes_.push_back(std::move(nt));
    return ptr;
  }

private:
  PrimitiveType *primitives_[6];
  std::vector<std::unique_ptr<FunctionType>> functionTypes_;
  std::vector<std::unique_ptr<ArrayType>> arrayTypes_;
  std::vector<std::unique_ptr<NamedType>> namedTypes_;

  PrimitiveType *allocPrimitive(PrimitiveKind pk) {
    return new PrimitiveType(pk); // intentional — owned by the array
  }
};

/// Type equality helpers
inline bool typesEqual(const Type *a, const Type *b) {
  return a == b;  // pointer equality — each unique type has one instance
}

inline bool isPrimitive(const Type *t, PrimitiveKind pk) {
  return t->kind == TypeKind::PRIMITIVE
      && static_cast<const PrimitiveType*>(t)->primKind == pk;
}

inline bool isInt(const Type *t)   { return isPrimitive(t, PrimitiveKind::INT); }
inline bool isBool(const Type *t)  { return isPrimitive(t, PrimitiveKind::BOOL); }
inline bool isVoid(const Type *t)  { return isPrimitive(t, PrimitiveKind::VOID); }

} // namespace elpc
