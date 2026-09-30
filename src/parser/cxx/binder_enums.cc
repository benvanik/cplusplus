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

struct CompletedEnumerationTypes {
  // Integer type used for object representation.
  const Type* underlyingType = nullptr;
  // Range-based arithmetic promotion.
  const Type* promotionType = nullptr;
  // Whether every enumerator has a resolved integral value.
  bool valuesResolved = false;
};

[[nodiscard]] auto enumerationHasNegativeValue(EnumSpecifierAST* ast)
    -> std::optional<bool> {
  bool hasNegativeValue = false;
  for (auto enumerator : ListView{ast->enumeratorList}) {
    if (!enumerator->symbol || !enumerator->symbol->value())
      return std::nullopt;
    auto value = std::get_if<ConstInt>(&*enumerator->symbol->value());
    if (!value) return std::nullopt;
    hasNegativeValue |= value->isNegative();
  }
  return hasNegativeValue;
}

[[nodiscard]] auto firstTypeRepresentingEnumerators(
    const TypeTraits& traits, EnumSpecifierAST* ast,
    std::span<const Type* const> candidates) -> const Type* {
  for (auto type : candidates) {
    bool representsAllValues = true;
    for (auto enumerator : ListView{ast->enumeratorList}) {
      auto value = std::get_if<ConstInt>(&*enumerator->symbol->value());
      if (!convertedEnumeratorValue(traits, type, *value)) {
        representsAllValues = false;
        break;
      }
    }
    if (representsAllValues) return type;
  }
  return nullptr;
}

[[nodiscard]] auto completedEnumerationTypes(Control* control,
                                             const TypeTraits& traits,
                                             EnumSpecifierAST* ast, bool packed)
    -> CompletedEnumerationTypes {
  auto hasNegativeValue = enumerationHasNegativeValue(ast);
  if (!hasNegativeValue.has_value()) return {};

  const Type* underlyingType = nullptr;
  if (!ast->enumeratorList && !packed) {
    underlyingType = control->getIntType();
  } else if (packed) {
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
        *hasNegativeValue ? std::span<const Type* const>{signedCandidates}
                          : std::span<const Type* const>{unsignedCandidates};
    underlyingType = firstTypeRepresentingEnumerators(traits, ast, candidates);
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
        *hasNegativeValue ? std::span<const Type* const>{signedCandidates}
                          : std::span<const Type* const>{unsignedCandidates};
    underlyingType = firstTypeRepresentingEnumerators(traits, ast, candidates);
  }

  const auto promotionCandidates = integralPromotionCandidates(control);
  auto promotionType =
      firstTypeRepresentingEnumerators(traits, ast, promotionCandidates);
  return {underlyingType, promotionType, true};
}

[[nodiscard]] auto provisionalInitializerType(const TypeTraits& traits,
                                              const Type* expressionType)
    -> const Type* {
  auto type = traits.remove_cvref(expressionType);
  if (traits.is_enum(type) && !traits.is_scoped_enum(type))
    return traits.underlying_type(type);
  return type;
}
}  // namespace

void Binder::bind(EnumeratorAST* ast, EnumeratorSymbol* previous) {
  if (isC()) {
    std::optional<ConstValue> value;
    if (ast->expression) {
      value = ASTInterpreter{unit_, scope()}.evaluate(ast->expression);
    } else if (!previous) {
      value = ConstInt{std::intmax_t{0}};
    } else if (previous->value()) {
      if (auto integer = std::get_if<ConstInt>(&*previous->value()))
        value = incrementedValue(*integer);
    }

    auto enumSymbol = symbol_cast<EnumSymbol>(scope());
    auto parentScope = enumSymbol->parent();
    auto symbol =
        control()->newEnumeratorSymbol(parentScope, ast->identifierLoc);
    ast->symbol = symbol;
    symbol->setName(ast->identifier);
    symbol->setType(scope()->type());
    symbol->setValue(value);
    parentScope->addSymbol(symbol);
    return;
  }

  auto fixedType = fixedUnderlyingType(scope());
  const Type* type = fixedType ? fixedType : control()->getIntType();
  std::optional<ConstValue> value;

  if (ast->expression) {
    auto expressionType = traits.remove_cvref(ast->expression->type);
    if (!fixedType && expressionType)
      type = provisionalInitializerType(traits, expressionType);

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
    auto previousValue = previous->value()
                             ? std::get_if<ConstInt>(&*previous->value())
                             : nullptr;
    if (previousValue && !isDependent(unit_, type)) {
      auto incremented = incrementedValue(*previousValue);
      if (!incremented) {
        error(ast->identifierLoc,
              "no integral type can represent the incremented enumerator "
              "value");
      } else if (auto converted =
                     convertedEnumeratorValue(traits, type, *incremented)) {
        value = *converted;
      } else if (fixedType) {
        error(ast->identifierLoc,
              "incremented enumerator value is not representable in the "
              "underlying type");
      } else if (auto widened =
                     widenedEnumeratorValue(control(), traits, *incremented)) {
        type = widened->type;
        value = widened->value;
      } else {
        error(ast->identifierLoc,
              "no integral type can represent the incremented enumerator "
              "value");
      }
    }
  }

  auto symbol = control()->newEnumeratorSymbol(scope(), ast->identifierLoc);
  ast->symbol = symbol;
  symbol->setName(ast->identifier);
  symbol->setType(type);
  symbol->setValue(value);
  if (auto enclosingEnum = symbol_cast<EnumSymbol>(scope()))
    symbol->setAccessSpecifier(enclosingEnum->accessSpecifier());
  if (auto enclosingEnum = symbol_cast<ScopedEnumSymbol>(scope()))
    symbol->setAccessSpecifier(enclosingEnum->accessSpecifier());
  scope()->addSymbol(symbol);

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
  if (isCxx()) {
    auto unscoped = symbol_cast<EnumSymbol>(ast->symbol);
    if (unscoped && !unscoped->hasFixedUnderlyingType()) {
      const auto completed = completedEnumerationTypes(control(), traits, ast,
                                                       unscoped->isPacked());
      if (completed.underlyingType && completed.promotionType) {
        unscoped->setUnderlyingType(completed.underlyingType);
        unscoped->setPromotionType(completed.promotionType);
      } else if (completed.valuesResolved) {
        error(ast->enumLoc,
              "no integral type can represent all enumerator values");
      }
    }
  }

  if (!ast->symbol) return;
  for (auto enumerator : ListView{ast->enumeratorList}) {
    if (enumerator->symbol) enumerator->symbol->setType(ast->symbol->type());
  }
}

}  // namespace cxx
