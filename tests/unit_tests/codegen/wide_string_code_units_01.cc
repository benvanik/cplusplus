// RUN: %cxx --target=x86_64-unknown-linux-gnu -emit-llvm %s -o - | %filecheck %s --check-prefixes=CHECK,WIDE32
// RUN: %cxx --target=x86_64-pc-windows-msvc -emit-llvm %s -o - | %filecheck %s --check-prefixes=CHECK,WIDE16

extern "C" {
const wchar_t* literal() { return L"\U0001f642"; }
extern const wchar_t padded[4] = L"\U0001f642";
extern const char16_t utf16[4] = u"\u03bb";
extern const char32_t utf32[3] = U"\U0001f642";
}

// CHECK-DAG: @utf32 = constant [3 x i32] [i32 128578, i32 0, i32 0]
// CHECK-DAG: @utf16 = constant [4 x i16] [i16 955, i16 0, i16 0, i16 0]
// WIDE32-DAG: @padded = constant [4 x i32] [i32 128578, i32 0, i32 0, i32 0]
// WIDE32-DAG: @.str1 = internal constant [2 x i32] [i32 128578, i32 0]
// WIDE16-DAG: @padded = constant [4 x i16] [i16 -10179, i16 -8638, i16 0, i16 0]
// WIDE16-DAG: @.str1 = internal constant [3 x i16] [i16 -10179, i16 -8638, i16 0]
