# Core module

The core module provides the foundation types used by every stage of the pipeline.

## `SourceLocation` (`elpc/core/loc.hpp`)

Represents a position in source code: line, column, and filename.

```cpp
elpc::SourceLocation loc{1, 5, "test.elpc"};
loc.line;   // 1
loc.column; // 5
```

## `SourceManager` (`elpc/core/sourceManager.hpp`)

Owns the source text and provides zero-copy string slices and O(log n) line/column lookups. All tokens reference memory owned by the SourceManager — it must outlive any tokens created from it.

```cpp
// From a string
auto src = elpc::SourceManager::fromString("test.elpc", "let x = 42;");

// From a file
auto src = elpc::SourceManager::fromFile("program.lang");

// Zero-copy slice
auto slice = src.slice(4, 5);  // string_view, no allocation

// Location lookup (O(log n))
auto loc = src.location(10);  // byte offset → line/column

// Null-terminated C string
const char *raw = src.c_str();
```

## `Token<TokenType>` (`elpc/core/token.hpp`)

A token is a (type, lexeme, location) triple. The lexeme is a `std::string_view` pointing into a SourceManager — no per-token allocations.

```cpp
enum class MyToken { INT, PLUS, IDENT };

elpc::Token<MyToken> tok(MyToken::INT, "42", {1, 1});

tok.is(MyToken::INT);          // true
tok.isNot(MyToken::PLUS);      // true
tok.isOneOf(MyToken::INT, MyToken::PLUS);  // true

tok.lexeme;     // "42" (string_view)
tok.location;   // SourceLocation

tok.toInt();    // std::optional<int>{42}
tok.toFloat();  // std::optional<float>
tok.toDouble(); // std::optional<double>
```

## `TokenReader<TokenType>` (`elpc/core/tokenReader.hpp`)

Cursor over a vector of tokens. Used internally by the Parser.

```cpp
elpc::TokenReader<MyToken> reader(tokens);
reader.peek();       // current token
reader.peek(1);      // lookahead
reader.consume();    // advance and return previous
reader.check(MyToken::INT);  // peek and compare
reader.match(MyToken::INT);  // consume if matches
reader.isAtEnd();
```

## `SymbolTable<Key, Value>` (`elpc/core/table.hpp`)

Scoped symbol table with push/pop scope. Useful for semantic analysis.

```cpp
elpc::SymbolTable<std::string, int> table;

table.define("x", 42);               // returns false if already defined
table.lookup("x");                    // std::optional<int>{42}
table.isDefined("x");                 // true

table.pushScope();
table.define("y", 99);                // inner scope
table.lookup("x");                    // still finds outer x
table.lookupCurrent("y");             // only current scope
table.popScope();

table.defineOrReplace("x", 100);      // overwrites existing
table.reset();                        // clear all scopes
```
