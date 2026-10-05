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

#pragma once

#include <cxx/ast.h>
#include <cxx/cxx_fwd.h>
#include <cxx/names_fwd.h>
#include <cxx/symbols.h>
#include <cxx/symbols_fwd.h>
#include <cxx/types.h>
#include <cxx/views/symbol_chain.h>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace cxx {

[[nodiscard]] auto bindsName(Symbol* symbol) -> bool;

[[nodiscard]] auto inlineNamespaceSet(NamespaceSymbol* namespaceSymbol)
    -> std::vector<NamespaceSymbol*>;

struct ClassMemberLookup {
  Symbol* symbol = nullptr;
  // Ordered storage subobjects from the lookup scope to the declaration.
  std::vector<Symbol*> subobjectPath;
  bool ambiguous = false;
};

[[nodiscard]] auto lookupClassMember(ClassSymbol* scope, const Name* name,
                                     const std::function<bool(Symbol*)>& accept)
    -> ClassMemberLookup;

[[nodiscard]] auto lookupBaseSubobjectPath(ClassSymbol* derived,
                                           ClassSymbol* base)
    -> std::optional<std::vector<Symbol*>>;

[[nodiscard]] auto resolveClassScope(Symbol* symbol) -> ClassSymbol*;

class VisitedScopes {
 public:
  [[nodiscard]] auto begin() const -> ScopeSymbol* const* { return data(); }
  [[nodiscard]] auto end() const -> ScopeSymbol* const* {
    return data() + size_;
  }

  void push_back(ScopeSymbol* scope) {
    if (overflow_.empty() && size_ < inline_.size()) {
      inline_[size_++] = scope;
      return;
    }
    if (overflow_.empty()) overflow_.assign(inline_.begin(), inline_.end());
    overflow_.push_back(scope);
    ++size_;
  }

 private:
  [[nodiscard]] auto data() const -> ScopeSymbol* const* {
    return overflow_.empty() ? inline_.data() : overflow_.data();
  }

  std::array<ScopeSymbol*, 32> inline_;
  std::vector<ScopeSymbol*> overflow_;
  std::size_t size_ = 0;
};

namespace detail {
template <typename Predicate>
[[nodiscard]] auto searchScope(ScopeSymbol* scope, const Name* name,
                               VisitedScopes& visited, Predicate accept,
                               bool followUsingDirectives = true,
                               bool searchBaseClasses = true,
                               std::vector<Symbol*>* subobjectPath = nullptr)
    -> Symbol* {
  if (std::ranges::contains(visited, scope)) return nullptr;
  visited.push_back(scope);

  if (auto cls = symbol_cast<ClassSymbol>(scope)) {
    if (auto def = cls->definition(); def && def != cls)
      return searchScope(def, name, visited, accept, followUsingDirectives,
                         searchBaseClasses, subobjectPath);
  }

  Symbol* classOrEnumDeclaration = nullptr;

  auto consider = [&](Symbol* candidate) -> Symbol* {
    if (!std::invoke(accept, candidate)) return nullptr;
    if (!is_class_or_enum_declaration(candidate)) return candidate;
    if (!classOrEnumDeclaration) classOrEnumDeclaration = candidate;
    return nullptr;
  };

  for (auto symbol : scope->find(name)) {
    if (symbol->isHidden()) continue;
    if (!bindsName(symbol)) continue;

    if (auto u = symbol_cast<UsingDeclarationSymbol>(symbol);
        u && u->target()) {
      auto target = resolve_using_declaration(symbol);
      if (auto found = consider(target)) {
        if (subobjectPath && is_non_static_member(target)) {
          auto classSymbol = symbol_cast<ClassSymbol>(scope);
          auto declarator = u->declarator();
          auto designatingClass =
              declarator && declarator->nestedNameSpecifier
                  ? resolveClassScope(declarator->nestedNameSpecifier->symbol)
                  : nullptr;
          if (classSymbol && designatingClass) {
            auto prefix =
                lookupBaseSubobjectPath(classSymbol, designatingClass);
            auto selected = lookupClassMember(designatingClass, name, accept);
            if (prefix && selected.symbol &&
                resolve_using_declaration(selected.symbol) == target) {
              *subobjectPath = std::move(*prefix);
              subobjectPath->insert(subobjectPath->end(),
                                    selected.subobjectPath.begin(),
                                    selected.subobjectPath.end());
            }
          }
        }
        return found;
      }
    }

    if (auto found = consider(symbol)) return found;
  }

  if (classOrEnumDeclaration) return classOrEnumDeclaration;

  if (auto classSymbol = symbol_cast<ClassSymbol>(scope)) {
    for (auto member : classSymbol->find(/*unnamed=*/nullptr)) {
      auto field = symbol_cast<FieldSymbol>(member);
      auto nestedClass = field ? anonymous_member_class(field) : nullptr;
      if (!nestedClass) continue;
      std::vector<Symbol*> nestedPath;
      if (auto s = searchScope(nestedClass, name, visited, accept,
                               followUsingDirectives, true, &nestedPath)) {
        if (subobjectPath) {
          subobjectPath->push_back(field);
          subobjectPath->insert(subobjectPath->end(), nestedPath.begin(),
                                nestedPath.end());
        }
        return s;
      }
    }

    if (searchBaseClasses && !classSymbol->baseClasses().empty()) {
      auto result = lookupClassMember(classSymbol, name, accept);
      if (subobjectPath) *subobjectPath = std::move(result.subobjectPath);
      return result.symbol;
    }
  }

  if (!followUsingDirectives) return nullptr;

  for (auto u : scope->usingDirectives()) {
    if (auto s = searchScope(u, name, visited, accept)) return s;
  }

  return nullptr;
}

template <typename Predicate>
[[nodiscard]] auto resolveAndSearch(Symbol* scopeSymbol, const Name* name,
                                    Predicate accept) -> Symbol* {
  if (!scopeSymbol) return nullptr;

  if (auto alias = symbol_cast<TypeAliasSymbol>(scopeSymbol)) {
    auto aliasedType = unqualified_type(alias->type());
    if (auto ct = type_cast<ClassType>(aliasedType))
      return resolveAndSearch(ct->symbol(), name, accept);
    if (auto et = type_cast<EnumType>(aliasedType))
      return resolveAndSearch(et->symbol(), name, accept);
    if (auto st = type_cast<ScopedEnumType>(aliasedType))
      return resolveAndSearch(st->symbol(), name, accept);
  }

  switch (scopeSymbol->kind()) {
    case SymbolKind::kNamespace:
    case SymbolKind::kClass:
    case SymbolKind::kEnum:
    case SymbolKind::kScopedEnum: {
      VisitedScopes visited;
      return searchScope(scopeSymbol->asScopeSymbol(), name, visited, accept);
    }
    case SymbolKind::kNamespaceAlias: {
      return resolveAndSearch(resolve_namespace_alias(scopeSymbol), name,
                              accept);
    }
    case SymbolKind::kInjectedClassName: {
      auto injected = symbol_cast<InjectedClassNameSymbol>(scopeSymbol);
      if (auto cls = injected->classSymbol()) {
        VisitedScopes visited;
        return searchScope(cls, name, visited, accept);
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}
}  // namespace detail

template <typename Predicate>
  requires std::predicate<Predicate, Symbol*>
[[nodiscard]] auto qualifiedLookup(ScopeSymbol* scope, const Name* name,
                                   Predicate accept) -> Symbol* {
  if (!scope || !name) return nullptr;
  VisitedScopes visited;
  return detail::searchScope(scope, name, visited, accept);
}

[[nodiscard]] inline auto qualifiedLookup(ScopeSymbol* scope, const Name* name)
    -> Symbol* {
  return qualifiedLookup(scope, name, [](Symbol*) { return true; });
}

template <typename Predicate>
  requires std::predicate<Predicate, Symbol*>
[[nodiscard]] auto qualifiedLookup(Symbol* scopeOrAlias, const Name* name,
                                   Predicate accept) -> Symbol* {
  if (!name) return nullptr;
  return detail::resolveAndSearch(scopeOrAlias, name, accept);
}

[[nodiscard]] inline auto qualifiedLookup(Symbol* scopeOrAlias,
                                          const Name* name) -> Symbol* {
  return qualifiedLookup(scopeOrAlias, name, [](Symbol*) { return true; });
}

[[nodiscard]] auto qualifiedLookupType(Symbol* scopeOrAlias,
                                       const Identifier* id) -> Symbol*;

[[nodiscard]] auto qualifiedLookupNamespace(Symbol* scopeOrAlias,
                                            const Identifier* id)
    -> NamespaceSymbol*;

[[nodiscard]] auto lookupStandardLibraryType(TranslationUnit* unit,
                                             WellKnownName name) -> Symbol*;

[[nodiscard]] auto argumentDependentLookup(
    TranslationUnit* unit, const Name* name,
    std::span<const Type* const> argumentTypes) -> std::vector<FunctionSymbol*>;

[[nodiscard]] auto unqualifiedNonMemberLookup(Control* control,
                                              ScopeSymbol* scope,
                                              const Name* name) -> Symbol*;

[[nodiscard]] auto isDeferredDependentLookupContext(TranslationUnit* unit,
                                                    Symbol* lookupContext,
                                                    ScopeSymbol* scope) -> bool;

[[nodiscard]] auto isArgumentDependentCallee(Symbol* symbol) -> bool;

[[nodiscard]] auto isPureFriend(FunctionSymbol* func) -> bool;

void addOverloadCandidate(std::vector<FunctionSymbol*>& candidates,
                          FunctionSymbol* function);

void addLookupCandidates(std::vector<FunctionSymbol*>& candidates,
                         Symbol* found);

[[nodiscard]] auto designatedFunction(Symbol* symbol) -> FunctionSymbol*;

[[nodiscard]] auto qualifiedLookupIncludingInlineNamespaces(
    Control* control, Symbol* scopeOrAlias, const Name* name,
    bool* ambiguous = nullptr) -> Symbol*;

struct DeallocationSignature {
  bool isDestroying = false;
  bool hasSize = false;
  bool hasAlignment = false;
};

[[nodiscard]] auto deallocationSignatureOf(TranslationUnit* unit,
                                           FunctionSymbol* fn)
    -> std::optional<DeallocationSignature>;

[[nodiscard]] auto resolveUsualOperatorDelete(TranslationUnit* unit,
                                              ClassSymbol* classSymbol,
                                              const Type* objectType,
                                              bool isArrayDelete)
    -> FunctionSymbol*;

void declareImplicitAllocationFunctions(TranslationUnit* unit, TokenKind op);

[[nodiscard]] auto resolveBuiltinLibcallOperator(TranslationUnit* unit,
                                                 TokenKind op,
                                                 const FunctionType* type)
    -> FunctionSymbol*;

[[nodiscard]] auto resolveBuiltinLibcallSymbol(TranslationUnit* unit,
                                               const char* nameStr,
                                               const FunctionType* funcType)
    -> FunctionSymbol*;

[[nodiscard]] auto namesBuiltinFunction(Symbol* symbol,
                                        BuiltinFunctionKind kind) -> bool;

[[nodiscard]] auto resolveBuiltinFunctionSymbol(TranslationUnit* unit,
                                                const Identifier* name,
                                                BuiltinFunctionKind kind)
    -> Symbol*;
}  // namespace cxx
