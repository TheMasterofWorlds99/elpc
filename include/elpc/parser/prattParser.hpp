/*
   PRATTPARSER.HPP
   This file contains the PrattParser base class, which extends Parser with
   a Pratt (top-down operator precedence) expression parsing engine.

   Users subclass PrattParser instead of Parser when they need expression
   parsing with operator precedence. All Parser helpers (peek, consume,
   expect, synchronize, etc.) are still available.

   Example:
     class MyParser : public elpc::PrattParser<TokenType, ExprNode> {
     public:
       MyParser(const std::vector<elpc::Token<TokenType>> &tokens)
           : elpc::PrattParser<TokenType, ExprNode>(tokens) {

         // Register a literal
         registerPrefix(TokenType::INT_LIT, [this](const auto &tok) {
           return ExprNode{ tok.toInt().value_or(0) };
         });

         // Register a binary operator
         registerInfix(TokenType::PLUS, Precedence::TERM,
           [this](const auto &tok, auto lhs) {
             auto rhs = parseExpression(Precedence::TERM);
             return ExprNode{ lhs.value + rhs.value };
           });
       }

       ExprNode parseExpr() {
         return parseExpression(Precedence::NONE);
       }
     };
*/

#pragma once

#include <elpc/parser/parser.hpp>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace elpc {

enum class Precedence : int {
  NONE = 0,
  ASSIGNMENT = 1, // =
  EQUALITY = 2,   // == !=
  COMPARISON = 3, // < > <= >=
  TERM = 4,       // + -
  FACTOR = 5,     // * /
  UNARY = 6,      // - !
  PRIMARY = 7     // literals, grouping
};

inline int precedenceOf(Precedence p) { return static_cast<int>(p); }

template <typename TokenType, typename NodeType>
class PrattParser : public Parser<TokenType> {
public:
  using PrefixFn = std::function<NodeType(const Token<TokenType> &)>;
  using InfixFn = std::function<NodeType(const Token<TokenType> &, NodeType)>;

private:
  struct InfixRule {
    int precedence;
    InfixFn fn;
  };

  std::unordered_map<int, PrefixFn> prefixRules;
  std::unordered_map<int, InfixRule> infixRules;

  static int key(TokenType t) { return static_cast<int>(t); }

protected:
  void registerPrefix(TokenType type, PrefixFn fn) {
    prefixRules[key(type)] = std::move(fn);
  }

  void registerInfix(TokenType type, Precedence precedence, InfixFn fn) {
    infixRules[key(type)] = InfixRule{precedenceOf(precedence), std::move(fn)};
  }

  NodeType parseExpression(Precedence minPrec = Precedence::NONE) {
    return parseExpression(precedenceOf(minPrec));
  }

  NodeType parseExpression(int minPrec) {
    // Consume the first token and find its prefix rule
    const auto &tok = this->consume();
    auto prefixIt = prefixRules.find(key(tok.type));

    if (prefixIt == prefixRules.end()) {
      throw std::runtime_error("[elpc] Pratt: no prefix rule for token '" +
                               std::string(tok.lexeme) + "' at line " +
                               std::to_string(tok.location.line) + ", col " +
                               std::to_string(tok.location.column));
    }

    // Parse the left-hand side using the prefix rule
    NodeType left = prefixIt->second(tok);

    // Absorb infix operators as long as their precedence is high enough
    while (true) {
      const auto &next = this->peek();
      auto infixIt = infixRules.find(key(next.type));

      if (infixIt == infixRules.end())
        break;

      if (infixIt->second.precedence <= minPrec)
        break;

      const auto &opTok = this->consume();
      left = infixIt->second.fn(opTok, std::move(left));
    }

    return left;
  }

public:
  explicit PrattParser(const std::vector<Token<TokenType>> &tokens)
      : Parser<TokenType>(tokens) {}
};

} // namespace elpc
