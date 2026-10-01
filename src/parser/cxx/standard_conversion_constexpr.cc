// Copyright (c) 2026 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cxx/arena.h>
#include <cxx/ast.h>
#include <cxx/ast_interpreter.h>
#include <cxx/standard_conversion.h>
#include <cxx/symbols.h>
#include <cxx/types.h>

#include <utility>

namespace cxx {

auto StandardConversion::convertConstantExpression(ExpressionAST* expression,
                                                   const Type* destinationType)
    -> ConstExpressionAST* {
  if (!expression || !expression->type || !destinationType) return nullptr;

  auto sequence = computeConversionSequence(expression, destinationType);
  if (!sequence || sequence.form == ConversionSequenceForm::kAmbiguous) {
    return nullptr;
  }
  if (!isAccessible(sequence)) return nullptr;

  ASTInterpreter interpreter{unit_};
  auto converted = expression;
  if (sequence.resolvedFunction) {
    applyResolvedFunction(converted, sequence.resolvedFunction);
  }

  auto retainValue = [&]() -> ConstExpressionAST* {
    if (auto constant = ast_cast<ConstExpressionAST>(converted)) {
      return constant;
    }

    auto value = interpreter.evaluate(converted);
    if (!value) return nullptr;

    auto constant = ConstExpressionAST::create(arena_);
    constant->expression = converted;
    constant->type = converted->type;
    constant->valueCategory = ValueCategory::kPrValue;
    constant->constValue = arena_->make<ConstValue>(std::move(*value));
    converted = constant;
    return constant;
  };

  auto applyConstantStep = [&](const ImplicitConversionSequence::Step& step) {
    switch (step.kind) {
      case ImplicitCastKind::kIdentity:
      case ImplicitCastKind::kLValueToRValueConversion:
      case ImplicitCastKind::kArrayToPointerConversion:
      case ImplicitCastKind::kFunctionToPointerConversion:
      case ImplicitCastKind::kQualificationConversion:
      case ImplicitCastKind::kIntegralPromotion:
      case ImplicitCastKind::kFloatingPointPromotion:
      case ImplicitCastKind::kFunctionPointerConversion:
        break;

      case ImplicitCastKind::kUserDefinedConversion:
        if (!sequence.udc.function || !sequence.udc.function->isConstexpr() ||
            sequence.udc.function->isDeleted()) {
          return false;
        }
        break;

      case ImplicitCastKind::kIntegralConversion:
      case ImplicitCastKind::kBooleanConversion: {
        auto constant = retainValue();
        if (!constant) return false;

        auto value = std::get_if<ConstInt>(constant->constValue);
        if (!value) return false;

        const bool targetIsBool =
            type_cast<BoolType>(traits.remove_cv(step.type)) != nullptr;
        if (targetIsBool) {
          if (!value->isZero() && *value != ConstInt{1}) return false;
        } else if (!traits.converted_integral_constant(step.type, *value)) {
          return false;
        }
        break;
      }

      case ImplicitCastKind::kFloatingPointConversion: {
        auto constant = retainValue();
        if (!constant) return false;

        auto narrowed = interpreter.convertArithmetic(
            *constant->constValue, converted->type, step.type);
        if (!narrowed) return false;

        auto restored = interpreter.convertArithmetic(*narrowed, step.type,
                                                      converted->type);
        if (!restored || *restored != *constant->constValue) return false;
        break;
      }

      case ImplicitCastKind::kPointerConversion:
      case ImplicitCastKind::kPointerToMemberConversion:
        if (!type_cast<NullptrType>(traits.remove_cv(converted->type))) {
          return false;
        }
        break;

      default:
        return false;
    }

    applyStep(sequence, step, converted);
    return true;
  };

  for (const auto& step : sequence.udc.firstSteps) {
    if (!applyConstantStep(step)) return nullptr;
  }
  for (const auto& step : sequence.steps) {
    if (!applyConstantStep(step)) return nullptr;
  }
  for (const auto& step : sequence.udc.secondSteps) {
    if (!applyConstantStep(step)) return nullptr;
  }

  adjustCv(converted);
  return retainValue();
}

}  // namespace cxx
