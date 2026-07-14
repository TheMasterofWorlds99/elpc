# Multi-file compilation pattern

Real languages span many files. Here's the standard pattern for compiling multiple source files with ELPC.

## Pattern: shared state per pipeline stage

Each file goes through the pipeline independently, but later stages share state.

### 1. Shared symbol table

```cpp
// Collector phase — load all files, build symbol table
elpc::DiagnosticEngine diag;
elpc::SymbolTable<std::string, FileInfo> globalSymbols;
std::vector<elpc::SourceManager> sources;
std::vector<std::unique_ptr<elpc::BlockStmt>> asts;

for (auto &path : filePaths) {
  auto src = elpc::SourceManager::fromFile(path);
  auto lexer = createLexer(src);
  lexer.setDiagnostics(diag);
  auto tokens = lexer.tokenize();

  MyParser parser(tokens);
  parser.setDiagnostics(diag);
  auto ast = parser.parseProgram();

  // Register public symbols from this file
  collectPublicSymbols(*ast, globalSymbols, path);

  sources.push_back(std::move(src));
  asts.push_back(std::move(ast));
}

if (diag.hasErrors()) {
  diag.reportWithSource(std::cerr, sources[0]); // show first file's errors
  return 1;
}
```

### 2. Shared type context

```cpp
elpc::TypeContext types;

// First pass: register all type declarations
for (auto &ast : asts) {
  registerTypes(*ast, types, diag);
}

// Second pass: resolve type references and analyze
for (auto &ast : asts) {
  analyzeTypes(*ast, types, diag);
}
```

### 3. Linking

```cpp
// Generate a single IR module with all functions
elpc::IRModule irModule("my_program");

for (auto &ast : asts) {
  auto *fn = irModule.addFunction(funcName, returnType);
  codegenFunction(*ast, irModule, fn);
}

// Emit as a single C file or object
std::string cCode = irModule.emitC();
```

## Entry point convention

The standard convention: one file contains `main()` / the entry point. The other files contain library functions.

```cpp
// main.my
import "stdlib.my";
import "math.my";

fn main() -> int {
  print(greet("world"));
}
```
