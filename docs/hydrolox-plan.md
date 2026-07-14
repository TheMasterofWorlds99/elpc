# Hydrolox (.hlox) — Language Plan

A compiled, high-performance, low-level language built with ELPC.
Designed for scientific computing, systems programming, and GPU workloads.
Part of the Lox language family (hlox, klox, mlox, ...).

---

## 1. Design Philosophy

**Faster than C++** — No hidden costs, no vtable unless you ask, no exceptions,
no RTTI. Zero-overhead abstractions. The programmer controls the machine.

**Safer than C++** — Bounds-checked slices, explicit initialization, no implicit
conversions that lose data, sane default behavior. Not Rust-level strict —
no borrow checker, no lifetime annotations. Pragmatic safety.

**GPU-native** — Vectors (vec2/3/4, dvec2/3/4, ivec2/3/4) are first-class types.
Kernel syntax for GPU compute. The type system knows about GPU memory spaces.

**Scientific computing** — SIMD-friendly data layouts, matrix types,
built-in math intrinsics, AoS → SoA with a keyword.

**The Lox family** — hlox is the metal sibling. klox might be the scripting
sibling, mlox the ML-inspired one. Shared runtime, shared IR, shared tooling.

---

## 2. Syntax (drawn from your parser)

```
// Function:     ret-type fn-name(params) -> ret-type { body }
i32 fn add(i32: a, i32: b) {
    return a + b;
}

// Extern FFI
extern fn void c_sin(f64: x);

// Variables:    name: type = expr;
x: i32 = 42;

// GPU vectors — first-class types
v: vec4 = {1.0, 0.0, 0.0, 1.0};
d: dvec2 = {3.14, 2.71};

// Arrays
arr: i32[4] = {1, 2, 3, 4};

// Structs
struct Point {
    x: f64;
    y: f64;
}
p: Point = {x: 1.0, y: 2.0};
p.x;  // member access

// Control flow
if (x > 0) { }
while (true) { }
for (i: i32 = 0; i < 10; i = i + 1) { }

// Array indexing, struct literals, casts
arr[0];
(i32)3.14;  // cast
```

---

## 3. Type System

### Primitives
| Type | Size | Purpose |
|------|------|---------|
| `u8` | 1B | Byte, unsigned |
| `u16` | 2B | Unsigned short |
| `i32` | 4B | Default integer |
| `i64` | 8B | Large integer |
| `f32` | 4B | Float |
| `f64` | 8B | Double (default float) |
| `bool` | 1B | Boolean |
| `string`/`str` | 16B | String view / owned |

### GPU vector types
| Type | Components | Element | Use case |
|------|-----------|---------|---------|
| `vec2/3/4` | 2/3/4 × f32 | `f32` | Graphics, compute |
| `dvec2/3/4` | 2/3/4 × f64 | `f64` | Scientific GPU |
| `ivec2/3/4` | 2/3/4 × i32 | `i32` | Compute indices |

### Compound types
| Syntax | Meaning |
|--------|---------|
| `i32[4]` | Fixed-size array (4 × i32) |
| `vec<4, f32>` | Generic vector |
| `struct Point { ... }` | Named struct |
| `{x: i32, y: i32}` | Struct literal |
| `&i32` | Pointer to i32 |

---

## 4. Pipeline Architecture (built with ELPC)

```
┌─────────────┐
│  .hlox file │
└──────┬──────┘
       │ SourceManager (zero-copy file load)
       ▼
┌─────────────┐
│   Lexer     │ ← elpc::Lexer with NFA engine
│             │   Tokens are string_views into source
└──────┬──────┘
       │ tokens
       ▼
┌─────────────┐
│   Parser    │ ← elpc::PrattParser for expressions
│             │   Recursive descent for declarations
│             │   elpc::DiagnosticEngine for error recovery
│             │   Produces elpc::ast nodes + custom types
└──────┬──────┘
       │ AST
       ▼
┌─────────────┐
│   Sema      │ ← Custom semantic analysis
│             │   Type checking, symbol resolution
│             │   elpc::SymbolTable for scopes
│             │   elpc::TypeContext for type dedup
│             │   elpc::InternedStringPool for names
└──────┬──────┘
       │ typed AST
       ▼
┌─────────────┐
│  Transforms │ ← elpc::ASTTransform passes
│             │   Constant folding
│             │   Desugaring (for → while, ++ → +=)
│             │   GPU kernel outlining
│             │   AoS → SoA transformation
└──────┬──────┘
       │ optimized AST
       ▼
┌─────────────┐
│  IR Lower   │ ← Custom AST → IRModule lowering
│             │   Basic blocks, instructions
│             │   SSA-like values
└──────┬──────┘
       │ IRModule
       ▼
┌──────────────────────┬──────────────────────┐
│   C Codegen          │   LLVM Codegen        │
│   (IRModule::emitC)  │   (LLVMIRTranslator)  │
│         │            │         │             │
│   out.c → gcc -O3    │   .o / .s / executable│
└──────────────────────┴──────────────────────┘
```

### ELPC modules used at each stage

| Stage | ELPC component | What it provides |
|-------|---------------|-----------------|
| Input | `SourceManager` | Zero-copy file loading, line/col lookup |
| Lexing | `Lexer<Token>` | NFA-based, O(n), handles pattern collisions |
| Parsing | `PrattParser<T, N>` | Operator-precedence expression parsing |
| Errors | `DiagnosticEngine` | Structured error collection, source-context display |
| Scopes | `SymbolTable<K,V>` | Push/pop scoped symbol storage |
| Types | `TypeContext` | Type deduplication, canonical types |
| Names | `InternedStringPool` | O(1) string comparison, deduplication |
| AST | `ast.hpp` nodes | 16 expression/statement types, source locations |
| Transforms | `ASTTransform<D>` | Recursive AST rewriting, cloning |
| Folding | `ConstantFold` | Compile-time expression evaluation |
| Memory | `BumpAllocator` | Arena allocation for AST nodes |
| IR | `IRModule` | Basic blocks, instructions, C emission |
| LLVM | `LLVMBridge` + `toLLVM.hpp` | Native codegen via LLVM |
| Drivers | `elpc-cli` | CLI tool with `--dump-ir`, `--tokens`, `--ast` |

---

## 5. Your old parser — what it tells us

Your old `parser.hpp` shows you already solved most of the hard expression problems:

- **Pratt parser** for expressions — correct, handled precedence well
- **GPU vector types** — `vec2/3/4`, `dvec2/3/4`, `ivec2/3/4` with proper type parsing
- **Generic vectors** — `vec<4, f32>` syntax for arbitrary-size vectors
- **Struct literals** — `{x: 1.0, y: 2.0}` syntax with field names
- **Postfix ++/--** — desugared to `x = x ± 1`
- **Type casting** — `(i32)3.14` with proper type parsing inside parens
- **Extern FFI** — full extern function declarations with variadic support
- **Member access and assignment** — `obj.field` and `obj.field = val`
- **Array types** — `i32[4]` with size, nesting `vec3[8]`
- **Function declarations** — with return types, params, blocks

What you'll change starting fresh with ELPC's full toolkit:

| Old approach | New approach |
|-------------|-------------|
| `std::unique_ptr<AST::Expr>` | `elpc::BumpAllocator` + typed AST nodes |
| Manual error throwing | `elpc::DiagnosticEngine` with recovery |
| Custom AST types from scratch | Extend `elpc::ast` nodes, add Hydrolox-specific ones |
| Manual string handling | `elpc::InternedStringPool` |
| No IR stage | `elpc::IRModule` → C or LLVM |

---

## 6. Implementation order

### Phase 1 — Core language (MVP)
| Step | What | ELPC tools |
|------|------|-----------|
| 1 | Token types + lexer rules | `Lexer<Token>` |
| 2 | Expression parser (Pratt) | `PrattParser<Token, Node*>` |
| 3 | Statement parser (recursive descent) | `Parser<Token>` |
| 4 | AST types for Hydrolox | Extend `elpc::ast` |
| 5 | Semantic analysis + symbol table | `SymbolTable`, `TypeContext` |
| 6 | IR lowering | `IRModule`, `IRFuncBuilder` |
| 7 | C codegen | `IRModule::emitC()` |

### Phase 2 — GPU support
| Step | What |
|------|------|
| 1 | Vector types in type system |
| 2 | Kernel syntax (`kernel fn ...`) |
| 3 | GPU memory qualifiers (`__global`, `__local`, `__private`) |
| 4 | AoS → SoA transform pass |
| 5 | LLVM codegen with vector intrinsics |

### Phase 3 — Lox family infrastructure
| Step | What |
|------|------|
| 1 | Shared IR for all Lox languages |
| 2 | Common runtime library |
| 3 | Interop ABI between Lox languages |
| 4 | `klox`, `mlox` frontends |

---

## 7. Your existing codebase

The files you already have live alongside ELPC:

```
hydrolox/
├── include/hydrolox/
│   ├── lexer.hpp        ← Port to elpc::Lexer
│   ├── parser.hpp       ← Your old parser (reference)
│   ├── ast.hpp          ← Hydrolox-specific AST extensions
│   ├── sema.hpp         ← Type checker
│   └── codegen/         ← IR lowering
├── examples/
└── tests/
```

ELPC handles the heavy lifting — your code defines *only* the language-specific
parts: token types, grammar rules, type-checking logic, and codegen.
