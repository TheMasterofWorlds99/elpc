# Diagnostics module

Structured error, warning, and note reporting for all pipeline stages.

## `Diagnostic` (`elpc/diagnostics/diagnostic.hpp`)

A single diagnostic entry with severity, message, and source location.

```cpp
enum class Severity { ERROR, WARNING, NOTE };

struct Diagnostic {
  Severity severity;
  std::string message;
  SourceLocation location;
};
```

## `DiagnosticEngine` (`elpc/diagnostics/diagnosticEngine.hpp`)

Collects and reports diagnostics. Used by all pipeline stages (lexer, parser, sema, codegen).

```cpp
elpc::DiagnosticEngine diag;

diag.error("undefined variable 'x'", {3, 5});
diag.warning("unused variable 'y'", {4, 1});
diag.note("declared here", {1, 10});

// Query
diag.hasErrors();  // true
diag.empty();      // false
diag.count();      // 3

// Report to a stream
diag.reportDiagnostics(std::cerr);
// [error]   [3:5] undefined variable 'x'
// [warning] [4:1] unused variable 'y'
// [note]    [1:10] declared here

// Access all diagnostics
const auto &all = diag.all();

// Clear for reuse
diag.clear();
```

### Integration with pipeline stages

All major components optionally accept a DiagnosticEngine:

```cpp
elpc::DiagnosticEngine diag;

// Lexer
elpc::Lexer<Token> lexer(src);
lexer.setDiagnostics(diag);
auto tokens = lexer.tokenize();  // reports errors, continues

// Parser  
parser.setDiagnostics(diag);
// expect() now reports via engine instead of throwing

// Sema (always uses engine)
MyAnalyzer analyzer(diag);
analyzer.analyze(ast);

// IRBuilder (always uses engine)
CBackend backend(diag);
```
