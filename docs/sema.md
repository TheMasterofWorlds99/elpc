# Semantic analysis

## `Sema` (`elpc/parser/sema.hpp`)

Base class for semantic analysis. Subclass it and use the protected diagnostic helpers. All errors and warnings go through a `DiagnosticEngine`.

```cpp
class MyAnalyzer : public elpc::Sema {
  elpc::SymbolTable<std::string, std::string> symbols; // name → type

public:
  MyAnalyzer(elpc::DiagnosticEngine &diag) : elpc::Sema(diag) {}

  bool analyze(elpc::BlockStmt &program) {
    // Walk the AST, calling error()/warning()/note()
    // when you find problems
    return !hasErrors();
  }

  void checkVarDecl(elpc::VarDecl &decl) {
    auto &name = decl.name;
    if (!symbols.define(name, decl.typeName)) {
      error("duplicate variable '" + name + "'", decl.location);
    }
  }
};
```

### Protected API

| Method | Description |
|--------|-------------|
| `error(msg, loc)` | Report an error |
| `warning(msg, loc)` | Report a warning |
| `note(msg, loc)` | Report a note |
| `hasErrors()` | True if any error was reported |
| `engine()` | Access the underlying DiagnosticEngine |

The class is non-copyable.

## `SymbolTable<Key, Value>` (`elpc/core/table.hpp`)

Scoped symbol table. See [core docs](core.md).
