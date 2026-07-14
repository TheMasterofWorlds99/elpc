#include <elpc/parser/parser.hpp>
#include <cassert>
#include <iostream>
#include <string>

enum class Tk { EXIT, INT_LIT, IDENT, LPAREN, RPAREN, SEMI, PLUS, MINUS, STAR, SLASH, EOF_ };

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

// A minimal parser to test the base class
class TestParser : public elpc::Parser<Tk> {
public:
  TestParser(const std::vector<elpc::Token<Tk>> &tokens)
    : elpc::Parser<Tk>(tokens) {}

  using elpc::Parser<Tk>::peek;
  using elpc::Parser<Tk>::consume;
  using elpc::Parser<Tk>::previous;
  using elpc::Parser<Tk>::check;
  using elpc::Parser<Tk>::match;
  using elpc::Parser<Tk>::isAtEnd;
  using elpc::Parser<Tk>::matchAny;
  using elpc::Parser<Tk>::expect;
  using elpc::Parser<Tk>::synchronize;
};

static std::vector<elpc::Token<Tk>> makeTokens() {
  return {
    {Tk::EXIT, "exit"},
    {Tk::INT_LIT, "42"},
    {Tk::PLUS, "+"},
    {Tk::INT_LIT, "7"},
    {Tk::SEMI, ";"},
    {Tk::INT_LIT, "99"},
  };
}

static int testPeekAndConsume() {
  auto tokens = makeTokens();
  TestParser parser(tokens);

  CHECK(parser.peek().is(Tk::EXIT), "first peek should be EXIT");

  auto &tok = parser.consume();
  CHECK(tok.is(Tk::EXIT), "consumed token should be EXIT");

  CHECK(parser.peek().is(Tk::INT_LIT), "after consume, peek should be INT_LIT");
  CHECK(parser.previous().is(Tk::EXIT), "previous should be EXIT");

  std::cout << "  PASS peekAndConsume\n";
  return 0;
}

static int testCheckAndMatch() {
  auto tokens = makeTokens();
  TestParser parser(tokens);

  CHECK(parser.check(Tk::EXIT), "check EXIT should be true");
  CHECK(!parser.check(Tk::INT_LIT), "check INT_LIT should be false");

  CHECK(parser.match(Tk::EXIT), "match EXIT should succeed");
  CHECK(!parser.match(Tk::EXIT), "match EXIT again should fail");
  CHECK(parser.match(Tk::INT_LIT), "match INT_LIT should succeed");
  CHECK(parser.previous().is(Tk::INT_LIT), "previous should be INT_LIT");
  CHECK(parser.previous().lexeme == "42", "previous lexeme should be 42");

  std::cout << "  PASS checkAndMatch\n";
  return 0;
}

static int testIsAtEnd() {
  std::vector<elpc::Token<Tk>> single = {{Tk::EXIT, "exit"}};
  TestParser parser(single);

  CHECK(!parser.isAtEnd(), "should not be at end initially");
  parser.consume();
  CHECK(parser.isAtEnd(), "should be at end after consuming all tokens");

  std::cout << "  PASS isAtEnd\n";
  return 0;
}

static int testMatchAny() {
  auto tokens = makeTokens();
  TestParser parser(tokens);

  CHECK(parser.matchAny(Tk::EXIT, Tk::INT_LIT, Tk::PLUS),
        "matchAny should find EXIT");

  CHECK(parser.matchAny(Tk::INT_LIT, Tk::PLUS),
        "matchAny should find INT_LIT");

  std::cout << "  PASS matchAny\n";
  return 0;
}

static int testExpect() {
  auto tokens = makeTokens();
  TestParser parser(tokens);

  parser.expect(Tk::EXIT, "expected exit");
  parser.expect(Tk::INT_LIT, "expected int literal");

  // This should throw
  try {
    parser.expect(Tk::SEMI, "expected semicolon but got PLUS");
    CHECK(false, "expect should have thrown");
  } catch (const std::runtime_error &e) {
    std::string msg = e.what();
    CHECK(msg.find("Syntax Error") != std::string::npos,
          "error should mention 'Syntax Error'");
  }

  std::cout << "  PASS expect\n";
  return 0;
}

static int testSynchronize() {
  auto tokens = makeTokens();
  TestParser parser(tokens);

  // Consume a few, then synchronize to SEMI
  parser.consume(); // EXIT
  parser.consume(); // INT_LIT

  parser.synchronize(Tk::SEMI);

  CHECK(parser.peek().is(Tk::SEMI), "after sync to SEMI, peek should be SEMI");

  std::cout << "  PASS synchronize\n";
  return 0;
}

static int testEmptyTokenList() {
  std::vector<elpc::Token<Tk>> empty;
  TestParser parser(empty);

  CHECK(parser.isAtEnd(), "empty token list should be at end");

  std::cout << "  PASS emptyTokenList\n";
  return 0;
}

static int testDiagnosticMode() {
  auto tokens = makeTokens();
  elpc::DiagnosticEngine diag;
  TestParser parser(tokens);
  parser.setDiagnostics(diag);

  // consume EXIT and INT_LIT normally
  parser.consume(); // EXIT
  parser.consume(); // INT_LIT

  // Now expect SEMI — but the next token is PLUS -> should report not throw
  parser.expect(Tk::SEMI, "expected semicolon");

  CHECK(diag.hasErrors(), "should have reported the error");
  CHECK(diag.count() >= 1, "at least one diagnostic");

  // Recovery should skip to SEMI, so peek should now be at SEMI
  CHECK(parser.peek().is(Tk::SEMI),
        "after recovery, parser should be at SEMI");

  // Continue from SEMI
  parser.consume(); // consume SEMI
  const auto &tok = parser.expect(Tk::INT_LIT, "expected int");
  CHECK(tok.is(Tk::INT_LIT), "should return INT_LIT");

  std::cout << "  PASS diagnosticMode\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testPeekAndConsume();
  failures += testCheckAndMatch();
  failures += testIsAtEnd();
  failures += testMatchAny();
  failures += testExpect();
  failures += testSynchronize();
  failures += testEmptyTokenList();
  failures += testDiagnosticMode();

  if (failures == 0)
    std::cout << "All TestParser tests passed!\n";
  else
    std::cerr << failures << " TestParser test(s) failed!\n";

  return failures;
}
