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

#include <cxx/ast.h>
#include <cxx/ast_interpreter.h>
#include <cxx/binder.h>
#include <cxx/const_int.h>
#include <cxx/control.h>
#include <cxx/dependent_types.h>
#include <cxx/names.h>
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <cxx/type_traits.h>
#include <cxx/types.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace cxx {
namespace {
struct TypedEnumeratorValue {
  const Type* type = nullptr;
  ConstInt value;
};

[[nodiscard]] auto fixedUnderlyingType(ScopeSymbol* scope) -> const Type* {
  if (auto scoped = symbol_cast<ScopedEnumSymbol>(scope))
    return scoped->underlyingType();
  auto unscoped = symbol_cast<EnumSymbol>(scope);
  if (unscoped && unscoped->hasFixedUnderlyingType())
    return unscoped->underlyingType();
  return nullptr;
}

[[nodiscard]] auto convertedEnumeratorValue(const TypeTraits& traits,
                                            const Type* type,
                                            const ConstInt& value)
    -> std::optional<ConstInt> {
  if (type_cast<BoolType>(traits.remove_cv(type)) && value != ConstInt{0} &&
      value != ConstInt{1}) {
    return std::nullopt;
  }
  return traits.converted_integral_constant(type, value);
}

[[nodiscard]] auto incrementedValue(const ConstInt& previous)
    -> std::optional<ConstInt> {
  if (previous.isNegative()) {
    return ConstInt::make(previous.toWide() + 1, ConstInt::maxWidth, true);
  }

  auto value = previous.toUWide();
  if (value == std::numeric_limits<ConstInt::UWide>::max()) return std::nullopt;
  return ConstInt::make(static_cast<ConstInt::Wide>(value + 1),
                        ConstInt::maxWidth, false);
}

[[nodiscard]] auto firstRepresentingType(
    const TypeTraits& traits, std::span<const Type* const> candidates,
    const ConstInt& value) -> std::optional<TypedEnumeratorValue> {
  for (auto type : candidates) {
    auto converted = convertedEnumeratorValue(traits, type, value);
    if (converted) return TypedEnumeratorValue{type, *converted};
  }
  return std::nullopt;
}

[[nodiscard]] auto integralPromotionCandidates(Control* control)
    -> std::array<const Type*, 6> {
  return {
      control->getIntType(),         control->getUnsignedIntType(),
      control->getLongIntType(),     control->getUnsignedLongIntType(),
      control->getLongLongIntType(), control->getUnsignedLongLongIntType(),
  };
}

[[nodiscard]] auto widenedEnumeratorValue(Control* control,
                                          const TypeTraits& traits,
                                          const ConstInt& value)
    -> std::optional<TypedEnumeratorValue> {
  const auto candidates = integralPromotionCandidates(control);
  return firstRepresentingType(traits, candidates, value);
}

[[nodiscard]] auto widenedCEnumeratorValue(Control* control,
                                           const TypeTraits& traits,
                                           const Type* previousType,
                                           const ConstInt& value)
    -> std::optional<TypedEnumeratorValue> {
  const auto representation = traits.integral_representation(previousType);
  if (!representation) return std::nullopt;

  if (representation->isSigned) {
    const std::array<const Type*, 3> candidates = {
        control->getIntType(), control->getLongIntType(),
        control->getLongLongIntType()};
    return firstRepresentingType(traits, candidates, value);
  }

  const std::array<const Type*, 3> candidates = {
      control->getUnsignedIntType(), control->getUnsignedLongIntType(),
      control->getUnsignedLongLongIntType()};
  return firstRepresentingType(traits, candidates, value);
}

struct EnumerationValueRange {
  // Smallest resolved enumerator value, or no value for an empty enumeration.
  std::optional<ConstInt> minimum;
  // Largest resolved enumerator value, or no value for an empty enumeration.
  std::optional<ConstInt> maximum;
};

struct CompletedEnumerationTypes {
  // Integer type used for object representation.
  const Type* underlyingType = nullptr;
  // Integer type produced by arithmetic promotion.
  const Type* promotionType = nullptr;
  // Type assigned to the enumeration constants after completion.
  const Type* memberType = nullptr;
  // Whether every enumerator has a resolved integral value.
  bool valuesResolved = false;
};

[[nodiscard]] auto enumerationValueRange(EnumSpecifierAST* ast)
    -> std::optional<EnumerationValueRange> {
  EnumerationValueRange range;
  for (auto enumerator : ListView{ast->enumeratorList}) {
    if (!enumerator->symbol || !enumerator->symbol->value())
      return std::nullopt;
    auto value = std::get_if<ConstInt>(&*enumerator->symbol->value());
    if (!value) return std::nullopt;
    if (!range.minimum || *value < *range.minimum) range.minimum = *value;
    if (!range.maximum || *value > *range.maximum) range.maximum = *value;
  }
  return range;
}

[[nodiscard]] auto firstTypeRepresentingRange(
    const TypeTraits& traits, const EnumerationValueRange& range,
    std::span<const Type* const> candidates) -> const Type* {
  for (auto type : candidates) {
    if (range.minimum &&
        !convertedEnumeratorValue(traits, type, *range.minimum))
      continue;
    if (range.maximum &&
        !convertedEnumeratorValue(traits, type, *range.maximum))
      continue;
    return type;
  }
  return nullptr;
}

[[nodiscard]] auto completedEnumerationTypes(Control* control,
                                             const TypeTraits& traits,
                                             EnumSpecifierAST* ast,
                                             EnumSymbol* symbol, bool isC)
    -> CompletedEnumerationTypes {
  auto range = enumerationValueRange(ast);
  if (!range) return {};

  const Type* underlyingType = nullptr;
  if (!ast->enumeratorList && !symbol->isPacked()) {
    underlyingType = control->getIntType();
  } else if (symbol->isPacked()) {
    const std::array<const Type*, 5> signedCandidates = {
        control->getSignedCharType(),  control->getShortIntType(),
        control->getIntType(),         control->getLongIntType(),
        control->getLongLongIntType(),
    };
    const std::array<const Type*, 5> unsignedCandidates = {
        control->getUnsignedCharType(),
        control->getUnsignedShortIntType(),
        control->getUnsignedIntType(),
        control->getUnsignedLongIntType(),
        control->getUnsignedLongLongIntType(),
    };
    const auto candidates =
        range->minimum && range->minimum->isNegative()
            ? std::span<const Type* const>{signedCandidates}
            : std::span<const Type* const>{unsignedCandidates};
    underlyingType = firstTypeRepresentingRange(traits, *range, candidates);
  } else {
    const std::array<const Type*, 3> signedCandidates = {
        control->getIntType(),
        control->getLongIntType(),
        control->getLongLongIntType(),
    };
    const std::array<const Type*, 3> unsignedCandidates = {
        control->getUnsignedIntType(),
        control->getUnsignedLongIntType(),
        control->getUnsignedLongLongIntType(),
    };
    const auto candidates =
        range->minimum && range->minimum->isNegative()
            ? std::span<const Type* const>{signedCandidates}
            : std::span<const Type* const>{unsignedCandidates};
    underlyingType = firstTypeRepresentingRange(traits, *range, candidates);
  }

  if (!underlyingType) return {.valuesResolved = true};

  const Type* promotionType = nullptr;
  const Type* memberType = symbol->type();
  if (isC) {
    const std::array<const Type*, 2> narrowPromotionCandidates = {
        control->getIntType(), control->getUnsignedIntType()};
    const auto underlyingRank = traits.integer_conversion_rank(underlyingType);
    const auto intRank = traits.integer_conversion_rank(control->getIntType());
    promotionType = underlyingRank > intRank
                        ? underlyingType
                        : firstTypeRepresentingRange(traits, *range,
                                                     narrowPromotionCandidates);

    const std::array<const Type*, 1> intCandidate = {control->getIntType()};
    if (firstTypeRepresentingRange(traits, *range, intCandidate))
      memberType = control->getIntType();
  } else {
    const auto promotionCandidates = integralPromotionCandidates(control);
    promotionType =
        firstTypeRepresentingRange(traits, *range, promotionCandidates);
  }

  return {underlyingType, promotionType, memberType, true};
}

[[nodiscard]] auto cxxProvisionalInitializerType(const TypeTraits& traits,
                                                 const Type* expressionType)
    -> const Type* {
  auto type = traits.remove_cvref(expressionType);
  if (traits.is_enum(type) && !traits.is_scoped_enum(type))
    return traits.underlying_type(type);
  return type;
}
}  // namespace

void Binder::bind(EnumeratorAST* ast, EnumeratorSymbol* previous) {
  auto fixedType = fixedUnderlyingType(scope());
  const Type* type = fixedType
                         ? (isC() ? scope()->type() : fixedType)
                         : static_cast<const Type*>(control()->getIntType());
  std::optional<ConstValue> value;

  if (ast->expression) {
    auto expressionType = traits.remove_cvref(ast->expression->type);
    if (!fixedType && expressionType && isCxx())
      type = cxxProvisionalInitializerType(traits, expressionType);

    const bool expressionDependent = isDependent(unit_, ast->expression);
    if (!expressionDependent &&
        !traits.is_integral_or_unscoped_enum(expressionType)) {
      error(ast->identifierLoc,
            "enumerator initializer must have integral or unscoped "
            "enumeration type");
    } else if (!expressionDependent) {
      auto evaluated = ASTInterpreter{unit_, scope()}.evaluate(ast->expression);
      auto integer = evaluated ? std::get_if<ConstInt>(&*evaluated) : nullptr;
      if (!integer) {
        error(ast->identifierLoc,
              "enumerator initializer is not an integral constant "
              "expression");
      } else if (fixedType && !isDependent(unit_, fixedType)) {
        auto converted = convertedEnumeratorValue(traits, fixedType, *integer);
        if (!converted) {
          error(ast->identifierLoc,
                "enumerator value is not representable in the underlying "
                "type");
        } else {
          value = *converted;
        }
      } else if (isC()) {
        auto intValue =
            convertedEnumeratorValue(traits, control()->getIntType(), *integer);
        if (intValue) {
          type = control()->getIntType();
          value = *intValue;
        } else if (expressionType) {
          type = expressionType;
          auto converted = convertedEnumeratorValue(traits, type, *integer);
          value = converted ? ConstValue{*converted} : ConstValue{*integer};
        } else {
          value = *integer;
        }
      } else if (type && !isDependent(unit_, type)) {
        auto converted = convertedEnumeratorValue(traits, type, *integer);
        value = converted ? ConstValue{*converted} : ConstValue{*integer};
      } else {
        value = *integer;
      }
    }
  } else if (!previous) {
    auto zero = ConstInt{0};
    if (fixedType && !isDependent(unit_, fixedType)) {
      value = *convertedEnumeratorValue(traits, fixedType, zero);
    } else {
      value = zero;
    }
  } else {
    if (!fixedType) type = previous->type();
    const Type* valueType = fixedType ? fixedType : type;
    auto previousValue = previous->value()
                             ? std::get_if<ConstInt>(&*previous->value())
                             : nullptr;
    if (previousValue && !isDependent(unit_, valueType)) {
      auto incremented = incrementedValue(*previousValue);
      if (!incremented) {
        error(ast->identifierLoc,
              "no integral type can represent the incremented enumerator "
              "value");
      } else if (auto converted = convertedEnumeratorValue(traits, valueType,
                                                           *incremented)) {
        value = *converted;
      } else if (fixedType) {
        error(ast->identifierLoc,
              "incremented enumerator value is not representable in the "
              "underlying type");
      } else if (auto widened = isC()
                                    ? widenedCEnumeratorValue(
                                          control(), traits, type, *incremented)
                                    : widenedEnumeratorValue(control(), traits,
                                                             *incremented)) {
        type = widened->type;
        value = widened->value;
      } else {
        error(ast->identifierLoc,
              "no integral type can represent the incremented enumerator "
              "value");
      }
    }
  }

  auto owningScope = isC() ? scope()->parent() : scope();
  auto symbol = control()->newEnumeratorSymbol(owningScope, ast->identifierLoc);
  ast->symbol = symbol;
  symbol->setName(ast->identifier);
  symbol->setType(type);
  symbol->setValue(value);
  if (auto enclosingEnum = symbol_cast<EnumSymbol>(scope()))
    symbol->setAccessSpecifier(enclosingEnum->accessSpecifier());
  if (auto enclosingEnum = symbol_cast<ScopedEnumSymbol>(scope()))
    symbol->setAccessSpecifier(enclosingEnum->accessSpecifier());
  owningScope->addSymbol(symbol);

  if (isC()) return;

  if (auto enumSymbol = symbol_cast<EnumSymbol>(scope())) {
    auto parentScope = enumSymbol->parent();
    auto usingDeclaration =
        control()->newUsingDeclarationSymbol(parentScope, ast->identifierLoc);
    usingDeclaration->setName(ast->identifier);
    usingDeclaration->setTarget(symbol);
    parentScope->addSymbol(usingDeclaration);
    applyAccessSpecifier(usingDeclaration);
  }
}

void Binder::complete(EnumSpecifierAST* ast) {
  if (auto symbol = symbol_cast<EnumSymbol>(ast->symbol)) {
    symbol->setDefined(true);
  } else if (auto symbol = symbol_cast<ScopedEnumSymbol>(ast->symbol)) {
    symbol->setDefined(true);
  }

  auto unscoped = symbol_cast<EnumSymbol>(ast->symbol);
  const Type* memberType = ast->symbol ? ast->symbol->type() : nullptr;
  if (unscoped && !unscoped->hasFixedUnderlyingType()) {
    const auto completed =
        completedEnumerationTypes(control(), traits, ast, unscoped, isC());
    if (completed.underlyingType && completed.promotionType) {
      unscoped->setUnderlyingType(completed.underlyingType);
      unscoped->setPromotionType(completed.promotionType);
      memberType = completed.memberType;
    } else if (completed.valuesResolved) {
      error(ast->enumLoc,
            "no integral type can represent all enumerator values");
    }
  }

  if (!ast->symbol) return;
  for (auto enumerator : ListView{ast->enumeratorList}) {
    if (enumerator->symbol) enumerator->symbol->setType(memberType);
  }
}

}  // namespace cxx
