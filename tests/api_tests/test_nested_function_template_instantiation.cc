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
#include <cxx/symbols.h>
#include <cxx/views/symbols.h>
#include <gtest/gtest.h>

#include <ranges>
#include <string_view>

#include "test_utils.h"

using namespace cxx;

TEST(FunctionTemplateInstantiation, CompletesNestedCallsIteratively) {
  auto source = R"(
template <int Count>
int acquire_all() {
  if constexpr (Count == 1) {
    return 1;
  } else {
    return 1 + acquire_all<Count - 1>();
  }
}

int entry() { return acquire_all<512>(); }
)"_cxx;

  auto functions = views::each_function(source.get("acquire_all"));
  ASSERT_EQ(std::ranges::distance(functions), 1);

  auto* pattern = *functions.begin();
  ASSERT_TRUE(pattern->isTemplatePattern());
  ASSERT_EQ(pattern->specializations().size(), 512u);
  for (const auto& specialization : pattern->specializations()) {
    auto* function = symbol_cast<FunctionSymbol>(specialization.symbol);
    ASSERT_NE(function, nullptr);
    EXPECT_NE(function->declaration(), nullptr);
    EXPECT_FALSE(function->hasPendingBody());
  }
}

TEST(FunctionTemplateInstantiation, ExplicitMembersRetainSelectedDefinitions) {
  auto source = R"(
struct Operations {
  template <class T> void apply() {}
  template <class T> static void static_apply(T) {}
  template <class T> void apply(T) const & {}
};
template void Operations::apply<int>();
template void Operations::static_apply(int);
template void Operations::apply(int) const &;

template <class T> struct Container {
  void store(T) {}
  void store(T*) {}
  template <class U> void convert(U) {}
};
template void Container<int>::store(int);
template void Container<int>::convert(float);
)"_cxx;

  ASSERT_EQ(source.diagnosticsClient.errorCount(), 0);
  EXPECT_EQ(source.get("apply"), nullptr);
  EXPECT_EQ(source.get("static_apply"), nullptr);
  EXPECT_EQ(source.get("store"), nullptr);
  EXPECT_EQ(source.get("convert"), nullptr);

  const auto& definitions = source.unit.explicitInstantiationDefinitions();
  ASSERT_EQ(definitions.size(), 5u);
  for (auto function : definitions) {
    EXPECT_NE(function->enclosingClass(), nullptr);
    EXPECT_TRUE(function->isDefinitionRequired());
    EXPECT_FALSE(function->hasPendingBody());
    EXPECT_NE(function->resolvedDefinition()->declaration(), nullptr);
  }
  EXPECT_TRUE(definitions[1]->isStatic());
  EXPECT_EQ(type_cast<FunctionType>(definitions[2]->type())->cvQualifiers(),
            CvQualifiers::kConst);
  auto selected = definitions[3];
  auto overloads = LookupMember{source}(selected->enclosingClass(), "store");
  ASSERT_NE(overloads, nullptr);
  for (auto function : views::each_function(overloads)) {
    if (function->canonical() == selected) continue;
    EXPECT_FALSE(function->isDefinitionRequired());
    EXPECT_TRUE(function->hasPendingBody());
  }
}

TEST(FunctionTemplateInstantiation, ExternMembersKeepBodiesDeferred) {
  auto source = R"(
struct Operations {
  template <class T> void apply() {
    static_assert(sizeof(T) == 0, "extern template body");
  }
};
extern template void Operations::apply<int>();

template <class T> struct Container {
  void external() { static_assert(sizeof(T) == 0, "extern member body"); }
  void untouched() { static_assert(sizeof(T) == 0, "unrelated body"); }
  void required() {}
};
extern template void Container<int>::external();
extern template void Container<int>::required();
template void Container<int>::required();
)"_cxx;

  ASSERT_EQ(source.diagnosticsClient.errorCount(), 0);
  const auto& definitions = source.unit.explicitInstantiationDefinitions();
  ASSERT_EQ(definitions.size(), 1u);
  auto required = definitions.front();
  EXPECT_TRUE(required->isDefinitionRequired());
  EXPECT_FALSE(required->hasPendingBody());
  EXPECT_TRUE(source.unit.isExplicitInstantiationDeclaration(required));

  auto classSymbol = required->enclosingClass();
  auto external =
      *views::each_function(LookupMember{source}(classSymbol, "external"))
           .begin();
  auto untouched =
      *views::each_function(LookupMember{source}(classSymbol, "untouched"))
           .begin();
  EXPECT_TRUE(source.unit.isExplicitInstantiationDeclaration(external));
  EXPECT_FALSE(source.unit.isExplicitInstantiationDeclaration(untouched));
  EXPECT_FALSE(external->isDefinitionRequired());
  EXPECT_FALSE(untouched->isDefinitionRequired());
  EXPECT_TRUE(external->hasPendingBody());
  EXPECT_TRUE(untouched->hasPendingBody());
}

TEST(FunctionTemplateInstantiation,
     ExplicitSpecializationsReplaceMemberBodies) {
  auto source = R"(
template <class T> struct Container {
  constexpr int apply() { static_assert(sizeof(T) == 0); return 1; }
  void defined() { static_assert(sizeof(T) == 0); }
  static unsigned convert(T) { static_assert(sizeof(T) == 0); return 0; }
};
template <> int Container<int>::apply();
template <> void Container<int>::defined() {}
template <> unsigned Container<int>::convert(int value) { return value + 7; }
template unsigned Container<int>::convert(int);
template int Container<int>::apply();
extern template void Container<int>::defined();
template struct Container<int>;
Container<int> object;
int entry() { object.defined(); return object.apply() + Container<int>::convert(5); }
)"_cxx;

  ASSERT_EQ(source.diagnosticsClient.errorCount(), 0);
  EXPECT_TRUE(source.unit.explicitInstantiationDefinitions().empty());
  auto pattern = symbol_cast<ClassSymbol>(source.get("Container"));
  ASSERT_NE(pattern, nullptr);
  ASSERT_EQ(pattern->specializations().size(), 1u);
  auto instance =
      symbol_cast<ClassSymbol>(pattern->specializations()[0].symbol);
  ASSERT_NE(instance, nullptr);
  for (std::string_view name : {"apply", "defined", "convert"}) {
    auto function =
        *views::each_function(LookupMember{source}(instance, name)).begin();
    EXPECT_FALSE(function->hasPendingBody());
    EXPECT_FALSE(function->isInline());
    EXPECT_FALSE(function->isConstexpr());
    EXPECT_TRUE(function->isExplicitlySpecialized());
    EXPECT_FALSE(is_inline_or_templated(function));
    EXPECT_FALSE(source.unit.isExplicitInstantiationDeclaration(function));
    EXPECT_FALSE(function->isTemplatePattern());
    EXPECT_TRUE(function->specializations().empty());
    if (name != "apply") {
      auto definition = function->resolvedDefinition();
      ASSERT_NE(definition->declaration(), nullptr);
      EXPECT_NE(ast_cast<CompoundStatementFunctionBodyAST>(
                    definition->declaration()->functionBody),
                nullptr);
    }
  }
}
