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

#include <cxx/ast_pretty_printer.h>
#include <cxx/names.h>
#include <cxx/printing.h>
#include <cxx/private/printing-priv.h>
#include <cxx/symbols.h>
#include <cxx/token.h>
#include <cxx/types.h>

#include <cstdint>
#include <format>

namespace cxx {

namespace {

struct TemplateArgumentPrinter {
  detail::PrintOptions options;

  [[nodiscard]] auto address_to_string(const ConstAddress& address,
                                       const Type* type) const -> std::string {
    const auto reference = type_cast<LvalueReferenceType>(type);
    const auto pointer = type_cast<PointerType>(type);
    if (!reference && !pointer) return "<const-address>";

    if (address.owner() || address.storage() || address.stringLiteral() ||
        address.typeInfoFor())
      return "<const-address>";
    auto root = address.root() ? address.root() : address.symbol();
    if (!root) return pointer ? "nullptr" : "<const-address>";

    auto designation = detail::renderQualifiedName(root, options);
    if (designation.starts_with("::")) designation.erase(0, 2);
    auto baseQualifier = false;
    auto baseStart = std::string::npos;
    for (const auto& step : address.subobjectPath()) {
      if (auto index = std::get_if<std::size_t>(&step)) {
        designation += std::format("[{}]", *index);
        baseQualifier = false;
        baseStart = std::string::npos;
        continue;
      }
      auto symbol = std::get<Symbol*>(step);
      auto name = detail::renderName(symbol->name(), options);
      if (name.empty()) continue;
      if (auto* base = symbol_cast<BaseClassSymbol>(symbol)) {
        if (!baseQualifier) baseStart = designation.size();
        if (base->isVirtual()) {
          designation.erase(baseStart);
          designation += "." + name;
        } else {
          designation += (baseQualifier ? "::" : ".") + name;
        }
        baseQualifier = true;
      } else {
        designation += (baseQualifier ? "::" : ".") + name;
        baseQualifier = false;
        baseStart = std::string::npos;
      }
    }

    auto pointerExpression = false;
    if (auto array = unqualified_cast<BoundedArrayType>(address.arrayType());
        array && address.offset() >= 0 &&
        static_cast<std::uintmax_t>(address.offset()) == array->size()) {
      designation = std::format("({} + {})", designation, address.offset());
      pointerExpression = true;
    } else if (address.arrayType()) {
      designation += std::format("[{}]", address.offset());
    } else if (address.offset() != 0) {
      designation = std::format("(&{} + {})", designation, address.offset());
      pointerExpression = true;
    }
    if (reference || pointerExpression) return designation;
    return "&" + designation;
  }

  [[nodiscard]] auto const_value_to_string(const ConstValue& value,
                                           const Type* type = nullptr) const
      -> std::string {
    if (auto address = std::get_if<std::shared_ptr<ConstAddress>>(&value)) {
      return *address ? address_to_string(**address, type) : "<const-address>";
    }
    if (auto v = std::get_if<ConstInt>(&value)) return v->toString();
    if (auto v = std::get_if<ConstFloat>(&value))
      return std::format("{}", v->toDouble());
    if (auto v = std::get_if<long double>(&value)) {
      return std::format("{}", *v);
    }
    if (std::holds_alternative<const StringLiteral*>(value)) return "\"...\"";
    if (std::holds_alternative<std::shared_ptr<Meta>>(value)) return "<meta>";
    if (std::holds_alternative<std::shared_ptr<InitializerList>>(value)) {
      return "<init-list>";
    }
    if (std::holds_alternative<std::shared_ptr<ConstObject>>(value)) {
      return "<const-object>";
    }
    return "<const>";
  }

  [[nodiscard]] auto bool_to_string(const Type* type,
                                    const ConstValue& value) const
      -> std::optional<std::string> {
    if (options.mode == detail::PrintMode::Internal) return std::nullopt;
    if (!unqualified_cast<BoolType>(type)) return std::nullopt;
    auto number = std::get_if<ConstInt>(&value);
    if (!number) return std::nullopt;
    return number->isZero() ? "false" : "true";
  }

  [[nodiscard]] auto pack_to_string(const ParameterPackSymbol* pack) const
      -> std::string {
    std::string s = "<";
    std::string_view sep = "";
    for (auto element : pack->elements()) {
      s += sep;
      s += detail::renderTemplateArgument(TemplateArgument{element}, options);
      sep = ", ";
    }
    s += '>';
    return s;
  }

  [[nodiscard]] auto operator()(const Type* type) const -> std::string {
    return detail::renderType(type, "", options);
  }

  [[nodiscard]] auto operator()(const ConstValue& value) const -> std::string {
    return const_value_to_string(value);
  }

  [[nodiscard]] auto operator()(Symbol* symbol) const -> std::string {
    if (!symbol) return "<null-symbol>";
    if (options.mode != detail::PrintMode::Internal) {
      if (auto pack = symbol_cast<ParameterPackSymbol>(symbol))
        return pack_to_string(pack);
    }
    if (symbol->isTypeAlias())
      return detail::renderType(symbol->type(), "", options);
    if (symbol->isVariable()) {
      auto var = static_cast<const VariableSymbol*>(symbol);
      if (auto cst = var->constValue()) {
        if (auto boolean = bool_to_string(var->type(), *cst)) return *boolean;
        return const_value_to_string(*cst, var->type());
      }
    }
    if (auto type = symbol->type())
      return detail::renderType(type, "", options);
    return detail::renderName(symbol->name(), options);
  }

  [[nodiscard]] auto operator()(ExpressionAST* value) const -> std::string {
    if (!value || !options.unit) return {};
    return std::format("{}", formatAST(options.unit, value));
  }
};

struct NamePrinter {
  detail::PrintOptions options;

  [[nodiscard]] auto operator()(const Identifier* name) const -> std::string {
    if (options.mode != detail::PrintMode::Internal && !name->value().empty() &&
        name->isAnonymous())
      return "(anonymous)";
    return name->value();
  }

  [[nodiscard]] auto operator()(const OperatorId* name) const -> std::string {
    switch (name->op()) {
      case TokenKind::T_LPAREN:
        return "operator ()";
      case TokenKind::T_LBRACKET:
        return "operator []";
      case TokenKind::T_NEW_ARRAY:
        return "operator new[]";
      case TokenKind::T_DELETE_ARRAY:
        return "operator delete[]";
      default:
        return std::format("operator {}", Token::spell(name->op()));
    }  // switch
  }

  [[nodiscard]] auto operator()(const DestructorId* name) const -> std::string {
    return "~" + detail::renderName(name->name(), options);
  }

  [[nodiscard]] auto operator()(const LiteralOperatorId* name) const
      -> std::string {
    return std::format("operator \"\"{}", name->name());
  }

  [[nodiscard]] auto operator()(const ConversionFunctionId* name) const
      -> std::string {
    return std::format("operator {}",
                       detail::renderType(name->type(), "", options));
  }

  [[nodiscard]] auto operator()(const TemplateId* name) const -> std::string {
    std::string s = detail::renderName(name->name(), options);
    s += options.mode == detail::PrintMode::Internal ? " <" : "<";
    std::string_view sep = "";
    for (const auto& arg : expand_template_arguments(name->arguments())) {
      s += sep;
      s += detail::renderTemplateArgument(arg, options);
      sep = ", ";
    }
    s += '>';
    return s;
  }
};

}  // namespace

auto detail::renderName(const Name* name, PrintOptions options) -> std::string {
  if (!name) return {};
  return visit(NamePrinter{options}, name);
}

auto detail::renderTemplateArgument(const TemplateArgument& argument,
                                    PrintOptions options) -> std::string {
  auto text = std::visit(TemplateArgumentPrinter{options}, argument);
  if (text.empty()) return "?";
  return text;
}

auto NameFormat::formatTo(std::format_context::iterator out) const
    -> std::format_context::iterator {
  return std::format_to(out, "{}", detail::renderName(name, {options, mode}));
}

auto TemplateArgumentFormat::formatTo(std::format_context::iterator out) const
    -> std::format_context::iterator {
  return std::format_to(
      out, "{}", detail::renderTemplateArgument(argument, {options, mode}));
}

auto to_string(const Name* name, TypePrintOptions options) -> std::string {
  return std::format("{}", formatName(name, options));
}

auto to_string(const TemplateArgument& argument, TypePrintOptions options)
    -> std::string {
  return std::format("{}", formatTemplateArgument(argument, options));
}

auto to_pretty_string(const Name* name, TypePrintOptions options)
    -> std::string {
  return std::format("{}", formatPrettyName(name, options));
}

auto to_pretty_string(const TemplateArgument& argument,
                      TypePrintOptions options) -> std::string {
  return std::format("{}", formatPrettyTemplateArgument(argument, options));
}

auto to_diag_string(const Name* name, TypePrintOptions options) -> std::string {
  if (auto id = name_cast<Identifier>(name); id && !id->isAnonymous())
    return id->value();
  return std::format("{}", formatDiagName(name, options));
}

auto to_diag_string(const TemplateArgument& argument, TypePrintOptions options)
    -> std::string {
  return std::format("{}", formatDiagTemplateArgument(argument, options));
}

}  // namespace cxx
