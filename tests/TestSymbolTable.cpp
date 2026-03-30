// tests/TestSymbolTable.cpp
#include "TestHelpers.hpp"

int main() {
    std::cout << "=== SYMBOL TABLE ===\n";

    elpc::SymbolTable<std::string, int> table;

    table.define("x", 10);
    table.define("y", 20);

    std::cout << "-- Global scope --\n";
    std::cout << "x = " << table.lookup("x").value_or(-1) << "\n";
    std::cout << "y = " << table.lookup("y").value_or(-1) << "\n";

    table.pushScope();
    table.define("x", 99);
    table.define("z", 30);

    std::cout << "\n-- Inner scope (x shadowed, z added) --\n";
    std::cout << "x = " << table.lookup("x").value_or(-1) << "\n";
    std::cout << "y = " << table.lookup("y").value_or(-1) << "\n";
    std::cout << "z = " << table.lookup("z").value_or(-1) << "\n";

    bool defined = table.define("z", 999);
    std::cout << "\n-- Redefinition of z in current scope --\n";
    std::cout << "define returned: " << (defined ? "true" : "false") << "\n";
    std::cout << "z = " << table.lookup("z").value_or(-1) << "\n";

    std::cout << "\n-- lookupCurrent vs lookup --\n";
    std::cout << "lookupCurrent(y): "
    << (table.lookupCurrent("y").has_value() ? "found" : "not found") << "\n";
    std::cout << "lookup(y):        "
    << (table.lookup("y").has_value() ? "found" : "not found") << "\n";

    table.popScope();

    std::cout << "\n-- Back to global scope --\n";
    std::cout << "x = " << table.lookup("x").value_or(-1) << "\n";
    std::cout << "z = " << table.lookup("z").value_or(-1) << "\n";

    std::cout << "\n-- Pop global scope guard --\n";
    try {
        table.popScope();
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
}
