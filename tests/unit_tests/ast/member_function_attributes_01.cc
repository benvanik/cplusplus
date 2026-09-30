// RUN: %cxx -fsyntax-only -verify -ast-dump %s | %filecheck %s --match-full-lines

struct Widget {
  [[nodiscard]] int value() { return 0; }
};

// clang-format off
//      CHECK:translation-unit
// CHECK-NEXT:  declaration-list
// CHECK-NEXT:    simple-declaration
// CHECK-NEXT:      decl-specifier-list
// CHECK-NEXT:        class-specifier
// CHECK-NEXT:          class-key: struct
// CHECK-NEXT:          unqualified-id: name-id
// CHECK-NEXT:            identifier: Widget
// CHECK-NEXT:          declaration-list
// CHECK-NEXT:            function-definition
// CHECK-NEXT:              attribute-list
// CHECK-NEXT:                cxx-attribute
// CHECK-NEXT:                  attribute-list
// CHECK-NEXT:                    attribute
// CHECK-NEXT:                      attribute-token: simple-attribute-token
// CHECK-NEXT:                        identifier: nodiscard
// CHECK-NEXT:              decl-specifier-list
// CHECK-NEXT:                integral-type-specifier
// CHECK-NEXT:                  specifier: int
// CHECK-NEXT:              declarator: declarator
// CHECK-NEXT:                core-declarator: id-declarator
// CHECK-NEXT:                  unqualified-id: name-id
// CHECK-NEXT:                    identifier: value
// CHECK-NEXT:                declarator-chunk-list
// CHECK-NEXT:                  function-declarator-chunk
// CHECK-NEXT:                    parameter-declaration-clause: parameter-declaration-clause
// CHECK-NEXT:              function-body: compound-statement-function-body
// CHECK-NEXT:                statement: compound-statement
// CHECK-NEXT:                  statement-list
// CHECK-NEXT:                    return-statement
// CHECK-NEXT:                      expression: int-literal-expression [prvalue int]
// CHECK-NEXT:                        literal: 0
