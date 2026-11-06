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
#include <cxx/names.h>
#include <cxx/types.h>
#include <cxx/util.h>
#include <cxx/vector_components.h>

#include <cstdint>
#include <string_view>

namespace cxx {

namespace {

[[nodiscard]] auto pointComponentIndex(char component) -> int {
  switch (component) {
    case 'x':
    case 'r':
      return 0;
    case 'y':
    case 'g':
      return 1;
    case 'z':
    case 'b':
      return 2;
    case 'w':
    case 'a':
      return 3;
    default:
      return -1;
  }
}

[[nodiscard]] auto hexadecimalComponentIndex(char component) -> int {
  if (component >= '0' && component <= '9') return component - '0';
  if (component >= 'A' && component <= 'F') return component - 'A' + 10;
  if (component >= 'a' && component <= 'f') return component - 'a' + 10;
  return -1;
}

[[nodiscard]] auto isRgbaComponent(char component) -> bool {
  return component == 'r' || component == 'g' || component == 'b' ||
         component == 'a';
}

}  // namespace

auto classifyVectorComponent(const Identifier* identifier,
                             std::size_t sourceElementCount)
    -> std::expected<VectorComponentSelection, VectorComponentError> {
  if (!identifier || identifier->name().empty())
    return std::unexpected(VectorComponentError{});

  VectorComponentSelection selection;
  const std::string_view name = identifier->name();
  if (name == "lo" || name == "hi" || name == "even" || name == "odd") {
    selection.kind = name == "lo"     ? VectorComponentKind::kLow
                     : name == "hi"   ? VectorComponentKind::kHigh
                     : name == "even" ? VectorComponentKind::kEven
                                      : VectorComponentKind::kOdd;
    selection.elementCount = (sourceElementCount + 1) / 2;
    return selection;
  }

  std::string_view components = name;
  const bool hasHexadecimalPrefix =
      components.size() > 1 &&
      (components.front() == 's' || components.front() == 'S');
  if (hasHexadecimalPrefix) components.remove_prefix(1);

  const int firstPoint = pointComponentIndex(components.front());
  const bool pointAccessor = !hasHexadecimalPrefix && firstPoint >= 0;
  if (!pointAccessor && !hasHexadecimalPrefix)
    return std::unexpected(VectorComponentError{
        VectorComponentErrorKind::kIllegalName, components.front()});

  const bool rgba = pointAccessor && isRgbaComponent(components.front());
  selection.kind = pointAccessor ? (rgba ? VectorComponentKind::kRgba
                                         : VectorComponentKind::kXyzw)
                                 : VectorComponentKind::kHexadecimal;

  std::uint16_t selectedLanes = 0;
  for (char component : components) {
    const int index = pointAccessor ? pointComponentIndex(component)
                                    : hexadecimalComponentIndex(component);
    if (index < 0 || (pointAccessor && isRgbaComponent(component) != rgba)) {
      return std::unexpected(VectorComponentError{
          VectorComponentErrorKind::kIllegalName, component});
    }
    if (static_cast<std::size_t>(index) >= sourceElementCount) {
      return std::unexpected(VectorComponentError{
          VectorComponentErrorKind::kOutOfRange, component});
    }
    const std::uint16_t lane = std::uint16_t{1} << index;
    selection.hasRepeatedLane |= (selectedLanes & lane) != 0;
    selectedLanes |= lane;
  }

  selection.elementCount = components.size();
  return selection;
}

auto vectorComponentAccess(MemberExpressionAST* ast)
    -> std::optional<VectorComponentAccess> {
  if (!ast || ast->vectorComponentKind == VectorComponentKind::kNone)
    return std::nullopt;

  auto name = ast_cast<NameIdAST>(ast->unqualifiedId);
  if (!name || !name->identifier || !ast->baseExpression)
    cxx_runtime_error("invalid retained vector component selection");

  auto objectType = ast->baseExpression->type;
  if (ast->accessOp == TokenKind::T_MINUS_GREATER) {
    auto pointer = unqualified_cast<PointerType>(objectType);
    if (!pointer) cxx_runtime_error("invalid retained vector component base");
    objectType = pointer->elementType();
  }
  auto vector = unqualified_cast<VectorType>(objectType);
  if (!vector || vector->vectorKind() != VectorKind::kExt)
    cxx_runtime_error("invalid retained vector component type");

  auto resultVector = unqualified_cast<VectorType>(ast->type);
  const std::size_t elementCount =
      resultVector ? resultVector->elementCount() : 1;
  return VectorComponentAccess{vector, name->identifier,
                               ast->vectorComponentKind, elementCount};
}

auto vectorComponentIndex(const VectorComponentAccess& access,
                          std::size_t resultPosition) -> std::size_t {
  const std::string_view name = access.identifier->name();
  switch (access.kind) {
    case VectorComponentKind::kXyzw:
    case VectorComponentKind::kRgba:
      return pointComponentIndex(name[resultPosition]);
    case VectorComponentKind::kHexadecimal:
      return hexadecimalComponentIndex(name[resultPosition + 1]);
    case VectorComponentKind::kLow:
      return resultPosition;
    case VectorComponentKind::kHigh:
      return (access.vectorType->elementCount() + 1) / 2 + resultPosition;
    case VectorComponentKind::kEven:
      return 2 * resultPosition;
    case VectorComponentKind::kOdd:
      return 2 * resultPosition + 1;
    case VectorComponentKind::kNone:
      cxx_runtime_error("invalid vector component kind");
  }
  cxx_runtime_error("invalid vector component kind");
}

}  // namespace cxx
