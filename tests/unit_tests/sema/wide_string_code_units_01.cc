// RUN: %cxx --target=x86_64-unknown-linux-gnu -std=c++23 -verify -fsyntax-only %s
// RUN: %cxx --target=x86_64-pc-windows-msvc -std=c++23 -verify -fsyntax-only %s
// RUN: %cxx --target=aarch64-pc-windows-msvc -std=c++23 -verify -fsyntax-only %s

constexpr bool utf16 = sizeof(wchar_t) == 2;
constexpr wchar_t value[] = L"\u03bb\U0001f642\0!";
static_assert(sizeof(value) / sizeof(wchar_t) == (utf16 ? 6 : 5));
static_assert(value[0] == 0x03bb);
static_assert(value[1] == (utf16 ? 0xd83d : 0x1f642));
static_assert(value[2] == (utf16 ? 0xde42 : 0));
static_assert(value[utf16 ? 3 : 2] == 0);
static_assert(value[utf16 ? 4 : 3] == '!');
static_assert(value[utf16 ? 5 : 4] == 0);
static_assert(sizeof(L"\U0001f642") == (utf16 ? 3 : 2) * sizeof(wchar_t));

constexpr wchar_t joined[] = L"\u03bb" "\U0001f642";
static_assert(sizeof(joined) / sizeof(wchar_t) == (utf16 ? 4 : 3));
static_assert(joined[1] == value[1]);
constexpr wchar_t raw[] = LR"(🙂)";
constexpr wchar_t unicode[] = L"🙂";
static_assert(sizeof(unicode) == sizeof(raw));
static_assert(unicode[0] == value[1]);
constexpr char16_t raw16[] = uR"(🙂)";
constexpr char32_t raw32[] = UR"(🙂)";
static_assert(sizeof(raw16) == 3 * sizeof(char16_t));
static_assert(raw16[0] == 0xd83d && raw16[1] == 0xde42);
static_assert(sizeof(raw32) == 2 * sizeof(char32_t));
static_assert(raw32[0] == 0x1f642);
static_assert(sizeof(raw) / sizeof(wchar_t) == (utf16 ? 3 : 2));
static_assert(raw[0] == value[1]);
constexpr wchar_t numeric[] = L"\xd83d\xde42";
static_assert(sizeof(numeric) / sizeof(wchar_t) == 3);
static_assert(numeric[0] == 0xd83d && numeric[1] == 0xde42);

constexpr auto operator""_units(const wchar_t*, decltype(sizeof(0)) count) {
  return count;
}
static_assert(L"\U0001f642"_units == (utf16 ? 2 : 1));

struct Text {
  // The destination retains explicit capacity after code-unit conversion.
  wchar_t data[7] = L"\U0001f642";
};
constexpr Text text;
static_assert(text.data[0] == value[1]);
static_assert(text.data[1] == (utf16 ? 0xde42 : 0));
static_assert(text.data[utf16 ? 2 : 1] == 0 && text.data[6] == 0);
