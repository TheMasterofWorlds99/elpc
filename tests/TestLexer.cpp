// tests/TestLexer.cpp
#include "TestHelpers.hpp"

void testCleanInput() {
    std::cout << "=== CLEAN INPUT ===\n";
    auto tokens = makeLexer("exit(0); return(42); return(7);").tokenize();
    for (const auto &t : tokens)
        std::cout << t << "\n";
}

void testUnknownCharacter() {
    std::cout << "\n=== LEXER ERROR: unknown character ===\n";
    try {
        auto tokens = makeLexer("exit(0); @;").tokenize();
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
}

int main() {
    testCleanInput();
    testUnknownCharacter();
}
