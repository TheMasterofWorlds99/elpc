#include <elpc/diagnostics/diagnosticEngine.hpp>
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

static int testBasicDiagnostics() {
  elpc::DiagnosticEngine diag;

  CHECK(diag.empty(), "should start empty");
  CHECK(!diag.hasErrors(), "should not have errors initially");

  diag.error("something broke", {1, 2});
  CHECK(!diag.empty(), "should not be empty after error");
  CHECK(diag.hasErrors(), "should have errors after error");
  CHECK(diag.count() == 1, "count should be 1");

  diag.warning("be careful", {3, 4});
  CHECK(diag.count() == 2, "count should be 2 after warning");

  diag.note("for your information", {5, 6});
  CHECK(diag.count() == 3, "count should be 3 after note");

  std::cout << "  PASS basicDiagnostics\n";
  return 0;
}

static int testClear() {
  elpc::DiagnosticEngine diag;

  diag.error("error");
  CHECK(!diag.empty(), "not empty after error");
  CHECK(diag.hasErrors(), "has errors after error");

  diag.clear();
  CHECK(diag.empty(), "should be empty after clear");
  CHECK(!diag.hasErrors(), "should not have errors after clear");

  std::cout << "  PASS clear\n";
  return 0;
}

static int testReportOutput() {
  elpc::DiagnosticEngine diag;

  diag.error("undefined variable 'x'", {10, 5});
  diag.warning("unused variable 'y'", {12, 3});
  diag.note("declared here", {5, 1});

  std::ostringstream out;
  diag.reportDiagnostics(out);
  std::string output = out.str();

  CHECK(output.find("[error]") != std::string::npos, "output should contain [error]");
  CHECK(output.find("[warning]") != std::string::npos, "output should contain [warning]");
  CHECK(output.find("[note]") != std::string::npos, "output should contain [note]");
  CHECK(output.find("undefined variable") != std::string::npos, "output should contain error message");
  CHECK(output.find("10:5") != std::string::npos, "output should contain location 10:5");
  CHECK(output.find("12:3") != std::string::npos, "output should contain location 12:3");
  CHECK(output.find("5:1") != std::string::npos, "output should contain location 5:1");

  std::cout << "  PASS reportOutput\n";
  return 0;
}

static int testFilenameInReport() {
  elpc::DiagnosticEngine diag;

  diag.error("file not found", {1, 1, "test.elpc"});

  std::ostringstream out;
  diag.reportDiagnostics(out);
  std::string output = out.str();

  CHECK(output.find("test.elpc") != std::string::npos,
        "output should contain filename");

  std::cout << "  PASS filenameInReport\n";
  return 0;
}

static int testAllReturns() {
  elpc::DiagnosticEngine diag;

  const auto &all = diag.all();
  CHECK(all.empty(), "all() should return empty initially");

  diag.error("err", {});
  diag.warning("warn", {});

  const auto &all2 = diag.all();
  CHECK(all2.size() == 2, "all() should contain 2 diagnostics");
  CHECK(all2[0].severity == elpc::Severity::ERROR, "first should be error");
  CHECK(all2[1].severity == elpc::Severity::WARNING, "second should be warning");
  CHECK(all2[0].message == "err", "first message should be 'err'");
  CHECK(all2[1].message == "warn", "second message should be 'warn'");

  std::cout << "  PASS allReturns\n";
  return 0;
}

static int testReportWithSource() {
  elpc::DiagnosticEngine diag;

  auto src = elpc::SourceManager::fromString("test",
    "let x = 42;\n"
    "print bad_var;\n"
    "return 0;\n"
  );

  diag.error("undefined variable 'bad_var'", {2, 7});

  std::ostringstream out;
  diag.reportWithSource(out, src);
  std::string output = out.str();

  // Should contain header
  CHECK(output.find("[error]") != std::string::npos, "output should contain [error]");
  CHECK(output.find("undefined variable") != std::string::npos, "output should contain message");
  CHECK(output.find("2:7") != std::string::npos, "output should contain location");

  // Should contain source context
  CHECK(output.find("bad_var") != std::string::npos, "output should show the source line");
  CHECK(output.find("^") != std::string::npos, "output should have a caret");
  CHECK(output.find("print bad_var;") != std::string::npos, "output should contain the error line");

  std::cout << "  PASS reportWithSource\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testBasicDiagnostics();
  failures += testClear();
  failures += testReportOutput();
  failures += testFilenameInReport();
  failures += testAllReturns();
  failures += testReportWithSource();

  if (failures == 0)
    std::cout << "All TestDiagnostics tests passed!\n";
  else
    std::cerr << failures << " TestDiagnostics test(s) failed!\n";

  return failures;
}
