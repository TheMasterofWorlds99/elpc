// tests/TestDiagnostics.cpp
#include "TestHelpers.hpp"

struct ExitNode   { int value; };
struct ReturnNode { int value; };
struct ProgramNode {
    std::vector<ExitNode>   exits;
    std::vector<ReturnNode> returns;
};

class DiagnosticParser : public elpc::Parser<TokenType> {
    elpc::DiagnosticEngine &engine;

public:
    DiagnosticParser(const std::vector<elpc::Token<TokenType>> &tokens,
                     elpc::DiagnosticEngine &engine)
    : elpc::Parser<TokenType>(tokens), engine(engine) {}

    ProgramNode parseProgram() {
        ProgramNode program;
        while (!isAtEnd()) {
            if (check(TokenType::EXIT))
                program.exits.push_back(parseExit());
            else if (matchAny(TokenType::RETURN))
                program.returns.push_back(parseReturn());
            else {
                const auto &t = peek();
                engine.error("Unexpected token '" + std::string(t.lexeme) + "'",
                             t.location);
                synchronize(TokenType::SEMI_COLON);
                if (!isAtEnd()) consume();
            }
        }
        return program;
    }

private:
    ExitNode parseExit() {
        try {
            expect(TokenType::EXIT,        "Expected 'exit'");
            expect(TokenType::LEFT_PAREN,  "Expected '('");
            const auto &num = expect(TokenType::INT_LIT, "Expected integer literal");
            expect(TokenType::RIGHT_PAREN, "Expected ')'");
            expect(TokenType::SEMI_COLON,  "Expected ';'");
            return ExitNode{ num.toInt().value_or(0) };
        } catch (const std::exception &e) {
            engine.error(e.what(), peek().location);
            synchronize(TokenType::SEMI_COLON);
            if (!isAtEnd()) consume();
            return ExitNode{ -1 };
        }
    }

    ReturnNode parseReturn() {
        try {
            expect(TokenType::LEFT_PAREN, "Expected '('");
            const auto &val = expectOneOf("Expected int or float literal",
                                          TokenType::INT_LIT, TokenType::FLOAT_LIT);
            if (!match(TokenType::RIGHT_PAREN))
                expect(TokenType::RIGHT_PAREN, "Expected ')'");
            if (!match(TokenType::SEMI_COLON))
                engine.warning("Missing ';' after return statement", peek().location);
            return ReturnNode{ val.toInt().value_or(0) };
        } catch (const std::exception &e) {
            engine.error(e.what(), peek().location);
            synchronize(TokenType::SEMI_COLON);
            if (!isAtEnd()) consume();
            return ReturnNode{ -1 };
        }
    }
};

void runDiagnosticTest(const std::string &label, const std::string &src) {
    std::cout << "\n=== " << label << " ===\n";
    auto tokens = makeLexer(src).tokenize();

    elpc::DiagnosticEngine engine;
    DiagnosticParser parser(tokens, engine);
    auto program = parser.parseProgram();

    for (const auto &e : program.exits)
        std::cout << "ExitNode   { value=" << e.value << " }\n";
    for (const auto &r : program.returns)
        std::cout << "ReturnNode { value=" << r.value << " }\n";

    if (!engine.empty()) {
        std::cout << "-- Diagnostics (" << engine.count() << ") --\n";
        engine.reportDiagnostics(std::cerr);
    }

    std::cout << "-- Has errors: " << (engine.hasErrors() ? "yes" : "no") << " --\n";
}

int main() {
    runDiagnosticTest("DIAG: clean input",                 "exit(0); return(42);");
    runDiagnosticTest("DIAG: errors collected, continues", "exit(0); garbage; exit(1; return(42);");
    runDiagnosticTest("DIAG: warning on missing semicolon","exit(0); return(42)");
}
