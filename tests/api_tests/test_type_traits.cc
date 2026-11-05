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

#include <cxx/symbols.h>
#include <cxx/type_traits.h>
#include <gtest/gtest.h>

#include "test_utils.h"

using namespace cxx;

TEST(TypeTraits, SeparatesValueOperationAndDestructorTriviality) {
  auto source = R"(
    struct Guard {
      unsigned value;
      Guard() = default;
      Guard(const Guard&) = default;
      Guard(Guard&&) = default;
      Guard& operator=(const Guard&) = default;
      Guard& operator=(Guard&&) = default;
      ~Guard() {}
    };
    struct Owner { Guard guard; };
    struct Mixed {
      Mixed() = default;
      Mixed(const Mixed&) {}
      Mixed(Mixed&&) = default;
      Mixed& operator=(const Mixed&) = default;
      Mixed& operator=(Mixed&&) { return *this; }
      ~Mixed() {}
    };
  )"_cxx;
  auto traits = source.unit.typeTraits();

  const auto expectTrivial = [&](FunctionSymbol* function, bool expected) {
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(traits.is_trivial_special_member(function), expected);
  };

  auto* guard = source.getAs<ClassSymbol>("Guard");
  ASSERT_TRUE(guard);
  expectTrivial(guard->copyConstructor(), true);
  expectTrivial(guard->moveConstructor(), true);
  expectTrivial(guard->copyAssignmentOperator(), true);
  expectTrivial(guard->moveAssignmentOperator(), true);
  expectTrivial(guard->destructor(), false);

  auto* owner = source.getAs<ClassSymbol>("Owner");
  ASSERT_TRUE(owner);
  expectTrivial(owner->copyConstructor(), true);
  expectTrivial(owner->moveConstructor(), true);
  expectTrivial(owner->copyAssignmentOperator(), true);
  expectTrivial(owner->moveAssignmentOperator(), true);
  expectTrivial(owner->destructor(), false);

  auto* mixed = source.getAs<ClassSymbol>("Mixed");
  ASSERT_TRUE(mixed);
  expectTrivial(mixed->copyConstructor(), false);
  expectTrivial(mixed->moveConstructor(), true);
  expectTrivial(mixed->copyAssignmentOperator(), true);
  expectTrivial(mixed->moveAssignmentOperator(), false);
  expectTrivial(mixed->destructor(), false);
}
