// RUN: %cxx -fsyntax-only -verify -ast-dump %s | %filecheck %s --check-prefix=DUMP --match-full-lines
// RUN: %cxx -fsyntax-only -verify -ast-print %s | %filecheck %s --check-prefix=PRINT --match-full-lines

namespace support {
int value;
}

[[vendor::marker("payload")]] using namespace support;

int read_value() { return value; }

// clang-format off
//      DUMP:    using-directive
// DUMP-NEXT:      attribute-list
// DUMP-NEXT:        cxx-attribute
// DUMP-NEXT:          attribute-list
// DUMP-NEXT:            attribute
// DUMP-NEXT:              attribute-token: scoped-attribute-token
// DUMP-NEXT:                attribute-namespace: vendor
// DUMP-NEXT:                identifier: marker
// DUMP-NEXT:              attribute-argument-clause: attribute-argument-clause
// DUMP-NEXT:                expression-list
// DUMP-NEXT:                  string-literal-expression [lvalue const char [8]]
// DUMP-NEXT:                    literal: "payload"
// DUMP-NEXT:                    encoding: <string_literal>
// DUMP-NEXT:      unqualified-id: name-id
// DUMP-NEXT:        identifier: support

// PRINT: {{\[\[vendor::marker\("payload"\)\]\] using namespace support;}}
