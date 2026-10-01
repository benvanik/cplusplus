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

#include <cxx/ast_interpreter.h>
#include <cxx/constant_bits.h>
#include <cxx/control.h>
#include <cxx/memory_layout.h>
#include <cxx/translation_unit.h>
#include <cxx/type_traits.h>
#include <cxx/types.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace cxx {
namespace {

struct ConstantShape {
  // Unqualified scalar type of each stored element.
  const Type* elementType = nullptr;

  // Number of bytes occupied by each element.
  std::size_t elementSize = 0;

  // Number of elements in the object.
  std::size_t elementCount = 0;

  // Vector type, or null for a scalar object.
  const VectorType* vectorType = nullptr;
};

[[nodiscard]] auto shapeOf(const Type* type, const TypeTraits& traits,
                           const MemoryLayout& layout)
    -> std::optional<ConstantShape> {
  type = traits.remove_cv(type);
  auto vectorType = type_cast<VectorType>(type);
  auto elementType =
      vectorType ? traits.remove_cv(vectorType->elementType()) : type;
  auto elementSize = layout.sizeOf(elementType);
  if (!elementSize || !*elementSize) return std::nullopt;

  if (auto representation = traits.integral_representation(elementType)) {
    if (elementType->kind() == TypeKind::kBool) {
      if (vectorType) return std::nullopt;
    } else if (representation->bits != *elementSize * 8 ||
               representation->bits > ConstInt::maxWidth) {
      return std::nullopt;
    }
  } else if (auto format = ConstFloat::formatFor(elementType->kind())) {
    auto zero = ConstFloat::fromBits(*format, 0);
    if (!zero || zero->bitWidth() != *elementSize * 8) return std::nullopt;
  } else {
    return std::nullopt;
  }

  const auto elementCount = vectorType ? vectorType->elementCount() : 1;
  auto objectSize = layout.sizeOf(type);
  if (!objectSize || !elementCount || *objectSize % *elementSize != 0 ||
      *objectSize / *elementSize != elementCount) {
    return std::nullopt;
  }

  return ConstantShape{elementType, *elementSize, elementCount, vectorType};
}

[[nodiscard]] auto scalarBits(ASTInterpreter& interpreter,
                              const ConstValue& value, const Type* type)
    -> std::optional<ConstInt::UWide> {
  if (auto format = ConstFloat::formatFor(type->kind())) {
    auto floating = std::get_if<ConstFloat>(&value);
    if (!floating || floating->format() != *format) return std::nullopt;
    return static_cast<ConstInt::UWide>(floating->bits());
  }

  auto converted = interpreter.toIntegralType(value, type);
  if (!converted) return std::nullopt;
  auto integer = std::get_if<ConstInt>(&*converted);
  if (!integer) return std::nullopt;
  return integer->toUWide();
}

[[nodiscard]] auto scalarValue(ASTInterpreter& interpreter,
                               ConstInt::UWide bits, const Type* type)
    -> std::optional<ConstValue> {
  if (auto format = ConstFloat::formatFor(type->kind())) {
    auto value =
        ConstFloat::fromBits(*format, static_cast<std::uint64_t>(bits));
    if (!value) return std::nullopt;
    return ConstValue{*value};
  }

  auto traits = interpreter.translationUnit()->typeTraits();
  auto representation = traits.integral_representation(type);
  if (!representation) return std::nullopt;
  if (type->kind() == TypeKind::kBool && bits > 1) return std::nullopt;

  auto value = ConstInt::make(std::bit_cast<ConstInt::Wide>(bits),
                              representation->bits, representation->isSigned);
  if (!value) return std::nullopt;
  return ConstValue{*value};
}

[[nodiscard]] auto byteShift(std::size_t byte, std::size_t size,
                             MemoryLayout::ByteOrder byteOrder) -> std::size_t {
  if (byteOrder == MemoryLayout::ByteOrder::kLittleEndian) return byte * 8;
  return (size - byte - 1) * 8;
}

}  // namespace

auto bitCastConstant(ASTInterpreter& interpreter, const ConstValue& value,
                     const Type* sourceType, const Type* targetType)
    -> std::optional<ConstValue> {
  auto traits = interpreter.translationUnit()->typeTraits();
  const auto& layout = *interpreter.control()->memoryLayout();
  auto source = shapeOf(sourceType, traits, layout);
  auto target = shapeOf(targetType, traits, layout);
  if (!source || !target ||
      source->elementSize * source->elementCount !=
          target->elementSize * target->elementCount) {
    return std::nullopt;
  }

  const InitializerList* sourceElements = nullptr;
  if (source->vectorType) {
    auto list = std::get_if<std::shared_ptr<InitializerList>>(&value);
    if (!list || !*list || (*list)->elements.size() != source->elementCount) {
      return std::nullopt;
    }
    sourceElements = list->get();
  }

  std::shared_ptr<InitializerList> result;
  if (target->vectorType) {
    result = std::make_shared<InitializerList>();
    result->elements.reserve(target->elementCount);
  }

  const auto byteOrder = layout.byteOrder();
  ConstInt::UWide sourceBits = 0;
  for (std::size_t targetIndex = 0; targetIndex < target->elementCount;
       ++targetIndex) {
    ConstInt::UWide targetBits = 0;
    for (std::size_t targetByte = 0; targetByte < target->elementSize;
         ++targetByte) {
      const auto objectByte = targetIndex * target->elementSize + targetByte;
      const auto sourceIndex = objectByte / source->elementSize;
      const auto sourceByte = objectByte % source->elementSize;
      if (sourceByte == 0) {
        const auto& sourceValue =
            sourceElements ? std::get<0>(sourceElements->elements[sourceIndex])
                           : value;
        auto bits = scalarBits(interpreter, sourceValue, source->elementType);
        if (!bits) return std::nullopt;
        sourceBits = *bits;
      }

      const auto sourceShift =
          byteShift(sourceByte, source->elementSize, byteOrder);
      const auto targetShift =
          byteShift(targetByte, target->elementSize, byteOrder);
      const auto byte = (sourceBits >> sourceShift) & 0xff;
      targetBits |= byte << targetShift;
    }

    auto targetValue =
        scalarValue(interpreter, targetBits, target->elementType);
    if (!targetValue) return std::nullopt;
    if (!result) return targetValue;
    result->elements.emplace_back(std::move(*targetValue), target->elementType);
  }

  return ConstValue{std::move(result)};
}

}  // namespace cxx
