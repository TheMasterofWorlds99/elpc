#include <elpc/parser/sema.hpp>
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

// A minimal semantic analyzer for testing
class TestAnalyzer : public elpc::Sema {
public:
  TestAnalyzer(elpc::DiagnosticEngine &diag) : elpc::Sema(diag) {}

  void checkPositive(int value, elpc::SourceLocation loc = {}) {
    if (value < 0)
      error("value must be positive", loc);
    else if (value == 0)
      warning("value is zero", loc);
  }

  void reportDeclared(const std::string &name, elpc::SourceLocation loc = {}) {
    note("declared '" + name + "'", loc);
  }
};

static int testErrorRouting() {
  elpc::DiagnosticEngine diag;
  TestAnalyzer analyzer(diag);

  analyzer.checkPositive(-5, {1, 1});

  CHECK(diag.hasErrors(), "should have errors after negative value");
  CHECK(diag.count() == 1, "should have 1 diagnostic");

  std::ostringstream out;
  diag.reportDiagnostics(out);
  CHECK(out.str().find("value must be positive") != std::string::npos,
        "should report the error message");

  std::cout << "  PASS errorRouting\n";
  return 0;
}

static int testWarningRouting() {
  elpc::DiagnosticEngine diag;
  TestAnalyzer analyzer(diag);

  analyzer.checkPositive(0, {2, 5});

  CHECK(!diag.hasErrors(), "should not have errors");
  CHECK(diag.count() == 1, "should have 1 diagnostic");

  std::ostringstream out;
  diag.reportDiagnostics(out);
  CHECK(out.str().find("[warning]") != std::string::npos,
        "should be a warning");

  std::cout << "  PASS warningRouting\n";
  return 0;
}

static int testNoteRouting() {
  elpc::DiagnosticEngine diag;
  TestAnalyzer analyzer(diag);

  analyzer.reportDeclared("myVar", {3, 10});

  CHECK(diag.count() == 1, "should have 1 diagnostic");

  std::ostringstream out;
  diag.reportDiagnostics(out);
  CHECK(out.str().find("[note]") != std::string::npos,
        "should be a note");
  CHECK(out.str().find("myVar") != std::string::npos,
        "should reference variable name");

  std::cout << "  PASS noteRouting\n";
  return 0;
}

static int testHasErrors() {
  elpc::DiagnosticEngine diag;
  TestAnalyzer analyzer(diag);

  CHECK(!analyzer.hasErrors(), "no errors initially");

  analyzer.checkPositive(-1, {});
  CHECK(analyzer.hasErrors(), "has errors after error");
  CHECK(diag.hasErrors(), "diag also has errors");

  std::cout << "  PASS hasErrors\n";
  return 0;
}

static int testEngineAccess() {
  elpc::DiagnosticEngine diag;
  TestAnalyzer analyzer(diag);

  auto &engine = analyzer.engine();
  engine.error("direct error via engine", {});

  CHECK(diag.count() == 1, "engine access works");

  std::cout << "  PASS engineAccess\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testErrorRouting();
  failures += testWarningRouting();
  failures += testNoteRouting();
  failures += testHasErrors();
  failures += testEngineAccess();

  if (failures == 0)
    std::cout << "All TestSema tests passed!\n";
  else
    std::cerr << failures << " TestSema test(s) failed!\n";

  return failures;
}
