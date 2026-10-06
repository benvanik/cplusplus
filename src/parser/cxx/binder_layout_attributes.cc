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
#include <cxx/attributes.h>
#include <cxx/binder.h>
#include <cxx/decl.h>
#include <cxx/symbols.h>

namespace cxx {

void Binder::validateLayoutAttributes(
    Symbol* symbol, List<AttributeSpecifierAST*>* attributes) {
  static constexpr AttributeSpelling kLayoutAttributeSpellings[] = {
      {AttributeSyntax::kGnu, "", "packed"},
      {AttributeSyntax::kCxx, "gnu", "packed"},
      {AttributeSyntax::kGnu, "", "aligned"},
      {AttributeSyntax::kCxx, "gnu", "aligned"},
  };

  visitAttributesBySpelling(
      unit_, attributes, kLayoutAttributeSpellings,
      [&](AttributeRef attribute) {
        if (attribute.spelling->name == "packed") {
          if (attribute.argumentClause) {
            error(attribute.location, "'packed' attribute takes no arguments");
            return true;
          }

          const auto field = symbol_cast<FieldSymbol>(symbol);
          const bool validEntity = symbol_cast<ClassSymbol>(symbol) ||
                                   symbol_cast<EnumSymbol>(symbol) ||
                                   symbol_cast<ScopedEnumSymbol>(symbol) ||
                                   (field && !field->isStatic());
          if (!validEntity) {
            error(attribute.location,
                  "'packed' attribute requires a record, enumeration, or "
                  "non-static data member");
          }
          return true;
        }

        const auto field = symbol_cast<FieldSymbol>(symbol);
        const bool validEntity = symbol_cast<ClassSymbol>(symbol) ||
                                 symbol_cast<VariableSymbol>(symbol) ||
                                 (field && !field->isStatic());
        if (!validEntity) {
          error(attribute.location,
                "'aligned' attribute requires a record, non-static data "
                "member, or variable");
        }
        return true;
      });
}

void Binder::validateDeclaratorLayoutAttributes(Symbol* symbol,
                                                const Decl& decl) {
  validateLayoutAttributes(symbol, decl.specs.attributeList);
  if (decl.declaratorId) {
    validateLayoutAttributes(symbol, decl.declaratorId->attributeList);
  }
}

}  // namespace cxx
