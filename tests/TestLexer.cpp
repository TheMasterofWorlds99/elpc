#include <elpc/elpc.hpp>
#include <cassert>
#include <iostream>
#include <string>

enum class Tk { EXIT, INT_LIT, FLOAT_LIT, IDENT, LPAREN, RPAREN, PLUS, MINUS, STAR, SLASH };

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testBasicTokenization() {
  auto src = elpc::SourceManager::fromString("test", "exit 42");
  elpc::Lexer<Tk> lexer(src);
  lexer.addRule(Tk::EXIT, "\\bexit\\b");
  lexer.addRule(Tk::INT_LIT, "\\b[0-9]+\\b");
  lexer.addSkip("\\s+");

  auto tokens = lexer.tokenize();

  CHECK(tokens.size() == 2, "expected 2 tokens");
  CHECK(tokens[0].is(Tk::EXIT), "first token should be EXIT");
  CHECK(tokens[0].lexeme == "exit", "first token lexeme should be 'exit'");
  CHECK(tokens[1].is(Tk::INT_LIT), "second token should be INT_LIT");
  CHECK(tokens[1].lexeme == "42", "second token lexeme should be '42'");
  CHECK(tokens[1].toInt().has_value(), "should convert to int");
  CHECK(tokens[1].toInt().value() == 42, "int value should be 42");

  std::cout << "  PASS basicTokenization\n";
  return 0;
}

static int testSkipRules() {
  auto src = elpc::SourceManager::fromString("test", "a   b\tc\n  d");
  elpc::Lexer<Tk> lexer(src);
  lexer.addRule(Tk::IDENT, "\\b[a-zA-Z]\\b");
  lexer.addSkip("\\s+");

  auto tokens = lexer.tokenize();

  CHECK(tokens.size() == 4, "expected 4 tokens after skipping whitespace");
  CHECK(tokens[0].lexeme == "a", "first token: a");
  CHECK(tokens[1].lexeme == "b", "second token: b");
  CHECK(tokens[2].lexeme == "c", "third token: c");
  CHECK(tokens[3].lexeme == "d", "fourth token: d");

  std::cout << "  PASS skipRules\n";
  return 0;
}

static int testMultipleTokenizes() {
  elpc::Lexer<Tk> lexer;
  lexer.addRule(Tk::INT_LIT, "\\b[0-9]+\\b");
  lexer.addSkip("\\s+");

  {
    auto src = elpc::SourceManager::fromString("test", "1 2 3");
    lexer.setSource(src);
    auto t1 = lexer.tokenize();
    CHECK(t1.size() == 3, "first tokenize: 3 tokens");
  }

  {
    auto src = elpc::SourceManager::fromString("test", "42");
    lexer.setSource(src);
    auto t2 = lexer.tokenize();
    CHECK(t2.size() == 1, "second tokenize: 1 token");
    CHECK(t2[0].toInt().value() == 42, "second tokenize value: 42");
  }

  std::cout << "  PASS multipleTokenizes\n";
  return 0;
}

static int testRemaining() {
  auto src = elpc::SourceManager::fromString("test", "hello world");
  elpc::Lexer<Tk> lexer(src);
  lexer.addRule(Tk::IDENT, "\\b[a-zA-Z]+\\b");
  lexer.addSkip("\\s+");

  lexer.tokenize();
  CHECK(lexer.remaining().empty(), "should have no remaining after full tokenize");

  std::cout << "  PASS remaining\n";
  return 0;
}

static int testTokenIsOneOf() {
  elpc::Token<Tk> tok(Tk::PLUS, "+");
  CHECK(tok.isOneOf(Tk::PLUS, Tk::MINUS), "PLUS should match PLUS/MINUS");
  CHECK(!tok.isOneOf(Tk::STAR, Tk::SLASH), "PLUS should not match STAR/SLASH");
  CHECK(tok.isOneOf(Tk::PLUS), "PLUS should match itself");

  std::cout << "  PASS tokenIsOneOf\n";
  return 0;
}

static int testTokenConversion() {
  elpc::Token<Tk> intTok(Tk::INT_LIT, "123");
  CHECK(intTok.toInt().value() == 123, "toInt");

  elpc::Token<Tk> badIntTok(Tk::INT_LIT, "abc");
  CHECK(!badIntTok.toInt().has_value(), "toInt on non-numeric");

  elpc::Token<Tk> floatTok(Tk::FLOAT_LIT, "3.14");
  CHECK(floatTok.toFloat().has_value(), "toFloat should succeed");
  CHECK(floatTok.toDouble().has_value(), "toDouble should succeed");

  elpc::Token<Tk> badFloatTok(Tk::FLOAT_LIT, "notanumber");
  CHECK(!badFloatTok.toFloat().has_value(), "toFloat on non-numeric");

  std::cout << "  PASS tokenConversion\n";
  return 0;
}

static int testTokenEquality() {
  elpc::Token<Tk> a(Tk::EXIT, "exit");
  elpc::Token<Tk> b(Tk::EXIT, "exit");
  elpc::Token<Tk> c(Tk::INT_LIT, "exit");

  CHECK(a == b, "same type and lexeme should be equal");
  CHECK(a != c, "different type should not be equal");

  std::cout << "  PASS tokenEquality\n";
  return 0;
}

static int testDiagnosticMode() {
  elpc::DiagnosticEngine diag;
  auto src = elpc::SourceManager::fromString("test", "exit @ 42");
  elpc::Lexer<Tk> lexer(src);
  lexer.setDiagnostics(diag);
  lexer.addRule(Tk::EXIT, "\\bexit\\b");
  lexer.addRule(Tk::INT_LIT, "\\b[0-9]+\\b");
  lexer.addSkip("\\s+");

  // '@' is not a valid token — should be reported via DiagnosticEngine
  auto tokens = lexer.tokenize();

  CHECK(diag.hasErrors(), "should have reported the unexpected '@'");
  CHECK(diag.count() >= 1, "at least one diagnostic");
  CHECK(tokens.size() == 2, "should still get valid tokens (EXIT and INT_LIT)");
  CHECK(tokens[0].is(Tk::EXIT), "first token: EXIT");
  CHECK(tokens[1].is(Tk::INT_LIT), "second token: INT_LIT");

  std::cout << "  PASS diagnosticMode\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testBasicTokenization();
  failures += testSkipRules();
  failures += testMultipleTokenizes();
  failures += testRemaining();
  failures += testTokenIsOneOf();
  failures += testTokenConversion();
  failures += testTokenEquality();
  failures += testDiagnosticMode();

  if (failures == 0)
    std::cout << "All TestLexer tests passed!\n";
  else
    std::cerr << failures << " TestLexer test(s) failed!\n";

  return failures;
}
