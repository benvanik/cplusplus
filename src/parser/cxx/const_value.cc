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
#include <cxx/symbols.h>
#include <cxx/types.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cxx {

namespace {
[[nodiscard]] auto isTransparentSubobject(const Symbol* symbol) -> bool {
  if (symbol_cast<BaseClassSymbol>(const_cast<Symbol*>(symbol))) return true;
  auto field = symbol_cast<FieldSymbol>(const_cast<Symbol*>(symbol));
  return field && anonymous_member_class(field);
}
}  // namespace

auto asConstexprUnknownObject(const ConstValue& value)
    -> std::shared_ptr<ConstObject> {
  auto object = std::get_if<std::shared_ptr<ConstObject>>(&value);
  if (!object || !*object) return {};
  if (!(*object)->isConstexprUnknown()) return {};
  return *object;
}

auto ConstObject::makeConstexprUnknown(const Type* type)
    -> std::shared_ptr<ConstObject> {
  auto object = std::make_shared<ConstObject>(type);
  object->setConstexprUnknown(true);
  return object;
}

auto ConstObject::isUnion() const -> bool {
  auto classType = unqualified_cast<ClassType>(type_);
  auto classSymbol = classType ? classType->symbol() : nullptr;
  return classSymbol && classSymbol->isUnion();
}

auto ConstObject::addMember(const Symbol* symbol, ConstValue value)
    -> ConstValue* {
  if (isUnion()) members_.clear();
  members_.push_back({symbol, std::move(value)});
  return &members_.back().value;
}

void ConstObject::setMember(const Symbol* symbol, ConstValue value) {
  if (!isUnion()) {
    for (auto& member : members_) {
      if (member.symbol == symbol) {
        member.value = std::move(value);
        return;
      }
    }
  }
  addMember(symbol, std::move(value));
}

auto ConstObject::subobject(const Symbol* symbol) const -> const ConstValue* {
  for (const auto& member : members_) {
    if (member.symbol == symbol) return &member.value;
  }
  for (const auto& member : members_) {
    if (!isTransparentSubobject(member.symbol)) continue;
    auto nested = std::get_if<std::shared_ptr<ConstObject>>(&member.value);
    if (!nested || !*nested) continue;
    if (auto found = (*nested)->subobject(symbol)) return found;
  }
  return nullptr;
}

auto ConstObject::mutableSubobject(const Symbol* symbol) -> ConstValue* {
  for (auto& member : members_) {
    if (member.symbol == symbol) return &member.value;
  }
  for (auto& member : members_) {
    if (!isTransparentSubobject(member.symbol)) continue;
    auto nested = std::get_if<std::shared_ptr<ConstObject>>(&member.value);
    if (!nested || !*nested) continue;
    if (auto found = (*nested)->mutableSubobject(symbol)) return found;
  }
  return nullptr;
}

ConstAddress::ConstAddress(Symbol* symbol, std::intmax_t offset)
    : symbol_(symbol ? symbol->canonical() : nullptr), offset_(offset) {}

ConstAddress::ConstAddress(std::shared_ptr<ConstAddress> parent, Symbol* symbol)
    : symbol_(symbol->canonical()), parent_(std::move(parent)) {}

auto ConstAddress::rootSymbol() const -> Symbol* {
  auto address = this;
  while (address->parent_) address = address->parent_.get();
  return address->owner_ ? nullptr : address->symbol_;
}

auto ConstAddress::sameTarget(const ConstAddress& other) const -> bool {
  auto left = this;
  auto right = &other;
  for (;;) {
    if (left->symbol_ != right->symbol_ || left->owner_ != right->owner_ ||
        left->string_ != right->string_ ||
        left->typeInfoFor_ != right->typeInfoFor_)
      return false;
    if (left->parent_ == right->parent_) return true;
    if (!left->parent_ || !right->parent_) return false;
    left = left->parent_.get();
    right = right->parent_.get();
    if (left->offset_ != right->offset_) return false;
  }
}

auto ConstObject::operator==(const ConstObject& other) const -> bool {
  if (type_ != other.type_) return false;
  if (members_.size() != other.members_.size()) return false;
  for (std::size_t i = 0; i < members_.size(); ++i) {
    if (members_[i].symbol != other.members_[i].symbol) return false;
    if (!equivalent_values(members_[i].value, other.members_[i].value))
      return false;
  }
  return true;
}

namespace {

[[nodiscard]] auto equivalentLongDouble(long double lhs, long double rhs)
    -> bool {
  if (lhs == rhs) {
    return lhs != 0 || std::signbit(lhs) == std::signbit(rhs);
  }
  if (!std::isnan(lhs) || !std::isnan(rhs)) return false;

  const auto lhsBits =
      std::bit_cast<std::array<unsigned char, sizeof(long double)>>(lhs);
  const auto rhsBits =
      std::bit_cast<std::array<unsigned char, sizeof(long double)>>(rhs);

  // The x87 ABI stores an 80-bit value in a padded 16-byte object. Padding is
  // not part of floating identity and may be indeterminate after a value copy.
  if constexpr (std::numeric_limits<long double>::digits == 64 &&
                std::numeric_limits<long double>::max_exponent == 16384 &&
                sizeof(long double) == 16 &&
                std::endian::native == std::endian::little) {
    return std::ranges::equal(lhsBits.begin(), lhsBits.begin() + 10,
                              rhsBits.begin(), rhsBits.begin() + 10);
  }

  return lhsBits == rhsBits;
}

struct EquivalentValues {
  const ConstValue& rhs;

  template <typename T>
  [[nodiscard]] auto other() const -> const T* {
    return std::get_if<T>(&rhs);
  }

  [[nodiscard]] auto operator()(const ConstInt& lhs) const -> bool {
    auto value = other<ConstInt>();
    return value && lhs == *value;
  }

  [[nodiscard]] auto operator()(ConstFloat lhs) const -> bool {
    auto value = other<ConstFloat>();
    return value && lhs == *value;
  }

  [[nodiscard]] auto operator()(long double lhs) const -> bool {
    auto value = other<long double>();
    return value && equivalentLongDouble(lhs, *value);
  }

  [[nodiscard]] auto operator()(
      const std::shared_ptr<InitializerList>& lhs) const -> bool {
    auto value = other<std::shared_ptr<InitializerList>>();
    if (!value || !*value || !lhs) return false;
    return std::ranges::equal(
        lhs->elements, (*value)->elements,
        [](const auto& left, const auto& right) {
          return std::get<1>(left) == std::get<1>(right) &&
                 equivalent_values(std::get<0>(left), std::get<0>(right));
        });
  }

  [[nodiscard]] auto operator()(const std::shared_ptr<ConstObject>& lhs) const
      -> bool {
    auto value = other<std::shared_ptr<ConstObject>>();
    return value && *value && lhs && *lhs == **value;
  }

  [[nodiscard]] auto operator()(const std::shared_ptr<ConstAddress>& lhs) const
      -> bool {
    auto value = other<std::shared_ptr<ConstAddress>>();
    if (!value || !*value || !lhs) return false;
    return lhs->sameTarget(**value) && lhs->offset() == (*value)->offset();
  }

  [[nodiscard]] auto operator()(const std::shared_ptr<ConstComplex>& lhs) const
      -> bool {
    auto value = other<std::shared_ptr<ConstComplex>>();
    if (!value || !*value || !lhs) return false;
    return equivalent_values(lhs->real(), (*value)->real()) &&
           equivalent_values(lhs->imag(), (*value)->imag());
  }

  template <typename T>
  [[nodiscard]] auto operator()(const T& lhs) const -> bool {
    auto value = other<T>();
    return value && lhs == *value;
  }
};

}  // namespace

auto equivalent_values(const ConstValue& lhs, const ConstValue& rhs) -> bool {
  return std::visit(EquivalentValues{rhs}, lhs);
}

}  // namespace cxx
