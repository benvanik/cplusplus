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

#include <cxx/ast_fwd.h>
#include <cxx/const_int.h>
#include <cxx/cxx_fwd.h>
#include <cxx/floating_point.h>
#include <cxx/literals_fwd.h>
#include <cxx/source_location.h>
#include <cxx/symbols_fwd.h>
#include <cxx/types_fwd.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace cxx {
class Meta;
class InitializerList;
class ConstComplex;
class ConstObject;
class ConstMaterializedStorage;
class ConstLifetimeStorage;
class ConstAddress;
class ConstLabelAddress;
class ASTInterpreter;

struct DefaultInitializerContext {
  SourceLocation location;
  ScopeSymbol* scope = nullptr;
};

struct IndeterminateValue {
  auto operator==(const IndeterminateValue&) const -> bool = default;
};

using ConstValue =
    std::variant<ConstInt, const StringLiteral*, ConstFloat, long double,
                 std::shared_ptr<Meta>, std::shared_ptr<InitializerList>,
                 std::shared_ptr<ConstObject>, std::shared_ptr<ConstAddress>,
                 std::shared_ptr<ConstLabelAddress>,
                 std::shared_ptr<ConstComplex>, IndeterminateValue>;

class InitializerList {
 public:
  InitializerList() = default;

  explicit InitializerList(
      std::vector<std::tuple<ConstValue, const Type*>> initialElements)
      : elements(std::move(initialElements)) {}

  std::vector<std::tuple<ConstValue, const Type*>> elements;

  [[nodiscard]] auto storage() const
      -> std::shared_ptr<ConstMaterializedStorage>;

  void setStorage(std::weak_ptr<ConstMaterializedStorage> storage) {
    storage_ = std::move(storage);
  }

 private:
  // Lazily created identity for an addressed element or descendant.
  std::weak_ptr<ConstMaterializedStorage> storage_;
};

class ConstComplex {
 public:
  ConstComplex() = default;

  ConstComplex(ConstValue real, ConstValue imag)
      : real_(std::move(real)), imag_(std::move(imag)) {}

  [[nodiscard]] auto real() const -> const ConstValue& { return real_; }
  [[nodiscard]] auto imag() const -> const ConstValue& { return imag_; }

  void setReal(ConstValue value) { real_ = std::move(value); }
  void setImag(ConstValue value) { imag_ = std::move(value); }

 private:
  ConstValue real_;
  ConstValue imag_;
};

class ConstObject {
 public:
  struct Member {
    const Symbol* symbol = nullptr;
    ConstValue value;
  };

  ConstObject() = default;

  explicit ConstObject(const Type* type) : type_(type) {}

  ConstObject(const Type* type, std::deque<Member> members)
      : type_(type), members_(std::move(members)) {}

  [[nodiscard]] static auto makeConstexprUnknown(const Type* type)
      -> std::shared_ptr<ConstObject>;

  [[nodiscard]] auto type() const -> const Type* { return type_; }

  [[nodiscard]] auto isConstexprUnknown() const -> bool {
    return constexprUnknown_;
  }

  void setConstexprUnknown(bool constexprUnknown) {
    constexprUnknown_ = constexprUnknown;
  }

  void setType(const Type* type) { type_ = type; }

  [[nodiscard]] auto members() const -> const std::deque<Member>& {
    return members_;
  }

  [[nodiscard]] auto mutableMembers() -> std::deque<Member>& {
    return members_;
  }

  [[nodiscard]] auto isUnion() const -> bool;

  auto addMember(const Symbol* symbol, ConstValue value) -> ConstValue*;

  void setMember(const Symbol* symbol, ConstValue value);

  [[nodiscard]] auto subobject(const Symbol* symbol) const -> const ConstValue*;

  [[nodiscard]] auto mutableSubobject(const Symbol* symbol) -> ConstValue*;

  [[nodiscard]] auto operator==(const ConstObject& other) const -> bool;

  [[nodiscard]] auto storage() const
      -> std::shared_ptr<ConstMaterializedStorage>;

  void setStorage(std::weak_ptr<ConstMaterializedStorage> storage) {
    storage_ = std::move(storage);
  }

 private:
  const Type* type_ = nullptr;
  std::deque<Member> members_;
  bool constexprUnknown_ = false;
  // Lazily created identity for addresses formed during materialization.
  std::weak_ptr<ConstMaterializedStorage> storage_;
};

class Meta {
 public:
  struct ConstExpr {
    ExpressionAST* expression = nullptr;
    ConstValue value;
  };

  std::variant<const Type*, const Symbol*, ConstExpr> value;
};

// Identity and lifetime of an addressed automatic value. The activation owns
// the slot; addresses retain this token without extending the slot's lifetime.
class ConstStorage {
 public:
  explicit ConstStorage(ConstValue* slot) : slot_(slot) {}
  [[nodiscard]] auto slot() const -> ConstValue* { return slot_; }

 private:
  friend class ASTInterpreter;

  // Borrowed frame slot, null after its source lifetime ends.
  ConstValue* slot_ = nullptr;
  // Intrusive ownership retained by the declaring automatic scope.
  std::shared_ptr<ConstStorage> automaticNext_;
};

// Value whose storage duration is owned independently of its pointer values.
// Owning and borrowing addresses share this identity so references can extend
// a temporary without letting a self-pointer retain its own value graph.
class ConstLifetimeStorage {
 public:
  ConstLifetimeStorage() = default;

  explicit ConstLifetimeStorage(ConstValue value) : value_(std::move(value)) {}

  [[nodiscard]] auto value() const -> const ConstValue& { return value_; }
  [[nodiscard]] auto mutableValue() -> ConstValue* { return &value_; }
  void setValue(ConstValue value) { value_ = std::move(value); }

 private:
  // Value whose lifetime is controlled by the owning address.
  ConstValue value_;
};

// Identity of an addressed materialized aggregate. Addresses retain this token
// without owning the value. Once the value reaches its C++ destination the
// token forwards to that storage address, preserving subobject and array
// provenance without rewriting pointers already stored in the value.
class ConstMaterializedStorage {
 public:
  explicit ConstMaterializedStorage(std::weak_ptr<ConstObject> object)
      : value_(std::move(object)) {}

  explicit ConstMaterializedStorage(std::weak_ptr<InitializerList> elements)
      : value_(std::move(elements)) {}

  [[nodiscard]] auto object() const -> std::shared_ptr<ConstObject> {
    auto object = std::get_if<std::weak_ptr<ConstObject>>(&value_);
    return object ? object->lock() : nullptr;
  }

  [[nodiscard]] auto elements() const -> std::shared_ptr<InitializerList> {
    auto elements = std::get_if<std::weak_ptr<InitializerList>>(&value_);
    return elements ? elements->lock() : nullptr;
  }

  [[nodiscard]] auto address() const -> std::shared_ptr<ConstAddress>;

  [[nodiscard]] auto isBound() const -> bool {
    return !std::holds_alternative<std::monostate>(address_);
  }

  [[nodiscard]] auto hasExpiredAddress() const -> bool {
    auto address = std::get_if<std::weak_ptr<ConstAddress>>(&address_);
    return address && address->expired();
  }

  void bind(std::shared_ptr<ConstAddress> address);
  void bindBorrowed(std::weak_ptr<ConstAddress> address);

 private:
  // Readable aggregate while this identity has not reached a destination.
  std::variant<std::weak_ptr<ConstObject>, std::weak_ptr<InitializerList>>
      value_;
  // Final storage. Lifetime-owning roots are borrowed so the materialized
  // value cannot retain its own owner.
  std::variant<std::monostate, std::shared_ptr<ConstAddress>,
               std::weak_ptr<ConstAddress>>
      address_;
};

class ConstAddress {
 public:
  ConstAddress() = default;

  // Symbol addresses use the canonical declaration as their identity.
  explicit ConstAddress(Symbol* symbol, std::intmax_t offset = 0);

  explicit ConstAddress(const StringLiteral* string, std::intmax_t offset = 0)
      : origin_(string), offset_(offset) {}

  ConstAddress(std::shared_ptr<ConstObject> owner, Symbol* symbol,
               std::intmax_t offset = 0);

  [[nodiscard]] static auto materialized(std::shared_ptr<ConstObject> object,
                                         Symbol* symbol = nullptr,
                                         std::intmax_t offset = 0)
      -> std::shared_ptr<ConstAddress>;

  [[nodiscard]] static auto materialized(
      std::shared_ptr<ConstMaterializedStorage> storage,
      std::intmax_t offset = 0) -> std::shared_ptr<ConstAddress>;

  [[nodiscard]] static auto owningLifetime(
      std::shared_ptr<ConstLifetimeStorage> storage, std::intmax_t offset = 0)
      -> std::shared_ptr<ConstAddress>;

  [[nodiscard]] static auto borrowedLifetime(
      const std::shared_ptr<ConstLifetimeStorage>& storage,
      std::intmax_t offset = 0) -> std::shared_ptr<ConstAddress>;

  // Subobject addresses retain the containing address that selected them.
  ConstAddress(std::shared_ptr<ConstAddress> parent, Symbol* symbol);

  explicit ConstAddress(const Type* typeInfoFor) : origin_(typeInfoFor) {}

  [[nodiscard]] auto symbol() const -> Symbol*;
  [[nodiscard]] auto typeInfoFor() const -> const Type*;
  [[nodiscard]] auto owner() const -> const std::shared_ptr<ConstObject>&;
  [[nodiscard]] auto storage() const -> const std::shared_ptr<ConstStorage>&;
  [[nodiscard]] auto materializedStorage() const
      -> std::shared_ptr<ConstMaterializedStorage>;
  [[nodiscard]] auto lifetimeStorage() const
      -> std::shared_ptr<ConstLifetimeStorage>;
  [[nodiscard]] auto ownedLifetimeStorage() const
      -> const std::shared_ptr<ConstLifetimeStorage>&;
  [[nodiscard]] auto borrowedLifetimeStorage() const
      -> std::shared_ptr<ConstLifetimeStorage>;
  [[nodiscard]] auto hasExpiredLifetimeStorage() const -> bool;
  [[nodiscard]] auto stringLiteral() const -> const StringLiteral*;
  [[nodiscard]] auto offset() const -> std::intmax_t;

  // Returns the containing storage address for a subobject address.
  [[nodiscard]] auto parent() const -> const std::shared_ptr<ConstAddress>&;

  // Returns the canonical declaration owning this address path.
  [[nodiscard]] auto rootSymbol() const -> Symbol*;

  [[nodiscard]] auto sameTarget(const ConstAddress& other) const -> bool;

  [[nodiscard]] auto denotesWholeOwner() const -> bool {
    return owner() && !symbol();
  }

  void setSymbol(Symbol* symbol) { symbol_ = symbol; }
  void setParent(std::shared_ptr<ConstAddress> parent);
  void setOwner(std::shared_ptr<ConstObject> owner);
  void setStorage(std::shared_ptr<ConstStorage> storage);
  void setOwnedLifetimeStorage(std::shared_ptr<ConstLifetimeStorage> storage);
  void setBorrowedLifetimeStorage(
      std::shared_ptr<ConstLifetimeStorage> storage);
  void setStringLiteral(const StringLiteral* string);
  void setTypeInfoFor(const Type* type);
  void setOffset(std::intmax_t offset);

 private:
  [[nodiscard]] auto forwardedAddress() const -> std::shared_ptr<ConstAddress>;

  [[nodiscard]] auto hasLifetimeStorage() const -> bool;
  [[nodiscard]] auto sameLifetimeStorage(const ConstAddress& other) const
      -> bool;

  explicit ConstAddress(std::shared_ptr<ConstMaterializedStorage> storage,
                        std::intmax_t offset)
      : origin_(std::move(storage)), offset_(offset) {}

  explicit ConstAddress(std::shared_ptr<ConstLifetimeStorage> storage,
                        std::intmax_t offset)
      : origin_(std::move(storage)), offset_(offset) {}

  explicit ConstAddress(std::weak_ptr<ConstLifetimeStorage> storage,
                        std::intmax_t offset)
      : origin_(std::move(storage)), offset_(offset) {}

  Symbol* symbol_ = nullptr;
  // Mutually exclusive root of this address. Pointer arithmetic clones the
  // leaf while subobject paths retain their immutable parent address.
  std::variant<std::monostate, std::shared_ptr<ConstAddress>,
               std::shared_ptr<ConstObject>, std::shared_ptr<ConstStorage>,
               std::shared_ptr<ConstMaterializedStorage>,
               std::shared_ptr<ConstLifetimeStorage>,
               std::weak_ptr<ConstLifetimeStorage>, const StringLiteral*,
               const Type*>
      origin_;
  std::intmax_t offset_ = 0;
};

class ConstLabelAddress {
 public:
  ConstLabelAddress() = default;

  explicit ConstLabelAddress(std::string name) : name_(std::move(name)) {}

  [[nodiscard]] auto name() const -> const std::string& { return name_; }

  void setName(std::string name) { name_ = std::move(name); }

 private:
  std::string name_;
};

[[nodiscard]] auto isFullyInitialized(const ConstValue& value) -> bool;

[[nodiscard]] auto equivalent_values(const ConstValue& lhs,
                                     const ConstValue& rhs) -> bool;

[[nodiscard]] auto asConstexprUnknownObject(const ConstValue& value)
    -> std::shared_ptr<ConstObject>;

}  // namespace cxx
