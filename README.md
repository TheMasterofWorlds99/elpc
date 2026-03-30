# ELPC - Easy Lexer & Parser Creation

A modern, header only C++20 library for building compilers and interpreters. elpc provides composable, zero-boilerplate building blocks for every stage of a langauge pipeline.

## Modules

| Module | Header | Description |
|---|---|---|
| Core | `elpc/core/token.hpp` | Token, SourceLocation, TokenReader |
| Lexer | `elpc/lexer/lexer.hpp` | Regex-based lexer with skip rules |
| Parser | `elpc/parser/parser.hpp` | Recursive descent base class |
| Pratt | `elpc/parser/prattParser.hpp` | Pratt expression parser |
| Sema | `elpc/parser/sema.hpp` | Semantic analysis base class |
| Diagnostics | `elpc/diagnostics/diagnosticEngine.hpp` | Structured error reporting |
| Table | `elpc/core/table.hpp` | Scoped symbol table |
| IR | `elpc/ir/irBuilder.hpp` | Generic IR/code generation base |
| LLVMBridge | `elpc/ir/llvmBridge.hpp` | Brige Class for connecting to LLVM IR |

Include everything with:

```cpp
#include <elpc/elpc.hpp>
```

## Quick Start

```cpp
#include <elpc/elpc.hpp>

enum class TokenType { EXIT, INT_LIT };

int main() {
    elpc::Lexer lexer("exit 0");
    lexer.addRule(TokenType::EXIT,    "\\bexit\\b");
    lexer.addRule(TokenType::INT_LIT, "\\b[0-9]+\\b");
    lexer.addSkip("\\s+");

    auto tokens = lexer.tokenize();
    for (const auto &t : tokens)
        std::cout << t << "\n";
}
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

- C++20 or later
- CMake 3.16 or later
- LLVM 14+ (optional, for LLVM backend)

## Optional LLVM backend

```bash
cmake -DELPC_ENABLE_LLVM=ON ..
```

This is my first proper, usable C++ library which was built with the assistance of Claude and Gemini. I hope you can find this useful for whatever projects you plan to make
