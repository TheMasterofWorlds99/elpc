#include <elpc/sema/type.hpp>
#include <iostream>
#include <string>

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL: " << msg << " (" << #cond << ")\n"; \
      return 1; \
    } \
  } while (false)

static int testPrimitiveTypes() {
  elpc::TypeContext ctx;

  auto *intTy  = ctx.getInt();
  auto *boolTy = ctx.getBool();
  auto *voidTy = ctx.getVoid();

  CHECK(intTy->kind == elpc::TypeKind::PRIMITIVE, "int should be PRIMITIVE");
  CHECK(intTy->toString() == "int", "int toString");
  CHECK(boolTy->toString() == "bool", "bool toString");
  CHECK(voidTy->toString() == "void", "void toString");

  CHECK(elpc::isInt(intTy), "isInt should be true");
  CHECK(elpc::isBool(boolTy), "isBool should be true");
  CHECK(!elpc::isInt(boolTy), "isInt should be false for bool");

  // Canonical: same primitive always returns same pointer
  CHECK(ctx.getInt() == ctx.getInt(), "getInt should be canonical");

  std::cout << "  PASS primitiveTypes\n";
  return 0;
}

static int testFunctionType() {
  elpc::TypeContext ctx;

  auto *intTy  = ctx.getInt();
  auto *boolTy = ctx.getBool();

  auto *fnTy1 = ctx.getFunction({intTy, boolTy}, intTy);
  auto *fnTy2 = ctx.getFunction({intTy, boolTy}, intTy);
  auto *fnTy3 = ctx.getFunction({intTy}, intTy);

  CHECK(fnTy1->kind == elpc::TypeKind::FUNCTION, "function kind");
  CHECK(fnTy1 == fnTy2, "identical function types should be deduplicated");
  CHECK(fnTy1 != fnTy3, "different function types should not be equal");

  CHECK(fnTy1->toString() == "(int, bool) -> int",
        "function toString");

  CHECK(fnTy1->paramTypes.size() == 2, "should have 2 params");
  CHECK(fnTy1->returnType == intTy, "return type should be int");

  std::cout << "  PASS functionType\n";
  return 0;
}

static int testArrayType() {
  elpc::TypeContext ctx;

  auto *intTy   = ctx.getInt();
  auto *arrInt1 = ctx.getArray(intTy);
  auto *arrInt2 = ctx.getArray(intTy);
  auto *arrBool = ctx.getArray(ctx.getBool());

  CHECK(arrInt1->kind == elpc::TypeKind::ARRAY, "array kind");
  CHECK(arrInt1 == arrInt2, "identical array types should be deduplicated");
  CHECK(arrInt1 != arrBool, "different array types should not be equal");
  CHECK(arrInt1->toString() == "int[]", "array toString");
  CHECK(arrInt1->elementType == intTy, "element type should be int");

  std::cout << "  PASS arrayType\n";
  return 0;
}

static int testNamedType() {
  elpc::TypeContext ctx;

  auto *intTy = ctx.getInt();
  auto *point = ctx.getNamed("Point", {intTy, intTy});
  auto *point2 = ctx.getNamed("Point");
  auto *other = ctx.getNamed("Other");

  CHECK(point->kind == elpc::TypeKind::NAMED, "named kind");
  CHECK(point == point2, "same name should deduplicate");
  CHECK(point != other, "different names should not be equal");
  CHECK(point->toString() == "Point", "named toString");
  CHECK(point->name == "Point", "name should be Point");

  std::cout << "  PASS namedType\n";
  return 0;
}

static int testPointerEquality() {
  elpc::TypeContext ctx;

  // All identical types should share pointers
  auto *a = ctx.getInt();
  auto *b = ctx.getPrimitive(elpc::PrimitiveKind::INT);
  CHECK(a == b, "getInt and getPrimitive(INT) should be same pointer");

  // Function types with same signature
  auto *f1 = ctx.getFunction({a}, a);
  auto *f2 = ctx.getFunction({ctx.getInt()}, ctx.getInt());
  CHECK(f1 == f2, "identical function sigs should deduplicate");

  // Array types
  auto *arr1 = ctx.getArray(a);
  auto *arr2 = ctx.getArray(ctx.getInt());
  CHECK(arr1 == arr2, "identical array types should deduplicate");

  std::cout << "  PASS pointerEquality\n";
  return 0;
}

int main() {
  int failures = 0;

  failures += testPrimitiveTypes();
  failures += testFunctionType();
  failures += testArrayType();
  failures += testNamedType();
  failures += testPointerEquality();

  if (failures == 0)
    std::cout << "All TestTypeSystem tests passed!\n";
  else
    std::cerr << failures << " TestTypeSystem test(s) failed!\n";

  return failures;
}
