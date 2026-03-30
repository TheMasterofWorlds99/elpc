// tests/TestParser.cpp
#include "TestHelpers.hpp"

struct ExitNode   { int value; };
struct ReturnNode { int value; };

struct ProgramNode {
    std::vector<ExitNode>    exits;
    std::vector<ReturnNode>  returns;
    std::vector<std::string> errors;
};

class TestParser : public elpc::Parser<TokenType> {
public:
    TestParser(const std::vector<elpc::Token<TokenType>> &tokens)
    : elpc::Parser<TokenType>(tokens) {}

    ProgramNode parseProgram() {
        ProgramNode program;
        while (!isAtEnd()) {
            if (check(TokenType::EXIT))
                program.exits.push_back(parseExit(program));
            else if (matchAny(TokenType::RETURN))
                program.returns.push_back(parseReturn(program));
            else {
                const auto &t = peek();
                program.errors.push_back(
                    "[elpc] Unexpected token '" + std::string(t.lexeme) +
                    "' at line " + std::to_string(t.location.line) +
                    ", col "     + std::to_string(t.location.column));
                synchronize(TokenType::SEMI_COLON);
                if (!isAtEnd()) consume();
            }
        }
        return program;
    }

private:
    ExitNode parseExit(ProgramNode &program) {
        try {
            expect(TokenType::EXIT,        "Expected 'exit'");
            expect(TokenType::LEFT_PAREN,  "Expected '('");
            const auto &num = expect(TokenType::INT_LIT, "Expected integer literal");
            expect(TokenType::RIGHT_PAREN, "Expected ')'");
            expect(TokenType::SEMI_COLON,  "Expected ';'");
            return ExitNode{ num.toInt().value_or(0) };
        } catch (const std::exception &e) {
            program.errors.push_back(e.what());
            synchronize(TokenType::SEMI_COLON);
            if (!isAtEnd()) consume();
            return ExitNode{ -1 };
        }
    }

    ReturnNode parseReturn(ProgramNode &program) {
        try {
            expect(TokenType::LEFT_PAREN, "Expected '('");
            const auto &val = expectOneOf("Expected int or float literal",
                                          TokenType::INT_LIT, TokenType::FLOAT_LIT);
            if (!match(TokenType::RIGHT_PAREN))
                expect(TokenType::RIGHT_PAREN, "Expected ')'");
            expect(TokenType::SEMI_COLON, "Expected ';'");
            return ReturnNode{ val.toInt().value_or(0) };
        } catch (const std::exception &e) {
            program.errors.push_back(e.what());
            synchronize(TokenType::SEMI_COLON);
            if (!isAtEnd()) consume();
            return ReturnNode{ -1 };
        }
    }
};

void printProgram(const ProgramNode &p) {
    for (const auto &e : p.exits)
        std::cout << "ExitNode   { value=" << e.value << " }\n";
    for (const auto &r : p.returns)
        std::cout << "ReturnNode { value=" << r.value << " }\n";
    if (!p.errors.empty()) {
        std::cout << "-- Errors --\n";
        for (const auto &err : p.errors)
            std::cerr << err << "\n";
    }
}

void runTest(const std::string &label, const std::string &src) {
    std::cout << "\n=== " << label << " ===\n";
    auto tokens = makeLexer(src).tokenize();
    TestParser parser(tokens);
    printProgram(parser.parseProgram());
}

int main() {
    runTest("CLEAN INPUT",              "exit(0); return(42); return(7);");
    runTest("RECOVERY: unknown token",  "exit(0); garbage; return(42);");
    runTest("RECOVERY: malformed exit", "exit(0); exit(1; return(42);");
}
