# ELPC — Easy Lexer & Parser Creation

**A modern, header-only C++20 library for building compilers and interpreters.**

ELPC provides composable, zero-boilerplate building blocks for every stage
of a language pipeline — from source files to compiled output — in a single
`#include <elpc/elpc.hpp>`.

```cpp
#include <elpc/elpc.hpp>

enum class Token { INT, PLUS };

int main() {
    auto src = elpc::SourceManager::fromString("calc", "1 + 2");

    elpc::Lexer<Token> lexer(src);
    lexer.addRule(Token::INT,  "\\b[0-9]+\\b");
    lexer.addRule(Token::PLUS, "\\+");
    lexer.addSkip("\\s+");

    for (auto &t : lexer.tokenize())
        std::cout << t << "\n";
}
```

## Pipeline

```
 Source (.hlox, .my, .lox)
    │
    ▼ SourceManager      — zero-copy file loading, line/col lookup
    ▼ Lexer              — NFA-simulation lexer, O(n), longest-match
    ▼ Parser             — recursive descent + Pratt operator-precedence
    ▼ AST                — 16 node types, visitor pattern, printer
    ▼ Transforms         — constant folding, desugaring via ASTTransform
    ▼ Semantic Analysis  — type checking, symbol resolution
    ▼ IR                 — basic blocks, instructions, C codegen
    ▼ LLVM (optional)    — native codegen via LLVM backend
```

## Modules

### Core — `elpc/core/`
| Component | Header | Purpose |
|-----------|--------|---------|
| `SourceLocation` | `loc.hpp` | Line, column, filename position |
| `SourceManager` | `sourceManager.hpp` | Owns source text, zero-copy slices, O(log n) line lookup |
| `Token<T>` | `token.hpp` | Typed token with string_view lexeme |
| `TokenReader<T>` | `tokenReader.hpp` | Cursor over token vector |
| `SymbolTable<K,V>` | `table.hpp` | Scoped symbol table with push/pop |
| `InternedString` | `internedString.hpp` | Interned strings, O(1) pointer comparison |

### Lexer — `elpc/lexer/`
| Component | Header | Purpose |
|-----------|--------|---------|
| `Lexer<T>` | `lexer.hpp` | NFA-based lexer, O(n), longest-match, pattern collisions handled |
| NFA engine | `dfa.hpp` | NFA simulation with bitset state tracking |

Pattern language: literals, `[a-z]`/`[^0-9]` classes, `+`/`*` quantifiers,
`\d`/`\w`/`\s` escapes, `.` wildcard, `(...)` groups, `\b` word boundaries.

### Parser — `elpc/parser/`
| Component | Header | Purpose |
|-----------|--------|---------|
| `Parser<T>` | `parser.hpp` | Recursive descent base with error recovery |
| `PrattParser<T,N>` | `prattParser.hpp` | Operator-precedence expression parsing |
| `Sema` | `sema.hpp` | Semantic analysis base class |

Diagnostic mode: attach a `DiagnosticEngine` to collect errors instead of
throwing, with panic-mode recovery that automatically skips to safe sync
points (`;`, `}`, etc.).

### AST — `elpc/ast/`
| Component | Header | Purpose |
|-----------|--------|---------|
| Node types | `ast.hpp` | 16 expression/statement nodes with source locations |
| `ASTVisitor<Ret>` | `ast.hpp` | Visitor pattern with NodeKind dispatch |
| `ASTPrinter` | `ast.hpp` | Indented debug tree output |
| `ASTTransform<D>` | `ast.hpp` | Recursive AST rewriting (clone + override) |
| `BumpAllocator` | `ast.hpp` | Arena allocator for cache-friendly AST construction |
| `ConstantFold` | `transform/constantFold.hpp` | Compile-time constant folding pass |

### Diagnostics — `elpc/diagnostics/`
| Component | Header | Purpose |
|-----------|--------|---------|
| `Diagnostic` | `diagnostic.hpp` | Severity, message, location |
| `DiagnosticEngine` | `diagnosticEngine.hpp` | Collect errors/warnings/notes, source-context display |

```cpp
diag.reportWithSource(std::cerr, src);
// [error]   [2:7] undefined variable 'bad_var'
//    │
// 2 │ print bad_var;
//    │       ^^^^^^^
```

### Semantic Analysis — `elpc/sema/`
| Component | Header | Purpose |
|-----------|--------|---------|
| `TypeContext` | `type.hpp` | Type deduplication, canonical types |
| `PrimitiveType` | `type.hpp` | void, bool, int, float, string |
| `FunctionType` | `type.hpp` | Parameter + return types |
| `ArrayType` | `type.hpp` | Element type |
| `NamedType` | `type.hpp` | User-defined types |

### IR & Codegen — `elpc/ir/`
| Component | Header | Purpose |
|-----------|--------|---------|
| `IRModule` | `ir.hpp` | IR with basic blocks, instructions, constants |
| `IRFuncBuilder` | `ir.hpp` | Builder for constructing IR functions |
| `IRBuilder<V>` | `irBuilder.hpp` | Text-based code generation template |
| `LLVMBridge` | `llvmBridge.hpp` | LLVM IR generation (LLVM 14+) |
| `LLVMIRTranslator` | `toLLVM.hpp` | IRModule → LLVM IR (LLVM 14+) |

## Quick Start (full pipeline)

```cpp
#include <elpc/elpc.hpp>

enum class Token { INT, PLUS, SEMI };
enum class NodeKind { LITERAL, BINARY, PROGRAM };

int main() {
    // 1. Load source
    auto src = elpc::SourceManager::fromString("test", "1 + 2;");

    // 2. Lex
    elpc::Lexer<Token> lexer(src);
    lexer.addRule(Token::INT,  "\\b[0-9]+\\b");
    lexer.addRule(Token::PLUS, "\\+");
    lexer.addRule(Token::SEMI, ";");
    lexer.addSkip("\\s+");
    auto tokens = lexer.tokenize();

    // 3. Print tokens
    for (auto &t : tokens)
        std::cout << t << "\n";

    // 4. Build IR and emit C
    elpc::IRModule mod("my_program");
    auto *fn = mod.addFunction("main", elpc::IRType::INT32);
    auto *block = fn->addBlock("entry");
    block->addInst(elpc::IROp::RET, elpc::IRType::VOID,
                   {mod.addConstInt(0)});
    std::cout << mod.emitC();
}
```

## Examples

| Example | Description |
|---------|-------------|
| `examples/mini/mini.cpp` | A complete imperative language → C (500 lines) |
| `examples/mini_ir/mini_ir.cpp` | Mini language through IR → C |
| `examples/repl/repl.cpp` | Interactive REPL |

```
cd build && cmake .. && make mini && ./mini examples/mini/test.mini
```

## Using in your project (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(elpc
    GIT_REPOSITORY https://github.com/YOUR_USERNAME/elpc.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(elpc)
target_link_libraries(YourProject PRIVATE elpc::elpc)
```

## Requirements

- **C++20** or later
- **CMake 3.16** or later
- **LLVM 14+** (optional, for LLVM backend): `cmake -DELPC_ENABLE_LLVM=ON ..`

## Building & Testing

```bash
git clone https://github.com/YOUR_USERNAME/elpc.git
cd elpc
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run all 81 tests
for t in Test*; do ./$t; done

# Try the Mini example
./mini ../examples/mini/test.mini
```

## Project Structure

```
include/elpc/
  core/       → loc, sourceManager, token, tokenReader, table, internedString
  lexer/      → lexer, dfa (NFA engine)
  parser/     → parser, prattParser, sema
  ast/        → ast (nodes, visitor, printer, transform, bumpAllocator)
    transform/ → constantFold
  diagnostics/ → diagnostic, diagnosticEngine
  sema/       → type
  ir/         → ir (IRModule, basic blocks), irBuilder, llvmBridge, toLLVM
  elpc.hpp    → umbrella header

examples/     → mini, mini_ir, repl
tools/        → elpc (CLI driver)
tests/        → 12 test binaries, 81 tests
docs/         → module docs, multi-file guide, hydrolox plan
```

## License

MIT — see [LICENSE](LICENSE).

## Author Notes
Hey! This is my first proper, usable C++ library that I made with the help of Claude, Gemini and Deepseek.
