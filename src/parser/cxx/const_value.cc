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
#include <cxx/literals.h>
#include <cxx/memory_layout.h>
#include <cxx/names.h>
#include <cxx/symbols.h>
#include <cxx/types.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
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

auto InitializerList::storage() const
    -> std::shared_ptr<ConstMaterializedStorage> {
  return storage_.lock();
}

auto ConstObject::storage() const -> std::shared_ptr<ConstMaterializedStorage> {
  return storage_.lock();
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

ConstAddress::ConstAddress(std::shared_ptr<ConstObject> owner, Symbol* symbol,
                           std::intmax_t offset)
    : symbol_(symbol ? symbol->canonical() : nullptr),
      origin_(std::move(owner)),
      offset_(offset) {}

auto ConstMaterializedStorage::address() const
    -> std::shared_ptr<ConstAddress> {
  if (auto owned = std::get_if<std::shared_ptr<ConstAddress>>(&address_))
    return *owned;
  if (auto borrowed = std::get_if<std::weak_ptr<ConstAddress>>(&address_))
    return borrowed->lock();
  return {};
}

void ConstMaterializedStorage::bind(std::shared_ptr<ConstAddress> address) {
  assert(std::holds_alternative<std::monostate>(address_));
  address_ = std::move(address);
}

void ConstMaterializedStorage::bindBorrowed(
    std::weak_ptr<ConstAddress> address) {
  assert(std::holds_alternative<std::monostate>(address_));
  address_ = std::move(address);
}

auto ConstAddress::materialized(std::shared_ptr<ConstObject> object,
                                Symbol* symbol, std::intmax_t offset)
    -> std::shared_ptr<ConstAddress> {
  if (!object) return {};
  auto storage = object->storage();
  if (!storage) {
    storage = std::make_shared<ConstMaterializedStorage>(object);
    object->setStorage(storage);
  }
  auto address = materialized(std::move(storage), offset);
  if (symbol) {
    return std::make_shared<ConstAddress>(std::move(address), symbol);
  }
  return address;
}

auto ConstAddress::materialized(
    std::shared_ptr<ConstMaterializedStorage> storage, std::intmax_t offset)
    -> std::shared_ptr<ConstAddress> {
  if (!storage) return {};
  return std::shared_ptr<ConstAddress>(
      new ConstAddress(std::move(storage), offset));
}

auto ConstAddress::owningLifetime(std::shared_ptr<ConstLifetimeStorage> storage,
                                  std::intmax_t offset)
    -> std::shared_ptr<ConstAddress> {
  if (!storage) return {};
  return std::shared_ptr<ConstAddress>(
      new ConstAddress(std::move(storage), offset));
}

auto ConstAddress::borrowedLifetime(
    const std::shared_ptr<ConstLifetimeStorage>& storage, std::intmax_t offset)
    -> std::shared_ptr<ConstAddress> {
  if (!storage) return {};
  return std::shared_ptr<ConstAddress>(
      new ConstAddress(std::weak_ptr<ConstLifetimeStorage>{storage}, offset));
}

auto ConstAddress::arrayElement(std::shared_ptr<ConstAddress> parent,
                                std::intmax_t offset)
    -> std::shared_ptr<ConstAddress> {
  if (!parent) return {};
  return std::shared_ptr<ConstAddress>(
      new ConstAddress(std::move(parent), offset));
}

ConstAddress::ConstAddress(std::shared_ptr<ConstAddress> parent, Symbol* symbol)
    : symbol_(symbol ? symbol->canonical() : nullptr),
      origin_(std::move(parent)) {
  assert(symbol_);
}

auto ConstAddress::forwardedAddress() const -> std::shared_ptr<ConstAddress> {
  auto storage =
      std::get_if<std::shared_ptr<ConstMaterializedStorage>>(&origin_);
  return storage && *storage ? (*storage)->address() : nullptr;
}

auto ConstAddress::symbol() const -> Symbol* {
  auto forwarded = forwardedAddress();
  return forwarded ? forwarded->symbol() : symbol_;
}

auto ConstAddress::parent() const -> const std::shared_ptr<ConstAddress>& {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->parent();
  static const std::shared_ptr<ConstAddress> empty;
  auto parent = std::get_if<std::shared_ptr<ConstAddress>>(&origin_);
  return parent ? *parent : empty;
}

auto ConstAddress::owner() const -> const std::shared_ptr<ConstObject>& {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->owner();
  static const std::shared_ptr<ConstObject> empty;
  auto owner = std::get_if<std::shared_ptr<ConstObject>>(&origin_);
  return owner ? *owner : empty;
}

auto ConstAddress::storage() const -> const std::shared_ptr<ConstStorage>& {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->storage();
  static const std::shared_ptr<ConstStorage> empty;
  auto storage = std::get_if<std::shared_ptr<ConstStorage>>(&origin_);
  return storage ? *storage : empty;
}

auto ConstAddress::materializedStorage() const
    -> std::shared_ptr<ConstMaterializedStorage> {
  auto storage =
      std::get_if<std::shared_ptr<ConstMaterializedStorage>>(&origin_);
  if (!storage || !*storage) return {};
  auto forwarded = (*storage)->address();
  return forwarded ? forwarded->materializedStorage() : *storage;
}

auto ConstAddress::lifetimeStorage() const
    -> std::shared_ptr<ConstLifetimeStorage> {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->lifetimeStorage();
  if (auto owned = ownedLifetimeStorage()) return owned;
  return borrowedLifetimeStorage();
}

auto ConstAddress::ownedLifetimeStorage() const
    -> const std::shared_ptr<ConstLifetimeStorage>& {
  static const std::shared_ptr<ConstLifetimeStorage> empty;
  auto storage = std::get_if<std::shared_ptr<ConstLifetimeStorage>>(&origin_);
  if (storage) return *storage;
  auto restricted = std::get_if<ConstRestrictedLifetimeStorage>(&origin_);
  return restricted ? restricted->storage : empty;
}

auto ConstAddress::borrowedLifetimeStorage() const
    -> std::shared_ptr<ConstLifetimeStorage> {
  if (auto borrowed =
          std::get_if<std::weak_ptr<ConstLifetimeStorage>>(&origin_))
    return borrowed->lock();
  auto forwarded = forwardedAddress();
  return forwarded ? forwarded->lifetimeStorage() : nullptr;
}

auto ConstAddress::hasLifetimeStorage() const -> bool {
  return std::holds_alternative<std::shared_ptr<ConstLifetimeStorage>>(
             origin_) ||
         std::holds_alternative<ConstRestrictedLifetimeStorage>(origin_) ||
         std::holds_alternative<std::weak_ptr<ConstLifetimeStorage>>(origin_);
}

auto ConstAddress::hasExpiredLifetimeStorage() const -> bool {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->hasExpiredLifetimeStorage();
  auto borrowed = std::get_if<std::weak_ptr<ConstLifetimeStorage>>(&origin_);
  return borrowed && borrowed->expired();
}

auto ConstAddress::sameLifetimeStorage(const ConstAddress& other) const
    -> bool {
  if (!hasLifetimeStorage() || !other.hasLifetimeStorage()) return false;

  auto weakStorage = [](const ConstAddress& address) {
    if (auto owned = address.ownedLifetimeStorage())
      return std::weak_ptr<ConstLifetimeStorage>{owned};
    return *std::get_if<std::weak_ptr<ConstLifetimeStorage>>(&address.origin_);
  };
  auto left = weakStorage(*this);
  auto right = weakStorage(other);
  return !left.owner_before(right) && !right.owner_before(left);
}

auto ConstAddress::stringLiteral() const -> const StringLiteral* {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->stringLiteral();
  auto string = std::get_if<const StringLiteral*>(&origin_);
  return string ? *string : nullptr;
}

auto ConstAddress::typeInfoFor() const -> const Type* {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->typeInfoFor();
  auto type = std::get_if<const Type*>(&origin_);
  return type ? *type : nullptr;
}

void ConstAddress::setParent(std::shared_ptr<ConstAddress> parent) {
  if (parent) origin_ = std::move(parent);
}

void ConstAddress::setOwner(std::shared_ptr<ConstObject> owner) {
  if (owner) origin_ = std::move(owner);
}

void ConstAddress::setStorage(std::shared_ptr<ConstStorage> storage) {
  if (storage) origin_ = std::move(storage);
}

void ConstAddress::setOwnedLifetimeStorage(
    std::shared_ptr<ConstLifetimeStorage> storage) {
  if (storage) origin_ = std::move(storage);
}

void ConstAddress::setBorrowedLifetimeStorage(
    std::shared_ptr<ConstLifetimeStorage> storage) {
  if (storage) origin_ = std::weak_ptr<ConstLifetimeStorage>{storage};
}

void ConstAddress::setPersistentReadsRestricted(
    bool persistentReadsRestricted) {
  if (!persistentReadsRestricted || this->persistentReadsRestricted()) return;
  auto storage = std::get_if<std::shared_ptr<ConstLifetimeStorage>>(&origin_);
  if (storage && *storage)
    origin_ = ConstRestrictedLifetimeStorage{std::move(*storage)};
}

void ConstAddress::setStringLiteral(const StringLiteral* string) {
  if (string) origin_ = string;
}

void ConstAddress::setTypeInfoFor(const Type* type) {
  if (type) origin_ = type;
}

auto ConstAddress::rootSymbol() const -> Symbol* {
  auto forwarded = forwardedAddress();
  if (forwarded) return forwarded->rootSymbol();
  return parent() ? parent()->rootSymbol()
         : std::holds_alternative<std::monostate>(origin_) ? symbol_
                                                           : nullptr;
}

auto ConstAddress::sameTarget(const ConstAddress& other) const -> bool {
  auto left = this;
  auto right = &other;
  bool compareOffset = false;
  for (;;) {
    auto leftForwarded = left->forwardedAddress();
    if (leftForwarded) {
      left = leftForwarded.get();
      continue;
    }
    auto rightForwarded = right->forwardedAddress();
    if (rightForwarded) {
      right = rightForwarded.get();
      continue;
    }
    if (compareOffset && left->offset_ != right->offset_) return false;
    if (left->hasLifetimeStorage() != right->hasLifetimeStorage()) return false;
    if (left->hasLifetimeStorage() && !left->sameLifetimeStorage(*right))
      return false;
    if (left->symbol_ != right->symbol_ || left->owner() != right->owner() ||
        left->storage() != right->storage() ||
        left->materializedStorage() != right->materializedStorage() ||
        left->stringLiteral() != right->stringLiteral() ||
        left->typeInfoFor() != right->typeInfoFor())
      return false;
    if (left->parent() == right->parent()) return true;
    if (!left->parent() || !right->parent()) return false;
    left = left->parent().get();
    right = right->parent().get();
    compareOffset = true;
  }
}

auto ConstAddress::persistentReadsRestricted() const -> bool {
  return std::holds_alternative<ConstRestrictedLifetimeStorage>(origin_);
}

void ConstAddress::finalizeConstantReference() {
  auto root = this;
  bool selectsSubobject = false;
  while (root->parent()) {
    selectsSubobject = true;
    root = root->parent().get();
  }
  if (!selectsSubobject) return;
  root->setPersistentReadsRestricted(true);
}

auto ConstAddress::offset() const -> std::intmax_t {
  auto forwarded = forwardedAddress();
  return offset_ + (forwarded ? forwarded->offset() : 0);
}

void ConstAddress::setOffset(std::intmax_t offset) {
  auto forwarded = forwardedAddress();
  offset_ = offset - (forwarded ? forwarded->offset() : 0);
}

auto constAddressPath(const ConstAddress& address,
                      const MemoryLayout& memoryLayout)
    -> std::optional<ConstAddressPath> {
  std::vector<const ConstAddress*> reversePath;
  for (auto current = &address; current; current = current->parent().get())
    reversePath.push_back(current);
  if (reversePath.empty()) return std::nullopt;

  auto root = reversePath.back();
  auto rootSymbol = address.rootSymbol();
  auto rootStringLiteral = root->stringLiteral();
  if (!rootSymbol && !rootStringLiteral) return std::nullopt;
  ConstAddressPath result{.rootAddress = root};
  const Type* selectedType = rootSymbol ? rootSymbol->type() : nullptr;

  auto addByteOffset = [&](std::uintmax_t byteOffset) {
    if (result.byteOffset >
        std::numeric_limits<std::uintmax_t>::max() - byteOffset)
      return false;
    result.byteOffset += byteOffset;
    return true;
  };

  auto addScaledOffset = [&](std::uintmax_t index, std::uintmax_t elementSize) {
    if (elementSize != 0 &&
        index > std::numeric_limits<std::uintmax_t>::max() / elementSize)
      return false;
    return addByteOffset(index * elementSize);
  };

  auto addElementOffset = [&](std::uintmax_t index, const Type* elementType) {
    auto size = memoryLayout.sizeOf(elementType);
    return size && addScaledOffset(index, *size);
  };

  for (auto step = reversePath.rbegin(); step != reversePath.rend(); ++step) {
    auto current = *step;
    if (current->offset() < 0) return std::nullopt;
    const auto index = static_cast<std::uintmax_t>(current->offset());

    if (current->isArrayElement()) {
      auto array = unqualified_cast<BoundedArrayType>(selectedType);
      auto unbounded = unqualified_cast<UnboundedArrayType>(selectedType);
      auto elementType = array       ? array->elementType()
                         : unbounded ? unbounded->elementType()
                                     : nullptr;
      if (elementType) {
        if (!addElementOffset(index, elementType)) return std::nullopt;
        selectedType = elementType;
      } else if (rootStringLiteral) {
        if (!addScaledOffset(index, rootStringLiteral->codeUnitSize()))
          return std::nullopt;
      } else {
        return std::nullopt;
      }
      result.designatesSubobject = true;
      continue;
    }

    auto symbol = current->symbol();
    if (symbol && symbol != rootSymbol) {
      if (auto field = symbol_cast<FieldSymbol>(symbol)) {
        auto offset = field->offsetInClass();
        if (!offset || !addByteOffset(*offset)) return std::nullopt;
      } else if (auto base = symbol_cast<BaseClassSymbol>(symbol)) {
        auto selectedClass = unqualified_cast<ClassType>(selectedType);
        auto selectedSymbol = selectedClass ? selectedClass->symbol() : nullptr;
        auto selectedDefinition =
            selectedSymbol ? selectedSymbol->resolvedDefinition() : nullptr;
        auto selectedLayout =
            selectedDefinition ? selectedDefinition->layout() : nullptr;
        auto baseSymbol = resolved_base_class(base);
        auto info =
            selectedLayout && baseSymbol
                ? selectedLayout->getBaseInfo(baseSymbol, base->isVirtual())
                : std::nullopt;
        if (!info || !addByteOffset(info->offset)) return std::nullopt;
      } else {
        return std::nullopt;
      }
      selectedType = symbol->type();
      result.designatesSubobject = true;
    }

    if (index == 0) continue;
    auto array = unqualified_cast<BoundedArrayType>(selectedType);
    auto elementType = array ? array->elementType() : selectedType;
    if (elementType) {
      if (!addElementOffset(index, elementType)) return std::nullopt;
      if (array) selectedType = elementType;
    } else if (rootStringLiteral) {
      if (!addScaledOffset(index, rootStringLiteral->codeUnitSize()))
        return std::nullopt;
    } else {
      return std::nullopt;
    }
    result.designatesSubobject = true;
  }

  return result;
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
