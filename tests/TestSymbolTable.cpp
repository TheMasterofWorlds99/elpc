#include <elpc/core/table.hpp>
#include <cassert>
#include <iostream>
#include <string>

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testBasicDefineLookup() {
  elpc::SymbolTable<std::string, int> table;

  CHECK(table.define("x", 42), "define x should succeed");
  CHECK(table.define("y", 99), "define y should succeed");

  auto vx = table.lookup("x");
  CHECK(vx.has_value(), "lookup x should find it");
  CHECK(vx.value() == 42, "x should be 42");

  auto vy = table.lookup("y");
  CHECK(vy.has_value(), "lookup y should find it");
  CHECK(vy.value() == 99, "y should be 99");

  auto vz = table.lookup("z");
  CHECK(!vz.has_value(), "lookup z should not find it");

  std::cout << "  PASS basicDefineLookup\n";
  return 0;
}

static int testScopePushPop() {
  elpc::SymbolTable<std::string, int> table;

  (void)table.define("a", 1);
  CHECK(table.depth() == 1, "depth should be 1 after construction + define");

  table.pushScope();
  CHECK(table.depth() == 2, "depth should be 2 after pushScope");

  (void)table.define("b", 2);
  CHECK(table.lookup("a").value() == 1, "inner scope sees outer 'a'");
  CHECK(table.lookup("b").value() == 2, "inner scope sees inner 'b'");

  table.popScope();
  CHECK(table.depth() == 1, "depth should be 1 after popScope");
  CHECK(table.lookup("a").value() == 1, "outer scope still has 'a'");
  CHECK(!table.lookup("b").has_value(), "outer scope should not see inner 'b'");

  std::cout << "  PASS scopePushPop\n";
  return 0;
}

static int testShadowing() {
  elpc::SymbolTable<std::string, int> table;

  (void)table.define("x", 10);
  table.pushScope();

  CHECK(table.define("x", 20), "inner define of x should succeed");
  CHECK(table.lookup("x").value() == 20, "inner lookup sees shadowed value");

  table.popScope();
  CHECK(table.lookup("x").value() == 10, "after pop, outer x is restored");

  std::cout << "  PASS shadowing\n";
  return 0;
}

static int testDuplicateDefine() {
  elpc::SymbolTable<std::string, int> table;

  CHECK(table.define("x", 1), "first define should succeed");
  CHECK(!table.define("x", 2), "duplicate define in same scope should fail");
  CHECK(table.lookup("x").value() == 1, "value should remain 1");

  std::cout << "  PASS duplicateDefine\n";
  return 0;
}

static int testDefineOrReplace() {
  elpc::SymbolTable<std::string, int> table;

  (void)table.define("x", 1);
  table.defineOrReplace("x", 99);
  CHECK(table.lookup("x").value() == 99, "defineOrReplace should overwrite");

  std::cout << "  PASS defineOrReplace\n";
  return 0;
}

static int testLookupCurrent() {
  elpc::SymbolTable<std::string, int> table;

  (void)table.define("a", 1);
  table.pushScope();
  (void)table.define("b", 2);

  CHECK(table.lookupCurrent("a").has_value() == false, "lookupCurrent should not see outer scope");
  CHECK(table.lookupCurrent("b").has_value(), "lookupCurrent should see current scope");

  std::cout << "  PASS lookupCurrent\n";
  return 0;
}

static int testReset() {
  elpc::SymbolTable<std::string, int> table;

  (void)table.define("x", 42);
  table.pushScope();
  (void)table.define("y", 99);

  table.reset();
  CHECK(table.depth() == 1, "reset should leave one scope");
  CHECK(!table.lookup("x").has_value(), "reset should clear all symbols");

  std::cout << "  PASS reset\n";
  return 0;
}

static int testPopGlobalScope() {
  elpc::SymbolTable<std::string, int> table;

  try {
    table.popScope();
    CHECK(false, "popScope on global scope should throw");
  } catch (const std::runtime_error &) {
    // expected
  }

  std::cout << "  PASS popGlobalScope\n";
  return 0;
}

static int testIsDefined() {
  elpc::SymbolTable<std::string, int> table;

  (void)table.define("x", 1);
  CHECK(table.isDefined("x"), "isDefined on existing symbol");
  CHECK(!table.isDefined("y"), "isDefined on missing symbol");

  table.pushScope();
  CHECK(table.isDefined("x"), "isDefined sees outer scope");
  CHECK(!table.isDefinedCurrent("x"), "isDefinedCurrent does not see outer scope");

  std::cout << "  PASS isDefined\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testBasicDefineLookup();
  failures += testScopePushPop();
  failures += testShadowing();
  failures += testDuplicateDefine();
  failures += testDefineOrReplace();
  failures += testLookupCurrent();
  failures += testReset();
  failures += testPopGlobalScope();
  failures += testIsDefined();

  if (failures == 0)
    std::cout << "All TestSymbolTable tests passed!\n";
  else
    std::cerr << failures << " TestSymbolTable test(s) failed!\n";

  return failures;
}
