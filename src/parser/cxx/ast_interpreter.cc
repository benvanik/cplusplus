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
#include <cxx/ast_rewriter.h>
#include <cxx/control.h>
#include <cxx/floating_point.h>
#include <cxx/initialization.h>
#include <cxx/literals.h>
#include <cxx/memory_layout.h>
#include <cxx/names.h>
#include <cxx/parser.h>
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <cxx/types.h>
#include <cxx/views/symbols.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

namespace cxx {
namespace {
struct ToInt {
  auto operator()(ConstFloat value) const -> std::optional<std::intmax_t> {
    return static_cast<std::intmax_t>(value.toLongDouble());
  }

  auto operator()(bool v) const -> std::optional<std::intmax_t> {
    return v ? 1 : 0;
  }

  auto operator()(ConstInt v) const -> std::optional<std::intmax_t> {
    return v.toIntMax();
  }

  auto operator()(long double v) const -> std::optional<std::intmax_t> {
    return static_cast<std::intmax_t>(v);
  }

  auto operator()(const std::shared_ptr<ConstComplex>& v) const
      -> std::optional<std::intmax_t> {
    if (!v) return std::nullopt;
    return std::visit(*this, v->real());
  }

  auto operator()(auto x) const -> std::optional<std::intmax_t> {
    return std::nullopt;
  }
};

struct ToUInt {
  auto operator()(ConstFloat value) const -> std::optional<std::uintmax_t> {
    return static_cast<std::uintmax_t>(value.toLongDouble());
  }

  auto operator()(bool v) const -> std::optional<std::uintmax_t> {
    return v ? 1 : 0;
  }

  auto operator()(ConstInt v) const -> std::optional<std::uintmax_t> {
    return static_cast<std::uintmax_t>(v.toWideValue());
  }

  auto operator()(long double v) const -> std::optional<std::uintmax_t> {
    return static_cast<std::uintmax_t>(v);
  }

  auto operator()(const std::shared_ptr<ConstComplex>& v) const
      -> std::optional<std::uintmax_t> {
    if (!v) return std::nullopt;
    return std::visit(*this, v->real());
  }

  auto operator()(auto x) const -> std::optional<std::uintmax_t> {
    return std::nullopt;
  }
};

template <typename T>
struct ArithmeticCast {
  auto operator()(ConstFloat value) const -> T {
    return static_cast<T>(value.toLongDouble());
  }

  auto operator()(const StringLiteral*) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(const std::shared_ptr<Meta>&) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(const std::shared_ptr<InitializerList>&) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(const std::shared_ptr<ConstObject>&) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(const std::shared_ptr<ConstAddress>&) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(const std::shared_ptr<ConstLabelAddress>&) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(IndeterminateValue) const -> T {
    cxx_runtime_error("invalid artihmetic cast");
    return T{};
  }

  auto operator()(const std::shared_ptr<ConstComplex>& value) const -> T {
    if (!value) return T{};
    return std::visit(*this, value->real());
  }

  auto operator()(ConstInt value) const -> T {
    if (value.isSigned()) return static_cast<T>(value.toIntMax());
    return static_cast<T>(value.toUIntMax());
  }

  auto operator()(auto value) const -> T { return static_cast<T>(value); }
};
}  // namespace

struct ASTInterpreter::ToBool {
  ASTInterpreter& interp;

  auto operator()(ConstFloat value) const -> std::optional<bool> {
    return value.toLongDouble() != 0;
  }

  auto operator()(const StringLiteral*) const -> std::optional<bool> {
    return true;
  }

  auto operator()(const Meta&) const -> std::optional<bool> {
    return std::nullopt;
  }

  auto operator()(const std::shared_ptr<ConstObject>&) const
      -> std::optional<bool> {
    return std::nullopt;
  }

  auto operator()(const std::shared_ptr<ConstAddress>&) const
      -> std::optional<bool> {
    return true;
  }

  auto operator()(IndeterminateValue) const -> std::optional<bool> {
    return std::nullopt;
  }

  auto operator()(const std::shared_ptr<ConstComplex>& value) const
      -> std::optional<bool> {
    if (!value) return std::nullopt;
    auto real = std::visit(*this, value->real());
    auto imag = std::visit(*this, value->imag());
    if (!real || !imag) return std::nullopt;
    return *real || *imag;
  }

  auto operator()(ConstInt value) const -> std::optional<bool> {
    return !value.isZero();
  }

  auto operator()(const auto& value) const -> std::optional<bool> {
    return bool(value);
  }
};

ASTInterpreter::ASTInterpreter(TranslationUnit* unit, ScopeSymbol* scope)
    : unit_(unit), traits(unit) {
  if (scope) currentFunction_ = scope->enclosingFunctionOrSelf();
}

ASTInterpreter::~ASTInterpreter() {}

auto ASTInterpreter::cloneValue(const ConstValue& value) -> ConstValue {
  if (auto object = std::get_if<std::shared_ptr<ConstObject>>(&value)) {
    if (!*object) return value;
    auto copy = std::make_shared<ConstObject>((*object)->type());
    for (const auto& member : (*object)->members())
      copy->addMember(member.symbol,
                      cloneValueOfType(member.value, member.symbol->type()));
    return ConstValue{std::move(copy)};
  }

  if (auto list = std::get_if<std::shared_ptr<InitializerList>>(&value)) {
    if (!*list) return value;
    auto copy = std::make_shared<InitializerList>();
    for (const auto& [element, type] : (*list)->elements)
      copy->elements.emplace_back(cloneValueOfType(element, type), type);
    return ConstValue{std::move(copy)};
  }

  if (auto complexValue = std::get_if<std::shared_ptr<ConstComplex>>(&value)) {
    if (!*complexValue) return value;
    return ConstValue{
        std::make_shared<ConstComplex>(cloneValue((*complexValue)->real()),
                                       cloneValue((*complexValue)->imag()))};
  }

  return value;
}

auto ASTInterpreter::cloneValueOfType(const ConstValue& value, const Type* type)
    -> ConstValue {
  if (traits.is_pointer(type)) return value;
  return cloneValue(value);
}

auto isFullyInitialized(const ConstValue& value) -> bool {
  if (std::holds_alternative<IndeterminateValue>(value)) return false;

  if (auto object = std::get_if<std::shared_ptr<ConstObject>>(&value)) {
    if (!*object) return false;
    if ((*object)->isConstexprUnknown()) return false;
    for (const auto& member : (*object)->members()) {
      if (!isFullyInitialized(member.value)) return false;
    }
  }

  if (auto list = std::get_if<std::shared_ptr<InitializerList>>(&value)) {
    if (!*list) return false;
    for (const auto& [element, type] : (*list)->elements) {
      if (!isFullyInitialized(element)) return false;
    }
  }

  if (auto address = std::get_if<std::shared_ptr<ConstAddress>>(&value)) {
    for (auto current = *address; current; current = current->parent()) {
      if (current->storage() && !current->storage()->slot()) return false;
    }
  }

  if (auto complexValue = std::get_if<std::shared_ptr<ConstComplex>>(&value)) {
    if (!*complexValue) return false;
    if (!isFullyInitialized((*complexValue)->real())) return false;
    if (!isFullyInitialized((*complexValue)->imag())) return false;
  }

  return true;
}

auto ASTInterpreter::control() const -> Control* { return unit_->control(); }

auto ASTInterpreter::evaluate(ExpressionAST* ast) -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  auto result = expression(ast);
  return result;
}

auto ASTInterpreter::evaluateAddress(ExpressionAST* ast)
    -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  return addressOfLvalue(ast);
}

auto ASTInterpreter::evaluateStaticDataMember(FieldSymbol* field)
    -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  return evaluateStaticField(field);
}

auto ASTInterpreter::toBool(const ConstValue& value) -> std::optional<bool> {
  return std::visit(ToBool{*this}, value);
}

auto ASTInterpreter::toInt(const ConstValue& value)
    -> std::optional<std::intmax_t> {
  return std::visit(ToInt{}, value);
}

auto ASTInterpreter::memberObjectPointerOffset(const ConstValue& value) const
    -> std::optional<std::intmax_t> {
  auto address = std::get_if<std::shared_ptr<ConstAddress>>(&value);
  if (!address || !*address) return std::nullopt;
  if (!(*address)->symbol())
    return control()->memoryLayout()->nullMemberObjectPointer();
  return (*address)->offset();
}

auto ASTInterpreter::toUInt(const ConstValue& value)
    -> std::optional<std::uintmax_t> {
  return std::visit(ToUInt{}, value);
}

auto ASTInterpreter::toIntegralType(const ConstValue& value, const Type* type)
    -> std::optional<ConstValue> {
  auto traits = translationUnit()->typeTraits();

  auto representation = traits.integral_representation(type);
  if (!representation) return std::nullopt;

  ConstInt::Wide raw = 0;

  if (auto stored = std::get_if<ConstInt>(&value)) {
    raw = stored->toWideValue();
  } else if (representation->isSigned) {
    auto result = toInt(value);
    if (!result.has_value()) return std::nullopt;
    raw = static_cast<ConstInt::Wide>(*result);
  } else {
    auto result = toUInt(value);
    if (!result.has_value()) return std::nullopt;
    raw = static_cast<ConstInt::Wide>(static_cast<ConstInt::UWide>(*result));
  }

  auto converted = traits.integral_constant(type, raw);
  if (!converted) return std::nullopt;

  return ConstValue{*converted};
}

auto ASTInterpreter::convertArithmetic(const ConstValue& value,
                                       const Type* sourceType,
                                       const Type* targetType)
    -> std::optional<ConstValue> {
  sourceType = traits.remove_cv(sourceType);
  targetType = traits.remove_cv(targetType);

  if (!sourceType || !targetType) return std::nullopt;

  if (traits.is_same(targetType, control()->getBoolType())) {
    auto converted = toBool(value);
    if (!converted) return std::nullopt;
    auto constant = traits.integral_constant(targetType, *converted);
    if (!constant) return std::nullopt;
    return ConstValue{*constant};
  }

  if (traits.is_integral_or_enum(sourceType) &&
      traits.is_floating_point(targetType)) {
    auto integer = std::get_if<ConstInt>(&value);
    auto representation = traits.integral_representation(sourceType);
    if (!integer || !representation) return std::nullopt;

    const auto bits = integer->toUWide();
    const auto signedValue = integer->toWide();
    const bool isUnsigned = !representation->isSigned;

    switch (targetType->kind()) {
      case TypeKind::kFloat: {
        const auto number = isUnsigned ? static_cast<float>(bits)
                                       : static_cast<float>(signedValue);
        return ConstValue{
            ConstFloat::fromValue(ConstFloat::Format::kFloat, number)};
      }
      case TypeKind::kDouble: {
        const auto number = isUnsigned ? static_cast<double>(bits)
                                       : static_cast<double>(signedValue);
        return ConstValue{
            ConstFloat::fromValue(ConstFloat::Format::kDouble, number)};
      }
      case TypeKind::kLongDouble:
        return ConstValue{isUnsigned ? static_cast<long double>(bits)
                                     : static_cast<long double>(signedValue)};
      default:
        break;
    }

    const bool isNegative = !isUnsigned && integer->isNegative();
    auto magnitude = isNegative ? integer->magnitude() : bits;

    int bitWidth = 0;
    for (auto remaining = magnitude; remaining; remaining >>= 1) ++bitWidth;

    // Preserve discarded integer bits with round-to-odd before the final
    // narrow rounding, including hosts where long double is only binary64.
    constexpr int precision = std::numeric_limits<double>::digits;
    const int shift = std::max(0, bitWidth - precision);
    auto leading = magnitude >> shift;
    if (shift) {
      const auto discardedMask = (ConstInt::UWide{1} << shift) - 1;
      if (magnitude & discardedMask) leading |= 1;
    }

    auto intermediate = std::ldexp(static_cast<long double>(leading), shift);
    if (isNegative) intermediate = -intermediate;
    return toArithmeticType(ConstValue{intermediate}, targetType);
  }

  if (traits.is_floating_point(sourceType) &&
      traits.is_integral_or_enum(targetType)) {
    auto number = toLongDouble(value);
    auto representation = traits.integral_representation(targetType);
    if (!number || !representation || !std::isfinite(*number))
      return std::nullopt;

    const auto truncated = std::trunc(*number);
    const auto maximum =
        std::ldexp(1.0L, representation->bits - representation->isSigned);
    const auto minimum = representation->isSigned ? -maximum : 0.0L;
    if (truncated < minimum || truncated >= maximum) return std::nullopt;

    ConstInt::Wide converted = 0;
    if (representation->isSigned) {
      converted = static_cast<ConstInt::Wide>(truncated);
    } else {
      converted =
          static_cast<ConstInt::Wide>(static_cast<ConstInt::UWide>(truncated));
    }

    auto constant = traits.integral_constant(targetType, converted);
    if (!constant) return std::nullopt;
    return ConstValue{*constant};
  }

  return toArithmeticType(value, targetType);
}

auto ASTInterpreter::toArithmeticType(const ConstValue& value, const Type* type)
    -> std::optional<ConstValue> {
  if (!type) return std::nullopt;

  const auto holdsArithmetic =
      std::holds_alternative<ConstInt>(value) ||
      std::holds_alternative<ConstFloat>(value) ||
      std::holds_alternative<long double>(value) ||
      std::holds_alternative<std::shared_ptr<ConstComplex>>(value);

  if (!holdsArithmetic) return std::nullopt;

  type = traits.remove_cv(type);

  if (auto complexType = type_cast<ComplexType>(type)) {
    auto elementType = complexType->elementType();
    if (auto complexValue =
            std::get_if<std::shared_ptr<ConstComplex>>(&value)) {
      if (!*complexValue) return std::nullopt;
      auto real = toArithmeticType((*complexValue)->real(), elementType);
      auto imag = toArithmeticType((*complexValue)->imag(), elementType);
      if (!real || !imag) return std::nullopt;
      return ConstValue{std::make_shared<ConstComplex>(*real, *imag)};
    }

    auto real = toArithmeticType(value, elementType);
    auto zero = zeroInitialize(elementType);
    if (!real || !zero) return std::nullopt;
    return ConstValue{std::make_shared<ConstComplex>(*real, *zero)};
  }

  if (auto format = ConstFloat::formatFor(type->kind())) {
    if (auto stored = std::get_if<ConstFloat>(&value);
        stored && stored->format() == *format) {
      return value;
    }

    auto result = toLongDouble(value);
    if (!result) return std::nullopt;
    return ConstValue{ConstFloat::fromValue(*format, *result)};
  }

  switch (type->kind()) {
    case TypeKind::kLongDouble: {
      auto result = toLongDouble(value);
      if (!result) return std::nullopt;
      return ConstValue{*result};
    }

    default:
      break;
  }

  return toIntegralType(value, type);
}

auto ASTInterpreter::toFloat(const ConstValue& value) -> std::optional<float> {
  return std::visit(ArithmeticCast<float>{}, value);
}

auto ASTInterpreter::toDouble(const ConstValue& value)
    -> std::optional<double> {
  return std::visit(ArithmeticCast<double>{}, value);
}

auto ASTInterpreter::toLongDouble(const ConstValue& value)
    -> std::optional<long double> {
  return std::visit(ArithmeticCast<long double>{}, value);
}

auto ASTInterpreter::lookupLocal(const Symbol* sym)
    -> std::optional<ConstValue> {
  auto slot = lookupLocalSlot(sym);
  if (!slot) return std::nullopt;
  if (asConstexprUnknownObject(*slot)) return std::nullopt;
  return *slot;
}

auto ASTInterpreter::lookupLocalSlot(const Symbol* sym) -> ConstValue* {
  for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
    auto ref = it->refs.find(sym);
    if (ref != it->refs.end()) return ref->second;
    auto found = it->locals.find(sym);
    if (found != it->locals.end()) return &found->second.value;
    if (it->referenceAddresses.contains(sym)) return nullptr;
  }
  return nullptr;
}

auto ASTInterpreter::materializedTemporary(ExpressionAST* ast)
    -> ExpressionAST* {
  while (auto nested = ast_cast<NestedExpressionAST>(ast))
    ast = nested->expression;
  if (!is_glvalue(ast)) return ast;
  auto cast = ast_cast<ImplicitCastExpressionAST>(ast);
  if (!cast) return nullptr;
  if (cast->castKind != ImplicitCastKind::kTemporaryMaterializationConversion)
    return nullptr;
  return cast->expression;
}

auto ASTInterpreter::bindReferenceTo(Frame& frame, Symbol* reference,
                                     ExpressionAST* initializer) -> bool {
  if (auto temporary = materializedTemporary(initializer)) {
    auto value = evaluate(temporary);
    if (!value) return false;
    auto storage = std::make_shared<ConstObject>(
        traits.remove_reference(reference->type()));
    storage->addMember(reference, cloneValue(*value));
    return bindReferenceAddress(
        frame, reference, std::make_shared<ConstAddress>(storage, reference));
  }

  if (auto value = addressOfLvalue(initializer)) {
    if (bindReferenceAddress(frame, reference, std::move(*value))) return true;
  }

  auto slot = lvalue(initializer);
  if (!slot) return false;
  frame.refs.insert_or_assign(reference, slot);
  return true;
}

auto ASTInterpreter::bindReferenceAddress(Frame& frame, const Symbol* reference,
                                          ConstValue address) -> bool {
  auto pointer = std::get_if<std::shared_ptr<ConstAddress>>(&address);
  if (!pointer || !*pointer) return false;

  auto slot = addressSlot(**pointer, 0, reference->type());
  auto referent = slot ? std::optional<ConstValue>{}
                       : loadAddress(**pointer, 0, reference->type());
  frame.referenceAddresses.insert_or_assign(reference, std::move(address));
  if (slot)
    frame.refs.insert_or_assign(reference, slot);
  else if (referent)
    frame.locals.insert_or_assign(reference,
                                  Frame::Local{std::move(*referent)});
  return true;
}

auto ASTInterpreter::bindReference(const Symbol* sym, ConstValue address)
    -> bool {
  if (frames_.empty()) frames_.push_back({});
  return bindReferenceAddress(frames_.back(), sym, std::move(address));
}

void ASTInterpreter::setLocal(const Symbol* sym, ConstValue value) {
  if (frames_.empty()) frames_.push_back({});
  frames_.back().locals.insert_or_assign(sym, Frame::Local{std::move(value)});
}

void ASTInterpreter::setAutomaticLocal(const Symbol* sym, ConstValue value) {
  if (frames_.empty()) frames_.push_back({});
  frames_.back().locals.insert_or_assign(
      sym, Frame::Local{std::move(value), currentAutomaticScope_});
}

auto ASTInterpreter::automaticAddress(AutomaticScope& scope, Symbol* symbol,
                                      ConstValue* slot)
    -> std::shared_ptr<ConstAddress> {
  auto storage = std::make_shared<ConstStorage>(slot);
  storage->automaticNext_ = std::move(scope.storageHead_);
  scope.storageHead_ = storage;
  auto address = std::make_shared<ConstAddress>(symbol);
  address->setStorage(std::move(storage));
  return address;
}

void ASTInterpreter::retireAutomaticStorage(AutomaticScope& scope) {
  while (scope.storageHead_) {
    auto storage = std::move(scope.storageHead_);
    scope.storageHead_ = std::move(storage->automaticNext_);
    storage->slot_ = nullptr;
  }
}

auto ASTInterpreter::localAddress(Frame& frame, Symbol* symbol)
    -> std::shared_ptr<ConstAddress> {
  auto found = frame.locals.find(symbol);
  if (found == frame.locals.end()) return {};
  auto& local = found->second;
  if (!local.declaringScope) return std::make_shared<ConstAddress>(symbol);

  if (auto cached = frame.referenceAddresses.find(symbol);
      cached != frame.referenceAddresses.end()) {
    auto address = std::get_if<std::shared_ptr<ConstAddress>>(&cached->second);
    if (address && *address && (*address)->storage() &&
        (*address)->storage()->slot())
      return *address;
  }

  auto address = automaticAddress(*local.declaringScope, symbol, &local.value);
  frame.referenceAddresses.insert_or_assign(symbol, address);
  return address;
}

auto ASTInterpreter::definingDeclarationOf(FunctionSymbol* function)
    -> FunctionSymbol* {
  auto definition = function->resolvedDefinition();
  (void)ASTRewriter::completePendingBodyFor(unit_, definition);
  return definition;
}

auto ASTInterpreter::bindParameters(Frame& frame, FunctionSymbol* func,
                                    std::vector<ConstValue>& args) -> bool {
  auto params = definingDeclarationOf(func)->parameters();
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (i < args.size()) {
      auto value = traits.is_reference(params[i]->type()) ? args[i]
                                                          : cloneValue(args[i]);
      frame.locals.insert_or_assign(params[i], Frame::Local{std::move(value)});
    } else {
      auto defaultArgument =
          ASTRewriter::requireDefaultArgument(unit_, params[i]);
      if (!defaultArgument) return false;
      if (!bindOneParameter(frame, params[i], defaultArgument)) return false;
    }
  }
  return true;
}

auto ASTInterpreter::bindOneParameter(Frame& frame, Symbol* paramSymbol,
                                      ExpressionAST* argExpr) -> bool {
  auto param = symbol_cast<ParameterSymbol>(paramSymbol);
  if (param && traits.is_reference(param->type()) &&
      bindReferenceTo(frame, param, argExpr)) {
    return true;
  }
  if (auto value = evaluate(argExpr)) {
    frame.locals.insert_or_assign(paramSymbol,
                                  Frame::Local{cloneValue(*value)});
    return true;
  }

  auto object = constexprUnknownObject(argExpr);
  if (!object) return false;
  frame.locals.insert_or_assign(paramSymbol, Frame::Local{std::move(object)});
  return true;
}

auto ASTInterpreter::bindParametersFromExprs(
    Frame& frame, FunctionSymbol* function,
    std::span<ExpressionAST* const> arguments) -> bool {
  auto parameters = definingDeclarationOf(function)->parameters();
  for (std::size_t i = 0; i < parameters.size(); ++i) {
    auto argument = i < arguments.size() ? arguments[i] : nullptr;
    if (!argument)
      argument = ASTRewriter::requireDefaultArgument(unit_, parameters[i]);
    if (!argument || !bindOneParameter(frame, parameters[i], argument))
      return false;
  }
  for (std::size_t i = parameters.size(); i < arguments.size(); ++i)
    if (!expression(arguments[i])) return false;
  return true;
}

auto ASTInterpreter::initializeDefaultedObject(
    const std::shared_ptr<ConstObject>& obj, ClassSymbol* classSymbol) -> bool {
  if (!obj || !classSymbol) return false;
  classSymbol = classSymbol->resolvedDefinition();
  auto savedThis = std::exchange(receiver_, Receiver{obj, {}});

  for (auto base : classSymbol->baseClasses()) {
    if (base->isVirtual()) continue;
    auto baseClass = symbol_cast<ClassSymbol>(base->symbol());
    if (!baseClass) continue;
    auto value =
        defaultConstruct(baseClass->type(), obj->mutableSubobject(base));
    if (!value) {
      receiver_ = std::move(savedThis);
      return false;
    }
    obj->setMember(base, std::move(*value));
  }

  for (auto member : classSymbol->members()) {
    auto field = symbol_cast<FieldSymbol>(member);
    if (!field || field->isStatic()) continue;
    if (field->isBitField() && !field->name()) continue;

    if (field->initializer()) {
      auto value = initializationValue(field->type(), field->constructor(),
                                       field->initializer(),
                                       obj->mutableSubobject(field));
      if (!value) {
        receiver_ = std::move(savedThis);
        return false;
      }
      obj->setMember(field, std::move(*value));
      if (classSymbol->isUnion()) break;
      continue;
    }

    // A union without a default member initializer has no construction step.
    // Value initialization retains the zero-initialized first member.
    if (classSymbol->isUnion()) continue;
    auto value = defaultConstruct(field->type(), obj->mutableSubobject(field));
    if (!value) {
      receiver_ = std::move(savedThis);
      return false;
    }
    obj->setMember(field, std::move(*value));
  }

  receiver_ = std::move(savedThis);
  return true;
}

auto ASTInterpreter::copyDefaultedObject(
    const std::shared_ptr<ConstObject>& obj,
    const std::shared_ptr<ConstObject>& source, ClassSymbol* classSymbol)
    -> bool {
  if (!obj || !source || !classSymbol) return false;
  classSymbol = classSymbol->resolvedDefinition();

  if (classSymbol->isUnion()) {
    for (const auto& member : source->members())
      obj->addMember(member.symbol, cloneValue(member.value));
    return true;
  }

  for (auto base : classSymbol->baseClasses()) {
    auto value = source->subobject(base);
    if (!value) return false;
    obj->addMember(base, cloneValue(*value));
  }

  for (auto member : classSymbol->members()) {
    auto field = symbol_cast<FieldSymbol>(member);
    if (!field || field->isStatic()) continue;
    if (field->isBitField() && !field->name()) continue;
    auto value = source->subobject(field);
    if (!value) return false;
    obj->setMember(field, cloneValue(*value));
  }

  return true;
}

auto ASTInterpreter::subobjectSlot(const std::shared_ptr<ConstObject>& object,
                                   const Symbol* symbol) -> ConstValue* {
  if (!object || !symbol) return nullptr;
  if (object->isConstexprUnknown()) return nullptr;

  if (auto slot = object->mutableSubobject(symbol)) return slot;

  auto classType = unqualified_cast<ClassType>(object->type());
  auto classSymbol = classType ? classType->symbol() : nullptr;
  if (classSymbol) classSymbol = classSymbol->resolvedDefinition();

  auto owner = symbol_cast<ClassSymbol>(symbol->parent());

  if (classSymbol && owner &&
      owner->resolvedDefinition() != classSymbol->resolvedDefinition()) {
    for (auto base : classSymbol->baseClasses()) {
      auto baseClass = symbol_cast<ClassSymbol>(base->symbol());
      if (!baseClass) continue;
      if (!traits.is_member_of_object_type(baseClass->type(),
                                           const_cast<Symbol*>(symbol)))
        continue;

      auto baseSlot = object->mutableSubobject(base);
      if (!baseSlot) {
        baseSlot = object->addMember(
            base, ConstValue{std::make_shared<ConstObject>(baseClass->type())});
      }

      auto nested = std::get_if<std::shared_ptr<ConstObject>>(baseSlot);
      if (!nested) continue;
      if (!*nested) *nested = std::make_shared<ConstObject>(baseClass->type());
      return subobjectSlot(*nested, symbol);
    }
  }

  return object->addMember(symbol, ConstValue{IndeterminateValue{}});
}

auto ASTInterpreter::copyArrayElements(const Type* type,
                                       FunctionSymbol* constructor,
                                       const ConstValue& source,
                                       ConstValue* storage)
    -> std::optional<ConstValue> {
  auto array = type_cast<BoundedArrayType>(traits.remove_cv(type));
  if (!array) {
    auto object =
        storage ? std::get_if<std::shared_ptr<ConstObject>>(storage) : nullptr;
    if (constructor)
      return evaluateConstructor(constructor, type, {source},
                                 object ? *object : nullptr);
    return cloneValue(source);
  }

  auto sourceElements = std::get_if<std::shared_ptr<InitializerList>>(&source);
  if (!sourceElements || !*sourceElements) return std::nullopt;
  auto retained = storage
                      ? std::get_if<std::shared_ptr<InitializerList>>(storage)
                      : nullptr;
  auto elements =
      retained && *retained ? *retained : std::make_shared<InitializerList>();
  const bool reuseStorage = retained && *retained;
  if (!reuseStorage) elements->elements.reserve(array->size());
  for (std::size_t index = 0; index < (*sourceElements)->elements.size();
       ++index) {
    const auto& [value, elementType] = (*sourceElements)->elements[index];
    auto slot =
        reuseStorage ? &std::get<0>(elements->elements[index]) : nullptr;
    auto element =
        copyArrayElements(array->elementType(), constructor, value, slot);
    if (!element) return std::nullopt;
    if (slot)
      *slot = std::move(*element);
    else
      elements->elements.emplace_back(std::move(*element), elementType);
  }
  return ConstValue{std::move(elements)};
}

auto ASTInterpreter::constructSubobject(
    MemInitializerAST* ast, const Type* type,
    const std::vector<ExpressionAST*>& arguments, ConstValue* storage)
    -> std::optional<ConstValue> {
  if (auto array = type_cast<BoundedArrayType>(traits.remove_cv(type))) {
    if (arguments.size() == 1 &&
        isWholeArrayCopy(traits, arguments.front(), type)) {
      auto source = expression(arguments.front());
      if (!source) return std::nullopt;
      return copyArrayElements(type, ast->constructor, *source, storage);
    }
    auto retained = storage
                        ? std::get_if<std::shared_ptr<InitializerList>>(storage)
                        : nullptr;
    auto elements =
        retained && *retained ? *retained : std::make_shared<InitializerList>();
    const bool reuseStorage = retained && *retained;
    if (!reuseStorage) elements->elements.reserve(array->size());
    for (std::size_t i = 0; i < array->size(); ++i) {
      auto slot = reuseStorage ? &std::get<0>(elements->elements[i]) : nullptr;
      auto element =
          constructSubobject(ast, array->elementType(), arguments, slot);
      if (!element) return std::nullopt;
      if (slot)
        *slot = std::move(*element);
      else
        elements->elements.emplace_back(std::move(*element),
                                        array->elementType());
    }
    return elements;
  }
  auto paren = ast_cast<ParenMemInitializerAST>(ast);
  const auto valueInitialized =
      paren && paren->lparenLoc && !paren->expressionList;

  if (valueInitialized &&
      traits.requires_zero_initialization(type, ast->constructor)) {
    auto retained =
        storage ? std::get_if<std::shared_ptr<ConstObject>>(storage) : nullptr;
    std::optional<ConstValue> zero;
    if (!retained || !*retained) {
      zero = zeroInitialize(type);
      if (!zero) return std::nullopt;
      retained = std::get_if<std::shared_ptr<ConstObject>>(&*zero);
    }
    if (!retained || !*retained) return std::nullopt;
    if (traits.is_trivially_constructible(type, {})) {
      if (zero) return zero;
      return *storage;
    }
    return evaluateConstructor(ast->constructor, type, {}, *retained);
  }

  auto object =
      storage ? std::get_if<std::shared_ptr<ConstObject>>(storage) : nullptr;
  return evaluateConstructorFromExprs(ast->constructor, type, arguments,
                                      object ? *object : nullptr);
}

void ASTInterpreter::applyMemInitializer(
    MemInitializerAST* ast, const std::vector<ExpressionAST*>& arguments) {
  if (!ast->symbol || !receiver_.object) return;

  if (auto cls = symbol_cast<ClassSymbol>(ast->symbol)) {
    if (cls != currentConstructorClass_) return;
    if (!ast->constructor) return;
    auto result =
        evaluateConstructorFromExprs(ast->constructor, receiver_.object->type(),
                                     arguments, receiver_.object);
    if (!result) aborted_ = true;
    return;
  }

  auto subobject = ast->symbol;
  if (auto base = symbol_cast<BaseClassSymbol>(subobject)) {
    if (base->isVirtual()) return;
    auto baseClassSym = symbol_cast<ClassSymbol>(base->symbol());
    if (!baseClassSym) return;
    initializeSubobject(ast, base, baseClassSym->type(), arguments);
    return;
  }

  auto field = symbol_cast<FieldSymbol>(subobject);
  if (!field) return;
  initializeSubobject(ast, field, field->type(), arguments);
}

void ASTInterpreter::initializeSubobject(
    MemInitializerAST* ast, Symbol* subobject, const Type* type,
    const std::vector<ExpressionAST*>& arguments) {
  if (!ast->constructor && arguments.empty()) return;

  auto value =
      ast->constructor
          ? constructSubobject(ast, type, arguments,
                               receiver_.object->mutableSubobject(subobject))
          : initialValue(type, arguments.front());
  if (!value) {
    aborted_ = true;
    return;
  }
  receiver_.object->setMember(subobject, std::move(*value));
}

auto ASTInterpreter::defaultConstruct(const Type* type, ConstValue* storage)
    -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  auto unqualified = traits.remove_cv(type);
  if (auto arrayType = type_cast<BoundedArrayType>(unqualified)) {
    auto retained = storage
                        ? std::get_if<std::shared_ptr<InitializerList>>(storage)
                        : nullptr;
    auto elements =
        retained && *retained ? *retained : std::make_shared<InitializerList>();
    const bool reuseStorage = retained && *retained;
    if (!reuseStorage) elements->elements.reserve(arrayType->size());
    for (std::size_t index = 0; index < arrayType->size(); ++index) {
      auto slot =
          reuseStorage ? &std::get<0>(elements->elements[index]) : nullptr;
      auto value = defaultConstruct(arrayType->elementType(), slot);
      if (!value) return std::nullopt;
      if (slot)
        *slot = std::move(*value);
      else
        elements->elements.emplace_back(std::move(*value),
                                        arrayType->elementType());
    }
    return ConstValue{std::move(elements)};
  }

  if (!traits.is_class(unqualified))
    return storage ? *storage : ConstValue{IndeterminateValue{}};

  auto classType = type_cast<ClassType>(unqualified);
  if (!classType || !classType->symbol()) return std::nullopt;

  auto classSymbol = classType->symbol()->resolvedDefinition();
  auto constructor = classSymbol->defaultConstructor();
  if (!constructor) return std::nullopt;
  if (!constructor->isConstexpr()) return std::nullopt;
  auto object =
      storage ? std::get_if<std::shared_ptr<ConstObject>>(storage) : nullptr;
  return evaluateConstructor(constructor, type, {}, object ? *object : nullptr);
}

void ASTInterpreter::pushFrame() { frames_.push_back({}); }

void ASTInterpreter::popFrame() {
  if (!frames_.empty()) frames_.pop_back();
}

void ASTInterpreter::retireFrame() {
  if (frames_.empty()) return;
  retiredFrames_.push_back(std::move(frames_.back()));
  frames_.pop_back();
}

ASTInterpreter::AutomaticScope::AutomaticScope(ASTInterpreter& interp)
    : interp_(interp), parent_(interp.currentAutomaticScope_) {
  if (interp_.frames_.empty()) return;
  frameIndex_ = interp_.frames_.size() - 1;
  automaticObjectMark_ = interp_.frames_[frameIndex_].automaticObjects.size();
  interp_.currentAutomaticScope_ = this;
  active_ = true;
}

ASTInterpreter::AutomaticScope::~AutomaticScope() { end(); }

void ASTInterpreter::AutomaticScope::end() {
  if (!active_) return;

  auto& interp = interp_;
  while (interp.frames_[frameIndex_].automaticObjects.size() >
         automaticObjectMark_) {
    auto variable = interp.frames_[frameIndex_].automaticObjects.back();
    interp.frames_[frameIndex_].automaticObjects.pop_back();
    auto found = interp.frames_[frameIndex_].locals.find(variable);
    if (found == interp.frames_[frameIndex_].locals.end() ||
        interp.destroyValue(variable->type(), found->second.value))
      continue;
    interp.aborted_ = true;
    interp.frames_[frameIndex_].automaticObjects.resize(automaticObjectMark_);
    break;
  }

  interp.retireAutomaticStorage(*this);

  interp.currentAutomaticScope_ = parent_;
  active_ = false;
}

void ASTInterpreter::registerAutomaticObject(Symbol* symbol) {
  if (!symbol) return;
  if (auto variable = symbol_cast<VariableSymbol>(symbol);
      variable && variable->isStatic())
    return;
  if (frames_.empty()) return;
  auto type = traits.remove_cv(symbol->type());
  if (!traits.is_class(type) && !traits.is_array(type)) return;
  frames_.back().automaticObjects.push_back(symbol);
}

auto ASTInterpreter::destroyValue(const Type* type, ConstValue& value) -> bool {
  auto unqual = traits.remove_cv(type);
  if (auto arrayType = type_cast<BoundedArrayType>(unqual)) {
    auto elements = std::get_if<std::shared_ptr<InitializerList>>(&value);
    if (!elements || !*elements) return false;
    for (auto it = (*elements)->elements.rbegin();
         it != (*elements)->elements.rend(); ++it) {
      auto& [element, elementType] = *it;
      auto typeToDestroy = elementType;
      if (!typeToDestroy) typeToDestroy = arrayType->elementType();
      if (!destroyValue(typeToDestroy, element)) return false;
    }
    return true;
  }

  auto classType = type_cast<ClassType>(unqual);
  if (!classType) return true;
  if (traits.has_trivial_destructor(unqual)) return true;

  auto object = std::get_if<std::shared_ptr<ConstObject>>(&value);
  if (!object || !*object) return false;
  auto classSymbol = classType->definition();
  traits.requireCompleteClass(classSymbol);
  if (!classSymbol || !classSymbol->isComplete()) return false;
  auto destructor = classSymbol->destructor();
  if (!destructor || destructor->isDeleted()) return false;

  if (!destructor->isDefaulted()) {
    if (!destructor->isConstexpr()) return false;
    auto savedReturnValue = std::move(returnValue_);
    auto savedCaptureReturnLValue = std::exchange(captureReturnLValue_, false);
    auto savedReturnLValue = std::exchange(returnLValue_, nullptr);
    auto savedCaptureReturnAddress =
        std::exchange(captureReturnAddress_, false);
    auto savedReturnAddress = std::move(returnAddress_);
    (void)evaluateCall(destructor, {}, *object);
    returnValue_ = std::move(savedReturnValue);
    captureReturnLValue_ = savedCaptureReturnLValue;
    returnLValue_ = savedReturnLValue;
    captureReturnAddress_ = savedCaptureReturnAddress;
    returnAddress_ = std::move(savedReturnAddress);
    if (aborted_) return false;
  }

  auto& members = (*object)->mutableMembers();
  for (auto it = members.rbegin(); it != members.rend(); ++it) {
    auto memberType =
        traits.aggregate_element_type(const_cast<Symbol*>(it->symbol));
    if (!memberType) continue;
    if (!destroyValue(memberType, it->value)) return false;
  }

  return true;
}

auto ASTInterpreter::executeFunction(FunctionSymbol* function, Frame frame,
                                     CallResultKind kind, Receiver receiver,
                                     bool constructor) -> CallResult {
  if (!function || !function->isConstexpr() || depth_ >= kMaxDepth) return {};
  auto definition = definingDeclarationOf(function);
  auto declaration = definition->declaration();
  auto body = declaration ? ast_cast<CompoundStatementFunctionBodyAST>(
                                declaration->functionBody)
                          : nullptr;
  if (!body) return {};

  auto functionType = type_cast<FunctionType>(function->type());
  const bool readReferenceResult =
      !constructor && kind == CallResultKind::kValue && functionType &&
      traits.is_reference(functionType->returnType());
  auto savedValue = std::exchange(returnValue_, std::nullopt);
  auto savedLValue = std::exchange(returnLValue_, nullptr);
  auto savedAddress = std::exchange(returnAddress_, std::nullopt);
  auto savedCaptureLValue =
      std::exchange(captureReturnLValue_, kind == CallResultKind::kLValue);
  auto savedCaptureAddress =
      std::exchange(captureReturnAddress_,
                    kind == CallResultKind::kAddress || readReferenceResult);
  auto savedFunction = std::exchange(currentFunction_, function);
  if (receiver.address && !receiver.object) {
    auto value = loadAddress(*receiver.address, 0);
    if (value) {
      if (auto object = std::get_if<std::shared_ptr<ConstObject>>(&*value)) {
        receiver.object = *object;
      }
    }
  }
  auto savedThis = std::exchange(receiver_, std::move(receiver));
  auto savedConstructor = std::exchange(
      currentConstructorClass_,
      constructor ? symbol_cast<ClassSymbol>(function->parent()) : nullptr);
  auto savedContext =
      std::exchange(defaultInitializerContext_, DefaultInitializerContext{});
  ++depth_;
  frames_.push_back(std::move(frame));
  AutomaticScope functionScope{*this};
  for (auto& [symbol, local] : frames_.back().locals)
    local.declaringScope = &functionScope;
  for (auto parameter : function->parameters()) {
    auto local = frames_.back().locals.find(parameter);
    if (local == frames_.back().locals.end()) continue;
    registerAutomaticObject(parameter);
    if (traits.is_reference(parameter->type()) &&
        !frames_.back().referenceAddresses.contains(parameter))
      frames_.back().referenceAddresses.insert_or_assign(
          parameter, localAddress(frames_.back(), parameter));
  }
  if (constructor) {
    for (auto initializer : ListView{body->memInitializerList}) {
      (void)memInitializer(initializer);
      if (aborted_) break;
    }
    if (!aborted_ && currentConstructorClass_ &&
        !currentConstructorClass_->isUnion()) {
      for (auto field :
           views::members(currentConstructorClass_->resolvedDefinition()) |
               views::non_static_fields) {
        if (field->isBitField() && !field->name()) continue;
        if (receiver_.object->subobject(field)) continue;
        auto value = defaultConstruct(field->type());
        if (!value) {
          aborted_ = true;
          break;
        }
        receiver_.object->setMember(field, std::move(*value));
      }
    }
  }
  if (!aborted_ && body->statement) (void)statement(body->statement);
  if (auto type = type_cast<FunctionType>(function->type());
      type && traits.is_void(type->returnType()) && !returnValue_)
    returnValue_ = ConstValue{ConstInt{std::intmax_t{0}}};
  functionScope.end();
  CallResult result;
  if (constructor)
    result.value = receiver_.object;
  else if (kind == CallResultKind::kAddress || readReferenceResult)
    result.value = returnAddress_;
  else if (kind == CallResultKind::kLValue)
    result.lvalue = returnLValue_;
  else
    result.value = returnValue_;
  if (result.lvalue)
    retireFrame();
  else
    popFrame();
  --depth_;
  returnValue_ = std::move(savedValue);
  returnLValue_ = savedLValue;
  returnAddress_ = std::move(savedAddress);
  captureReturnLValue_ = savedCaptureLValue;
  captureReturnAddress_ = savedCaptureAddress;
  currentFunction_ = savedFunction;
  receiver_ = std::move(savedThis);
  currentConstructorClass_ = savedConstructor;
  defaultInitializerContext_ = savedContext;
  if (aborted_) return {};
  if (readReferenceResult && result.value) {
    auto address = std::get_if<std::shared_ptr<ConstAddress>>(&*result.value);
    result.value = address && *address
                       ? loadAddress(**address, 0, functionType->returnType())
                       : std::nullopt;
  }
  return result;
}

auto ASTInterpreter::evaluateCallWithReceiver(FunctionSymbol* function,
                                              std::vector<ConstValue> arguments,
                                              Receiver receiver)
    -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  Frame frame;
  if (!function || !function->isConstexpr() ||
      !bindParameters(frame, function, arguments))
    return std::nullopt;
  return executeFunction(function, std::move(frame), CallResultKind::kValue,
                         std::move(receiver))
      .value;
}

auto ASTInterpreter::evaluateCall(FunctionSymbol* function,
                                  std::vector<ConstValue> arguments,
                                  std::shared_ptr<ConstObject> thisObject)
    -> std::optional<ConstValue> {
  Receiver receiver;
  if (thisObject)
    receiver.object = std::move(thisObject);
  else if (function && function->isImplicitObjectMemberFunction())
    receiver = receiver_;
  return evaluateCallWithReceiver(function, std::move(arguments),
                                  std::move(receiver));
}

auto ASTInterpreter::evaluateCallLValue(FunctionSymbol* func,
                                        std::vector<ConstValue> args)
    -> ConstValue* {
  auto value = evaluateCallAddress(func, std::move(args));
  auto address =
      value ? std::get_if<std::shared_ptr<ConstAddress>>(&*value) : nullptr;
  auto type = type_cast<FunctionType>(func->type());
  return address && *address ? addressSlot(**address, 0, type->returnType())
                             : nullptr;
}

auto ASTInterpreter::evaluateCallAddress(FunctionSymbol* func,
                                         std::vector<ConstValue> args)
    -> std::optional<ConstValue> {
  Frame frame;
  if (!func || !func->isConstexpr() || !bindParameters(frame, func, args))
    return std::nullopt;
  return executeFunction(func, std::move(frame), CallResultKind::kAddress,
                         receiver_)
      .value;
}

auto ASTInterpreter::evaluateConstructorFromExprs(
    FunctionSymbol* constructor, const Type* type,
    const std::vector<ExpressionAST*>& arguments,
    std::shared_ptr<ConstObject> object) -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  if (!constructor || !constructor->isConstexpr()) return std::nullopt;
  if (constructor->isDefaulted()) {
    std::vector<ConstValue> values;
    for (auto argument : arguments) {
      auto value = designatedValue(argument);
      if (!value) return std::nullopt;
      values.push_back(std::move(*value));
    }
    return evaluateConstructor(constructor, type, std::move(values),
                               std::move(object));
  }
  Frame frame;
  if (!bindParametersFromExprs(frame, constructor, arguments))
    return std::nullopt;
  if (!object) object = std::make_shared<ConstObject>(type);
  return executeFunction(constructor, std::move(frame), CallResultKind::kValue,
                         Receiver{std::move(object), {}}, true)
      .value;
}

auto ASTInterpreter::evaluateConstructor(FunctionSymbol* ctor,
                                         const Type* classType,
                                         std::vector<ConstValue> args,
                                         std::shared_ptr<ConstObject> object)
    -> std::optional<ConstValue> {
  EvaluationScope evaluationScope{*this};
  if (!ctor) return std::nullopt;
  if (!ctor->isConstexpr()) return std::nullopt;

  auto defn = definingDeclarationOf(ctor);

  auto funcDef = defn->declaration();
  if (!funcDef) return std::nullopt;

  auto body = funcDef->functionBody;
  if (!body) return std::nullopt;

  if (depth_ >= kMaxDepth) return std::nullopt;

  auto defaultedClass = symbol_cast<ClassSymbol>(ctor->parent());
  if (defaultedClass) defaultedClass = defaultedClass->resolvedDefinition();

  if (ctor->isDefaulted() && defaultedClass &&
      ast_cast<DefaultFunctionBodyAST>(body)) {
    auto defaultConstructor = defaultedClass->defaultConstructor();
    if (defaultConstructor &&
        ctor->canonical() == defaultConstructor->canonical()) {
      auto obj = object ? object : std::make_shared<ConstObject>(classType);
      ++depth_;
      auto initialized = initializeDefaultedObject(obj, defaultedClass);
      --depth_;
      if (!initialized) return std::nullopt;
      return ConstValue{std::move(obj)};
    }
  }

  if (ast_cast<DefaultFunctionBodyAST>(body)) {
    auto classSymbol = defaultedClass;
    if (!classSymbol) return std::nullopt;

    auto copy = classSymbol->copyConstructor();
    auto move = classSymbol->moveConstructor();
    auto canonical = ctor->canonical();
    auto copiesObject = copy && canonical == copy->canonical();
    if (move && canonical == move->canonical()) copiesObject = true;
    if (!copiesObject || args.size() != 1) return std::nullopt;
    auto source = std::get_if<std::shared_ptr<ConstObject>>(&args.front());
    if (!source || !*source) return std::nullopt;
    auto obj = object ? object : std::make_shared<ConstObject>(classType);
    if (!copyDefaultedObject(obj, *source, classSymbol)) return std::nullopt;
    return ConstValue{std::move(obj)};
  }

  Frame frame;
  if (!bindParameters(frame, ctor, args)) return std::nullopt;
  auto obj = object ? object : std::make_shared<ConstObject>(classType);
  return executeFunction(ctor, std::move(frame), CallResultKind::kValue,
                         Receiver{std::move(obj), {}}, true)
      .value;
}
}  // namespace cxx
