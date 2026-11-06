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
#include <cxx/names_fwd.h>
#include <cxx/types_fwd.h>

#include <cstddef>
#include <expected>
#include <optional>

namespace cxx {

struct VectorComponentSelection {
  VectorComponentKind kind = VectorComponentKind::kNone;
  std::size_t elementCount = 0;
  bool hasRepeatedLane = false;
};

enum class VectorComponentErrorKind {
  kIllegalName,
  kOutOfRange,
};

struct VectorComponentError {
  VectorComponentErrorKind kind = VectorComponentErrorKind::kIllegalName;
  char component = 0;
};

struct VectorComponentAccess {
  const VectorType* vectorType = nullptr;
  const Identifier* identifier = nullptr;
  VectorComponentKind kind = VectorComponentKind::kNone;
  std::size_t elementCount = 0;
};

// Validates one Clang extended-vector component name without allocating a lane
// array. Successful classification is retained on the member expression.
[[nodiscard]] auto classifyVectorComponent(const Identifier* identifier,
                                           std::size_t sourceElementCount)
    -> std::expected<VectorComponentSelection, VectorComponentError>;

// Returns the retained component-selection fact for an analyzed expression.
[[nodiscard]] auto vectorComponentAccess(MemberExpressionAST* ast)
    -> std::optional<VectorComponentAccess>;

// Returns one lane from a previously validated component selection. Odd-width
// high and odd selections include one padding lane equal to the source count.
[[nodiscard]] auto vectorComponentIndex(const VectorComponentAccess& access,
                                        std::size_t resultPosition)
    -> std::size_t;

}  // namespace cxx
