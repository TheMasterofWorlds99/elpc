# Lexer module

The lexer compiles regex-like patterns into a deterministic finite automaton (DFA) and tokenises source text in O(n) time — one table lookup per input byte.

## `Lexer<TokenType>` (`elpc/lexer/lexer.hpp`)

### Pattern language

The DFA engine supports a subset of regex:

| Pattern | Meaning |
|---------|---------|
| `exit` | Exact characters |
| `[0-9]+` | One or more digits |
| `[a-zA-Z_][a-zA-Z0-9_]*` | Identifier with optional suffix |
| `\d+` | One or more digits (predefined class) |
| `\s+` | One or more whitespace chars |
| `\w+` | One or more word chars (`[a-zA-Z0-9_]`) |
| `\bkeyword\b` | Word boundary (handled by longest-match) |
| `\n` | Newline |
| `\(` | Escaped literal `(` |
| `//[^\n]*` | Line comment |
| `\|\|` | Alternation (in pattern) |
| `[^0-9]` | Negated character class |
| `.` | Any single character |
| `(...)` | Grouping |

Rules use **longest-match disambiguation**: when two rules match the same length, the one added first wins. Keywords added before identifiers ensures keywords take priority.

### Usage

```cpp
enum class Token { IF, INT, IDENT, PLUS, MINUS, STAR, SEMI };

auto src = elpc::SourceManager::fromString("test", "if (x + 42)");
elpc::Lexer<Token> lexer(src);

// Keywords first (higher priority)
lexer.addRule(Token::IF, "\\bif\\b");

// Then catch-all patterns
lexer.addRule(Token::IDENT, "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b");
lexer.addRule(Token::INT,  "\\b[0-9]+\\b");
lexer.addRule(Token::PLUS, "\\+");
lexer.addRule(Token::MINUS, "-");
lexer.addRule(Token::STAR, "\\*");

// Skip whitespace
lexer.addSkip("\\s+");

auto tokens = lexer.tokenize();
```

### Diagnostic mode

By default the lexer throws on unrecognized characters. Attach a DiagnosticEngine to collect errors and continue:

```cpp
elpc::DiagnosticEngine diag;
lexer.setDiagnostics(diag);
auto tokens = lexer.tokenize();  // reports errors, skips bad chars
if (diag.hasErrors()) { /* handle */ }
```

### Multiple tokenize calls

```cpp
lexer.setSource(newSource);  // swap source, reset position
auto tokens = lexer.tokenize();
```

## DFA engine (`elpc/lexer/dfa.hpp`)

The DFA is an implementation detail — you don't use it directly. It compiles all rules into a single transition table on the first `tokenize()` call and runs in O(n) time.
