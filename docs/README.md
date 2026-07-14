# ELPC Documentation

ELPC is a header-only C++20 library for building compilers and interpreters. It provides composable building blocks for every stage of a language pipeline.

## Modules

| Module | File | Purpose |
|--------|------|---------|
| [Core](core.md) | `elpc/core/*.hpp` | Source locations, tokens, symbol tables, source management |
| [Lexer](lexer.md) | `elpc/lexer/*.hpp` | DFA-based lexer engine |
| [Parser](parser.md) | `elpc/parser/*.hpp` | Recursive descent + Pratt parsing |
| [AST](ast.md) | `elpc/ast/ast.hpp` | AST nodes, visitor pattern, printer |
| [Sema](sema.md) | `elpc/parser/sema.hpp` | Semantic analysis base class |
| [Diagnostics](diagnostics.md) | `elpc/diagnostics/*.hpp` | Error and warning reporting |
| [IR](ir.md) | `elpc/ir/*.hpp` | Code generation backends |

## Quick links

- [Example language: Mini](../examples/mini/mini.cpp)
- [Test suite](../tests/)
