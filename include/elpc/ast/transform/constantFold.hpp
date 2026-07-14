/*
   CONSTANTFOLD.HPP
   Constant folding pass — evaluates expressions on literals at compile time.

   Transforms:  1 + 2  →  3
                true && false  →  false
                -5  →  -5
                !true  →  false

   Works as an ASTTransform: override transformBinary, transformUnary, etc.
   to detect constant operands and replace with the folded result.
*/

#pragma once

#include <elpc/ast/ast.hpp>

namespace elpc {

struct ConstantFold : ASTTransform<ConstantFold> {
  BumpAllocator &pool;

  explicit ConstantFold(BumpAllocator &p) : pool(p) {}

  std::unique_ptr<ASTNode> transformBinary(BinaryExpr &n) override {
    // First transform children recursively
    auto left = transform(*n.left);
    auto right = transform(*n.right);
    if (!left || !right) return nullptr;

    // Try to fold if both sides are literal integers
    auto *li = dynamic_cast<LiteralExpr*>(left.get());
    auto *ri = dynamic_cast<LiteralExpr*>(right.get());

    if (li && ri) {
      int64_t result = 0;
      switch (n.op) {
        case BinaryOp::ADD: result = li->intValue + ri->intValue; break;
        case BinaryOp::SUB: result = li->intValue - ri->intValue; break;
        case BinaryOp::MUL: result = li->intValue * ri->intValue; break;
        case BinaryOp::DIV:
          if (ri->intValue != 0) result = li->intValue / ri->intValue;
          else return makeNode<BinaryExpr>(n.op, std::move(left), std::move(right));
          break;
        case BinaryOp::EQ:  return makeNode<BoolExpr>(li->intValue == ri->intValue);
        case BinaryOp::NE:  return makeNode<BoolExpr>(li->intValue != ri->intValue);
        case BinaryOp::LT:  return makeNode<BoolExpr>(li->intValue < ri->intValue);
        case BinaryOp::GT:  return makeNode<BoolExpr>(li->intValue > ri->intValue);
        case BinaryOp::LE:  return makeNode<BoolExpr>(li->intValue <= ri->intValue);
        case BinaryOp::GE:  return makeNode<BoolExpr>(li->intValue >= ri->intValue);
        default: return makeNode<BinaryExpr>(n.op, std::move(left), std::move(right));
      }
      return makeNode<LiteralExpr>(result);
    }

    // Fold boolean expressions
    auto *bi = dynamic_cast<BoolExpr*>(left.get());
    auto *bj = dynamic_cast<BoolExpr*>(right.get());

    if (bi && bj) {
      switch (n.op) {
        case BinaryOp::AND: return makeNode<BoolExpr>(bi->boolValue && bj->boolValue);
        case BinaryOp::OR:  return makeNode<BoolExpr>(bi->boolValue || bj->boolValue);
        default: break;
      }
    }

    return makeNode<BinaryExpr>(n.op, std::move(left), std::move(right));
  }

  std::unique_ptr<ASTNode> transformUnary(UnaryExpr &n) override {
    auto op = transform(*n.operand);
    if (!op) return nullptr;

    auto *li = dynamic_cast<LiteralExpr*>(op.get());
    if (li && n.op == UnaryOp::NEG) {
      return makeNode<LiteralExpr>(-li->intValue);
    }

    auto *bi = dynamic_cast<BoolExpr*>(op.get());
    if (bi && n.op == UnaryOp::NOT) {
      return makeNode<BoolExpr>(!bi->boolValue);
    }

    return makeNode<UnaryExpr>(n.op, std::move(op));
  }
};

} // namespace elpc
