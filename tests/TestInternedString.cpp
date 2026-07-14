#include <elpc/core/internedString.hpp>
#include <iostream>
#include <string>

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testBasicInterning() {
  elpc::InternedStringPool pool;

  auto a = pool.intern("hello");
  auto b = pool.intern("hello");
  auto c = pool.intern("world");

  CHECK(a == b, "same string should intern to same handle");
  CHECK(a != c, "different strings should intern to different handles");
  CHECK(a.view() == "hello", "view should return the original string");
  CHECK(c.view() == "world", "view should return the original string");

  std::cout << "  PASS basicInterning\n";
  return 0;
}

static int testEmptyAndDefault() {
  elpc::InternedString empty;
  CHECK(empty.empty(), "default interned string should be empty");
  CHECK(!empty, "default should be falsy");
  CHECK(empty.view().empty(), "default view should be empty");

  elpc::InternedStringPool pool;
  auto e = pool.intern("");
  CHECK(e.view().empty(), "empty string interned should have empty view");
  CHECK(e.view().data() != nullptr, "but should have a valid pointer");

  std::cout << "  PASS emptyAndDefault\n";
  return 0;
}

static int testPoolSize() {
  elpc::InternedStringPool pool;

  pool.intern("a");
  pool.intern("b");
  pool.intern("a"); // duplicate
  CHECK(pool.size() == 2, "pool should have 2 unique strings");

  pool.intern("c");
  CHECK(pool.size() == 3, "pool should have 3 unique strings");

  std::cout << "  PASS poolSize\n";
  return 0;
}

static int testMoveInterning() {
  elpc::InternedStringPool pool;

  std::string s = "dynamic";
  auto h = pool.internOwned(std::move(s));
  CHECK(h.view() == "dynamic", "move-interned string should work");

  std::cout << "  PASS moveInterning\n";
  return 0;
}

static int testClear() {
  elpc::InternedStringPool pool;

  auto a = pool.intern("hello");
  CHECK(!a.empty(), "should not be empty before clear");

  pool.clear();
  CHECK(pool.size() == 0, "pool should be empty after clear");

  // After clear, old handle should still not crash (string data may persist)
  // but its view should still be valid since the strings live in the set
  // (clear destroys them though, so this would be UB. Don't use handles after pool clear.)

  std::cout << "  PASS clear\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testBasicInterning();
  failures += testEmptyAndDefault();
  failures += testPoolSize();
  failures += testMoveInterning();
  failures += testClear();

  if (failures == 0)
    std::cout << "All TestInternedString tests passed!\n";
  else
    std::cerr << failures << " TestInternedString test(s) failed!\n";

  return failures;
}
