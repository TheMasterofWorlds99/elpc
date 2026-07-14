# IR / Codegen module

Base classes for code generation backends.

## `IRBuilder<ValueType>` (`elpc/ir/irBuilder.hpp`)

Base class for generating code in any output format. Subclass it and implement your emit methods. The base provides variable storage, scope management, diagnostics, and an output buffer.

```cpp
class CBackend : public elpc::IRBuilder<std::string> {
public:
  CBackend(elpc::DiagnosticEngine &diag)
    : elpc::IRBuilder<std::string>(diag) {}

  std::string emitAdd(int lhs, int rhs) {
    return std::to_string(lhs + rhs);
  }

  std::string emitVar(const std::string &name, int value) {
    if (defineVar(name, name)) {
      out() << "int " << name << " = " << value << ";\n";
    }
    return name;
  }
};
```

### Provided API

| Method | Returns | Description |
|--------|---------|-------------|
| `pushScope()` | `void` | Enter a new scope |
| `popScope()` | `void` | Exit current scope |
| `defineVar(name, value, loc)` | `bool` | Define in current scope (false if duplicate) |
| `lookupVar(name)` | `optional<ValueType>` | Look up in all scopes |
| `hasErrors()` | `bool` | Whether any errors occurred |
| `error/warning/note(msg, loc)` | `void` | Diagnostic helpers |
| `engine()` | `DiagnosticEngine&` | Access the engine |
| `out()` | `ostringstream&` | Write output code |
| `result()` | `string` | Get all accumulated output |
| `clearBuffer()` | `void` | Reset the output buffer |

## `LLVMBridge` (`elpc/ir/llvmBridge.hpp`)

Requires `-DELPC_ENABLE_LLVM=ON` and LLVM 14+. Extends IRBuilder with LLVM IR generation.

```cpp
#ifdef ELPC_ENABLE_LLVM
#include <elpc/ir/llvmBridge.hpp>

llvm::LLVMContext ctx;
llvm::Module mod("my_module", ctx);
elpc::DiagnosticEngine diag;
elpc::LLVMBridge bridge(ctx, mod, diag);

// Create a function
bridge.beginFunction("main",
  llvm::Type::getInt32Ty(ctx),     // return type
  {});                              // no params

// Use the LLVM builder directly
bridge.getBuilder().CreateRet(
  llvm::ConstantInt::get(ctx, llvm::APInt(32, 42)));

bridge.endFunction();
bridge.dumpIR();
bridge.writeObject("output.o");
#endif
```
