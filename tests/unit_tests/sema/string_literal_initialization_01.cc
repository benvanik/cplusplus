// RUN: %cxx -std=c++23 -verify -fsyntax-only %s

constexpr char narrow_deduced[] = (("value"));
constexpr char narrow_explicit[6] = ((("value")));
constexpr char8_t utf8_deduced[] = ((u8"value"));
constexpr char8_t utf8_explicit[6] = (((u8"value")));
constexpr char16_t utf16_deduced[] = ((u"value"));
constexpr char16_t utf16_explicit[6] = (((u"value")));
constexpr char32_t utf32_deduced[] = ((U"value"));
constexpr char32_t utf32_explicit[6] = (((U"value")));
constexpr wchar_t wide_deduced[] = ((L"value"));
constexpr wchar_t wide_explicit[6] = (((L"value")));

constexpr char direct_narrow_deduced[](("value"));
constexpr char direct_narrow_explicit[6]((("value")));
constexpr char8_t direct_utf8_deduced[]((u8"value"));
constexpr char8_t direct_utf8_explicit[6](((u8"value")));
constexpr char16_t direct_utf16_deduced[]((u"value"));
constexpr char16_t direct_utf16_explicit[6](((u"value")));
constexpr char32_t direct_utf32_deduced[]((U"value"));
constexpr char32_t direct_utf32_explicit[6](((U"value")));
constexpr wchar_t direct_wide_deduced[]((L"value"));
constexpr wchar_t direct_wide_explicit[6](((L"value")));

constexpr char braced_deduced[]{(("value"))};
constexpr char braced_explicit[6]{((("value")))};

static_assert(sizeof(narrow_deduced) / sizeof(*narrow_deduced) == 6);
static_assert(sizeof(utf8_deduced) / sizeof(*utf8_deduced) == 6);
static_assert(sizeof(utf16_deduced) / sizeof(*utf16_deduced) == 6);
static_assert(sizeof(utf32_deduced) / sizeof(*utf32_deduced) == 6);
static_assert(sizeof(wide_deduced) / sizeof(*wide_deduced) == 6);
static_assert(sizeof(direct_narrow_deduced) /
                  sizeof(*direct_narrow_deduced) ==
              6);
static_assert(sizeof(direct_utf8_deduced) / sizeof(*direct_utf8_deduced) == 6);
static_assert(sizeof(direct_utf16_deduced) / sizeof(*direct_utf16_deduced) ==
              6);
static_assert(sizeof(direct_utf32_deduced) / sizeof(*direct_utf32_deduced) ==
              6);
static_assert(sizeof(direct_wide_deduced) / sizeof(*direct_wide_deduced) == 6);
static_assert(sizeof(braced_deduced) / sizeof(*braced_deduced) == 6);

static_assert(narrow_deduced[0] == 'v' && narrow_deduced[5] == 0);
static_assert(utf8_deduced[0] == u8'v' && utf8_deduced[5] == 0);
static_assert(utf16_deduced[0] == u'v' && utf16_deduced[5] == 0);
static_assert(utf32_deduced[0] == U'v' && utf32_deduced[5] == 0);
static_assert(wide_deduced[0] == L'v' && wide_deduced[5] == 0);
static_assert(direct_narrow_deduced[0] == 'v' &&
              direct_narrow_deduced[5] == 0);
static_assert(braced_deduced[0] == 'v' && braced_deduced[5] == 0);
