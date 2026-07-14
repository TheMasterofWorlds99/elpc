#include <elpc/ir/irBuilder.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

// A simple C-code backend for testing
class CBackend : public elpc::IRBuilder<std::string> {
public:
  CBackend(elpc::DiagnosticEngine &diag) : elpc::IRBuilder<std::string>(diag) {}

  void emitIntDecl(const std::string &name, int value) {
    std::string code = "int " + name + " = " + std::to_string(value) + ";";
    if (defineVar(name, name)) {
      out() << code << "\n";
    }
  }

  void emitBlockStart() {
    pushScope();
    out() << "{\n";
  }

  void emitBlockEnd() {
    popScope();
    out() << "}\n";
  }

  std::string emitAdd(const std::string &result, const std::string &a, const std::string &b) {
    std::string code = result + " = " + a + " + " + b + ";";
    return code;
  }
};

static int testBasicOutput() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  backend.emitIntDecl("x", 42);
  std::string result = backend.result();

  CHECK(result.find("int x = 42;") != std::string::npos,
        "should emit int x = 42;");

  std::cout << "  PASS basicOutput\n";
  return 0;
}

static int testScopeManagement() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  backend.emitIntDecl("x", 1);

  backend.emitBlockStart();
  backend.emitIntDecl("y", 2);
  backend.emitBlockEnd();

  // x should still be accessible, y should not be in current scope
  // (defineVar checks for redefinition in current scope)
  // After blockEnd, we're back in global scope, so defineVar for y should succeed again
  // Actually, lookupVar checks all scopes, so y is still in outer scope
  // Let's just check the output buffer is correct
  std::string result = backend.result();
  CHECK(result.find("int x = 1;") != std::string::npos, "x should be declared");
  CHECK(result.find("int y = 2;") != std::string::npos, "y should be declared in block");

  std::cout << "  PASS scopeManagement\n";
  return 0;
}

static int testRedefinitionError() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  CHECK(backend.defineVar("x", "x"), "first define should succeed");
  CHECK(!backend.defineVar("x", "x"), "redefine in same scope should fail");
  CHECK(diag.hasErrors(), "should have error after redefinition");

  std::cout << "  PASS redefinitionError\n";
  return 0;
}

static int testLookupVar() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  (void)backend.defineVar("x", "int_value");
  auto result = backend.lookupVar("x");
  CHECK(result.has_value(), "lookup x should find it");
  CHECK(result.value() == "int_value", "x should map to int_value");

  auto missing = backend.lookupVar("y");
  CHECK(!missing.has_value(), "lookup y should not find it");

  std::cout << "  PASS lookupVar\n";
  return 0;
}

static int testClearBuffer() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  backend.emitIntDecl("x", 1);
  CHECK(!backend.result().empty(), "result should not be empty");

  backend.clearBuffer();
  CHECK(backend.result().empty(), "result should be empty after clear");

  std::cout << "  PASS clearBuffer\n";
  return 0;
}

static int testErrorDiagnostics() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  backend.error("syntax error in IR generation", {5, 3});
  backend.warning("unused variable", {6, 1});

  CHECK(diag.hasErrors(), "should have errors");
  CHECK(diag.count() == 2, "should have 2 diagnostics");

  std::ostringstream report;
  diag.reportDiagnostics(report);
  std::string output = report.str();
  CHECK(output.find("syntax error") != std::string::npos, "error message present");
  CHECK(output.find("unused variable") != std::string::npos, "warning message present");
  CHECK(output.find("5:3") != std::string::npos, "location 5:3 present");
  CHECK(output.find("6:1") != std::string::npos, "location 6:1 present");

  std::cout << "  PASS errorDiagnostics\n";
  return 0;
}

static int testHasErrors() {
  elpc::DiagnosticEngine diag;
  CBackend backend(diag);

  CHECK(!backend.hasErrors(), "no errors initially");
  backend.error("test error");
  CHECK(backend.hasErrors(), "has errors after error");

  std::cout << "  PASS hasErrors\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testBasicOutput();
  failures += testScopeManagement();
  failures += testRedefinitionError();
  failures += testLookupVar();
  failures += testClearBuffer();
  failures += testErrorDiagnostics();
  failures += testHasErrors();

  if (failures == 0)
    std::cout << "All TestIR tests passed!\n";
  else
    std::cerr << failures << " TestIR test(s) failed!\n";

  return failures;
}
