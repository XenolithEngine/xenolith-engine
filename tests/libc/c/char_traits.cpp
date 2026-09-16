/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// std::char_traits + std::hash conformance. On the freestanding x86_64-pc-windows-msvc
// build <string>/<functional> are the sprt-backed STL headers; on the Linux host they
// are the system headers. compare.sh diffs the two, so the sprt implementations are
// checked against the system reference. char_traits results are deterministic; hash
// VALUES are not portable, so only structural hash properties (enabled/disabled,
// consistency, distinctness) are printed.
//
// The multi-byte ordering cases below double as the regression test for the sprt string
// comparison fix: sprt::__basic_string routes its comparisons through sprt::char_traits
// (== std::char_traits here), whose compare() delegates to the same __constexpr_strcompare
// primitive. Each char16_t/char32_t case is chosen so a byte-wise memcmp on little-endian
// would give the OPPOSITE sign to the value-wise order the standard requires.

#include <stdio.h>

#include <string>
#include <functional>
#include <type_traits>

namespace sprt::test {

namespace {

using ct = std::char_traits<char>;

// Normalize a char_traits::compare result to its sign. The standard fixes only the
// sign (<0 / 0 / >0); the magnitude is unspecified and can differ between libstdc++
// and sprt, so only the sign is portable / diffable.
static int sgn(int v) noexcept { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

struct NotHashable {
	int x;
	int y;
};
enum Color {
	red,
	green
};

// compile-time conformance, validated on both targets
static_assert(ct::length("hello") == 5);
static_assert(ct::lt('\x01', '\xff')); // char compares as unsigned char
static_assert(ct::compare("\xff", "\x01", 1) > 0);
static_assert(ct::eof() == -1);
static_assert(std::is_same_v<ct::int_type, int>);
static_assert(std::char_traits<char16_t>::eof() == 0xFFFFu);
static_assert(std::char_traits<char32_t>::eof() == 0xFFFF'FFFFu);

// Multi-byte element ordering is by element VALUE, not object bytes. On little-endian a
// byte-wise memcmp compares the low byte first and would disagree with each case here:
//   0x0100 vs 0x00e9 : value 0x0100 > 0x00e9, but low bytes 0x00 < 0xe9 (memcmp < 0)
//   0x0102 vs 0x0201 : value 0x0102 < 0x0201, but low bytes 0x02 > 0x01 (memcmp > 0)
// so each assertion fails under a byte-wise comparison and passes under value order.
static_assert(std::char_traits<char16_t>::compare(u"Ā", u"é", 1) > 0);
static_assert(std::char_traits<char16_t>::compare(u"é", u"Ā", 1) < 0);
static_assert(std::char_traits<char16_t>::compare(u"Ă", u"ȁ", 1) < 0);
static_assert(std::char_traits<char16_t>::compare(u"Ă", u"Ă", 1) == 0);
static_assert(std::char_traits<char32_t>::compare(U"\U00000100", U"\U000000e9", 1) > 0);
static_assert(std::char_traits<char32_t>::compare(U"\U00010002", U"\U00020001", 1) < 0);
static_assert(!std::char_traits<char16_t>::lt(u'Ā', u'é'));
static_assert(std::char_traits<char16_t>::lt(u'é', u'Ā'));

// [unord.hash]/2: hash is disabled for types without an enabled specialization.
static_assert(std::is_default_constructible_v<std::hash<int>>);
static_assert(!std::is_default_constructible_v<std::hash<NotHashable>>);

} // namespace

void performCharTraitsTest() {
	// char_traits<char> static functions
	printf("len: %d\n", (int)ct::length("hello"));
	printf("cmp: lt=%d eq=%d gt=%d\n", sgn(ct::compare("abc", "abd", 3)),
			sgn(ct::compare("abc", "abc", 3)), sgn(ct::compare("abd", "abc", 3)));
	printf("cmp_u: %d\n", sgn(ct::compare("\xff", "\x01", 1))); // unsigned char ordering
	printf("eq_lt: eq=%d lt=%d ult=%d\n", (int)ct::eq('a', 'a'), (int)ct::lt('a', 'b'),
			(int)ct::lt('\x01', '\xff'));

	char buf[16];
	ct::assign(buf, 5, 'x');
	ct::copy(buf + 5, "YZ", 2);
	buf[7] = '\0';
	printf("assign_copy: %s\n", buf);

	char ov[8];
	ct::copy(ov, "abcde", 5);
	ct::move(ov + 1, ov, 4); // overlapping forward move
	ov[5] = '\0';
	printf("move: %s\n", ov);

	const char *hay = "hello";
	printf("find: l_at=%d z=%d\n", (int)(ct::find(hay, 5, 'l') - hay),
			(int)(ct::find(hay, 5, 'z') == nullptr));
	printf("int: ff=%d eof=%d not_eof_eof=%d not_eof_A=%d eq_eof=%d\n", ct::to_int_type('\xff'),
			ct::eof(), ct::not_eof(ct::eof()), ct::not_eof(65),
			(int)ct::eq_int_type(ct::eof(), ct::eof()));

	// other character types
	printf("c16: len=%d eof=%lu\n", (int)std::char_traits<char16_t>::length(u"hi"),
			(unsigned long)std::char_traits<char16_t>::eof());
	printf("c32: eof=%lu\n", (unsigned long)std::char_traits<char32_t>::eof());
	printf("c8: len=%d\n", (int)std::char_traits<char8_t>::length(u8"abcd"));

	// multi-byte ordering must follow element value, not object bytes (little-endian)
	printf("c16_ord: gt=%d lt=%d eq=%d lt_op=%d\n",
			sgn(std::char_traits<char16_t>::compare(u"Ā", u"é", 1)),
			sgn(std::char_traits<char16_t>::compare(u"Ă", u"ȁ", 1)),
			sgn(std::char_traits<char16_t>::compare(u"Ă", u"Ă", 1)),
			(int)std::char_traits<char16_t>::lt(u'é', u'Ā'));
	printf("c32_ord: gt=%d lt=%d\n",
			sgn(std::char_traits<char32_t>::compare(U"\U00000100", U"\U000000e9", 1)),
			sgn(std::char_traits<char32_t>::compare(U"\U00010002", U"\U00020001", 1)));
}

void performHashTest() {
	// enabled / disabled (the disabled set must match the system <functional>)
	printf("enabled: int=%d ptr=%d enum=%d char=%d dbl=%d\n",
			(int)std::is_default_constructible_v<std::hash<int>>,
			(int)std::is_default_constructible_v<std::hash<int *>>,
			(int)std::is_default_constructible_v<std::hash<Color>>,
			(int)std::is_default_constructible_v<std::hash<char>>,
			(int)std::is_default_constructible_v<std::hash<double>>);
	printf("disabled: struct_dc=%d struct_cc=%d\n",
			(int)std::is_default_constructible_v<std::hash<NotHashable>>,
			(int)std::is_copy_constructible_v<std::hash<NotHashable>>);

	// values are not portable across implementations; check consistency + distinctness
	std::hash<int> hi;
	printf("int_hash: consistent=%d distinct=%d noexcept=%d\n", (int)(hi(12'345) == hi(12'345)),
			(int)(hi(1) != hi(2)), (int) noexcept(hi(0)));

	std::hash<Color> hc;
	printf("enum_hash: consistent=%d\n", (int)(hc(red) == hc(red)));
}

} // namespace sprt::test
