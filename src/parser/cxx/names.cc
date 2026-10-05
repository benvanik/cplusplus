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

#include <cxx/const_value.h>
#include <cxx/names.h>

// cxx
#include <cxx/ast.h>
#include <cxx/control.h>
#include <cxx/literals.h>
#include <cxx/preprocessor.h>
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <cxx/util.h>

namespace cxx {

namespace {

struct TemplateArgumentHash {
  auto operator()(const Type* arg) const -> std::size_t {
    return std::hash<const void*>{}(arg);
  }

  auto operator()(Symbol* arg) const -> std::size_t {
    return arg->name() ? arg->name()->hashValue() : 0;
  }

  auto operator()(const ConstValue& arg) const -> std::size_t {
    return hash_value(arg).value_or(0);
  }

  auto operator()(ExpressionAST* arg) const -> std::size_t {
    return std::hash<const void*>{}(arg);
  }
};

struct ConvertToName {
  Control* control_;

  explicit ConvertToName(Control* control) : control_(control) {}

  auto operator()(NameIdAST* ast) const -> const Name* {
    return ast->identifier;
  }

  auto operator()(DestructorIdAST* ast) const -> const Name* {
    return control_->getDestructorId(visit(*this, ast->id));
  }

  auto operator()(PackIndexIdAST* ast) const -> const Name* { return nullptr; }

  auto operator()(DecltypeIdAST* ast) const -> const Name* {
    // decltype-id has no corresponding Name subclass.
    // Return nullptr; callers guard against null names.
    return nullptr;
  }

  auto operator()(OperatorFunctionIdAST* ast) const -> const Name* {
    return control_->getOperatorId(ast->op);
  }

  auto operator()(LiteralOperatorIdAST* ast) const -> const Name* {
    if (ast->identifier)
      return control_->getLiteralOperatorId(ast->identifier->name());

    auto value = ast->literal->value();
    auto suffix = value.substr(value.find_last_of('"') + 1);
    return control_->getLiteralOperatorId(suffix);
  }

  auto operator()(ConversionFunctionIdAST* ast) const -> const Name* {
    return control_->getConversionFunctionId(ast->typeId->type);
  }

  auto operator()(SimpleTemplateIdAST* ast) const -> const Name* {
    std::vector<TemplateArgument> arguments;
    return control_->getTemplateId(ast->identifier, std::move(arguments));
  }

  auto operator()(LiteralOperatorTemplateIdAST* ast) const -> const Name* {
    std::vector<TemplateArgument> arguments;
    return control_->getTemplateId(operator()(ast->literalOperatorId),
                                   std::move(arguments));
  }

  auto operator()(OperatorFunctionTemplateIdAST* ast) const -> const Name* {
    std::vector<TemplateArgument> arguments;
    return control_->getTemplateId(operator()(ast->operatorFunctionId),
                                   std::move(arguments));
  }
};

}  // namespace

auto template_argument_keys_equal(std::span<const TemplateArgument> left,
                                  std::span<const TemplateArgument> right)
    -> bool {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (auto value = std::get_if<ConstValue>(&left[index])) {
      auto otherValue = std::get_if<ConstValue>(&right[index]);
      if (!otherValue || !equivalent_values(*value, *otherValue)) return false;
    } else if (left[index] != right[index]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] auto get_name(Control* control, UnqualifiedIdAST* id)
    -> const Name* {
  if (!id) return nullptr;
  return visit(ConvertToName{control}, id);
}

auto get_lookup_name(Control* control, UnqualifiedIdAST* id) -> const Name* {
  auto name = get_name(control, id);
  if (auto templateId = name_cast<TemplateId>(name)) return templateId->name();
  return name;
}

auto get_template_arguments(UnqualifiedIdAST* id)
    -> List<TemplateArgumentAST*>* {
  if (auto templateId = ast_cast<SimpleTemplateIdAST>(id))
    return templateId->templateArgumentList;
  if (auto templateId = ast_cast<OperatorFunctionTemplateIdAST>(id))
    return templateId->templateArgumentList;
  if (auto templateId = ast_cast<LiteralOperatorTemplateIdAST>(id))
    return templateId->templateArgumentList;
  return nullptr;
}

auto get_name_location(IdExpressionAST* ast) -> SourceLocation {
  if (ast->unqualifiedId) return ast->unqualifiedId->firstSourceLocation();
  return ast->firstSourceLocation();
}

auto get_name_location(MemberExpressionAST* ast) -> SourceLocation {
  if (ast->unqualifiedId) return ast->unqualifiedId->firstSourceLocation();
  return ast->firstSourceLocation();
}

auto get_name_location(ExpressionAST* ast) -> SourceLocation {
  if (auto id = ast_cast<IdExpressionAST>(ast)) return get_name_location(id);
  if (auto member = ast_cast<MemberExpressionAST>(ast))
    return get_name_location(member);
  return ast->firstSourceLocation();
}

auto get_name_location(DotDesignatorAST* ast) -> SourceLocation {
  if (ast->identifierLoc) return ast->identifierLoc;
  return ast->firstSourceLocation();
}

auto resolveBuiltinFunctionKind(IdExpressionAST* idExpr)
    -> BuiltinFunctionKind {
  if (!idExpr) return BuiltinFunctionKind::T_NONE;

  if (auto function = symbol_cast<FunctionSymbol>(idExpr->symbol)) {
    return function->builtinKind();
  }

  if (auto overloadSet = symbol_cast<OverloadSetSymbol>(idExpr->symbol)) {
    for (auto function : overloadSet->declaredFunctions()) {
      auto kind = function->builtinKind();
      if (kind != BuiltinFunctionKind::T_NONE) return kind;
    }
  }

  return BuiltinFunctionKind::T_NONE;
}

Name::~Name() = default;

auto Identifier::isBuiltinTypeTrait() const -> bool {
  return builtinTypeTrait() != BuiltinTypeTraitKind::T_NONE;
}

auto Identifier::builtinTypeTrait() const -> BuiltinTypeTraitKind {
  if (!info_) return BuiltinTypeTraitKind::T_NONE;

  if (info_->kind() != IdentifierInfoKind::kTypeTrait)
    return BuiltinTypeTraitKind::T_NONE;

  return static_cast<const TypeTraitIdentifierInfo*>(info_)->trait();
}

auto Identifier::builtinFunction() const -> BuiltinFunctionKind {
  if (!info_) return BuiltinFunctionKind::T_NONE;

  if (info_->kind() != IdentifierInfoKind::kBuiltinFunction)
    return BuiltinFunctionKind::T_NONE;

  return static_cast<const BuiltinFunctionIdentifierInfo*>(info_)
      ->builtinKind();
}

auto well_known_name(const Name* name) -> WellKnownName {
  auto id = name_cast<Identifier>(name);
  return id ? id->wellKnownName() : WellKnownName::T_NONE;
}

auto Identifier::wellKnownName() const -> WellKnownName {
  if (!info_) return WellKnownName::T_NONE;

  if (info_->kind() != IdentifierInfoKind::kWellKnownName)
    return WellKnownName::T_NONE;

  return static_cast<const WellKnownNameIdentifierInfo*>(info_)
      ->wellKnownName();
}

auto Identifier::builtinTemplate() const -> BuiltinTemplateKind {
  if (!info_) return BuiltinTemplateKind::T_NONE;

  if (info_->kind() != IdentifierInfoKind::kBuiltinTemplate)
    return BuiltinTemplateKind::T_NONE;

  return static_cast<const BuiltinTemplateIdentifierInfo*>(info_)
      ->builtinKind();
}

auto TemplateId::hash(const Name* name,
                      const std::vector<TemplateArgument>& args)
    -> std::size_t {
  std::size_t hash = name->hashValue();
  for (const auto& arg : args) {
    hash_combine(hash, std::visit(TemplateArgumentHash{}, arg));
  }
  return hash;
}

}  // namespace cxx
