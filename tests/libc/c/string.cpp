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

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

void performStringCompareTest() {
	// strlen / strnlen
	static const char *lenCases[] = {"", "a", "hello", "with\0embedded", "ascii123!@#"};
	for (auto s : lenCases) { printf("strlen(%s)=%zu\n", s, strlen(s)); }
	printf("strnlen(hello,3)=%zu\n", strnlen("hello", 3));
	printf("strnlen(hello,10)=%zu\n", strnlen("hello", 10));
	printf("strnlen(\"\",5)=%zu\n", strnlen("", 5));
	printf("strnlen(hello,0)=%zu\n", strnlen("hello", 0));

	// strcmp / strncmp — sign only
	struct Pair {
		const char *a;
		const char *b;
	};
	static const Pair cmp[] = {
		{"", ""},
		{"a", ""},
		{"", "a"},
		{"abc", "abc"},
		{"abc", "abd"},
		{"abd", "abc"},
		{"abc", "abcd"},
		{"abcd", "abc"},
		{"ABC", "abc"},
		{"\x7f", "\x80"}, // signed vs unsigned char behaviour
		{"\x80", "\x7f"},
		{"héllo", "hello"},
	};
	for (auto &p : cmp) {
		printf("strcmp(%s,%s)=%d\n", p.a, p.b, sgn(strcmp(p.a, p.b)));
	}
	for (auto &p : cmp) {
		printf("strncmp(%s,%s,2)=%d strncmp(...,4)=%d\n", p.a, p.b, sgn(strncmp(p.a, p.b, 2)),
				sgn(strncmp(p.a, p.b, 4)));
	}
	printf("strncmp(abc,abd,0)=%d\n", sgn(strncmp("abc", "abd", 0)));

	// strcasecmp / strncasecmp
	static const Pair icmp[] = {
		{"ABC", "abc"},
		{"abc", "ABC"},
		{"AbC", "abd"},
		{"Hello", "HELLO"},
		{"abc", "abcd"},
		{"a", "B"},
		{"B", "a"},
	};
	for (auto &p : icmp) {
		printf("strcasecmp(%s,%s)=%d\n", p.a, p.b, sgn(strcasecmp(p.a, p.b)));
	}
	printf("strncasecmp(Hello,help,3)=%d\n", sgn(strncasecmp("Hello", "help", 3)));
	printf("strncasecmp(Hello,help,4)=%d\n", sgn(strncasecmp("Hello", "help", 4)));

	// strcoll in the "C" locale == strcmp
	printf("strcoll(abc,abd)=%d\n", sgn(strcoll("abc", "abd")));
	printf("strcoll(abc,abc)=%d\n", sgn(strcoll("abc", "abc")));

	// strverscmp (GNU natural ordering)
	static const Pair vers[] = {
		{"a", "a"},
		{"a1", "a2"},
		{"a10", "a9"},
		{"a009", "a10"},
		{"foo1", "foo10"},
		{"file5", "file40"},
		{"000", "00"},
	};
	for (auto &p : vers) {
		printf("strverscmp(%s,%s)=%d\n", p.a, p.b, sgn(strverscmp(p.a, p.b)));
	}
}

void performStringSearchTest() {
	const char *s = "Hello, World! Hello!";
	size_t len = strlen(s);

	// strchr / strrchr
	printOffset("strchr(o)", s, strchr(s, 'o'), len);
	printOffset("strrchr(o)", s, strrchr(s, 'o'), len);
	printOffset("strchr(z)", s, strchr(s, 'z'), len);
	printOffset("strchr(NUL)", s, strchr(s, '\0'), len); // points at terminator
	printOffset("strrchr(NUL)", s, strrchr(s, '\0'), len);
	printOffset("strchr(H)", s, strchr(s, 'H'), len);
	printOffset("strrchr(H)", s, strrchr(s, 'H'), len);

	// strstr
	printOffset("strstr(Hello)", s, strstr(s, "Hello"), len);
	printOffset("strstr(World)", s, strstr(s, "World"), len);
	printOffset("strstr(xyz)", s, strstr(s, "xyz"), len);
	printOffset("strstr(empty)", s, strstr(s, ""), len);
	printOffset("strstr(!)", s, strstr(s, "!"), len);

	// strpbrk
	printOffset("strpbrk(,!)", s, strpbrk(s, ",!"), len);
	printOffset("strpbrk(xyz)", s, strpbrk(s, "xyz"), len);
	printOffset("strpbrk(empty)", s, strpbrk(s, ""), len);

	// strspn / strcspn
	printf("strspn(Hello,Helo)=%zu\n", strspn("Hello", "Helo"));
	printf("strspn(Hello,xyz)=%zu\n", strspn("Hello", "xyz"));
	printf("strspn(12345abc,0123456789)=%zu\n", strspn("12345abc", "0123456789"));
	printf("strcspn(Hello, lo)=%zu\n", strcspn("Hello", "lo"));
	printf("strcspn(Hello,xyz)=%zu\n", strcspn("Hello", "xyz"));
	printf("strcspn(Hello,H)=%zu\n", strcspn("Hello", "H"));
}

void performStringCopyTest() {
	char buf[32];

	// strcpy / strncpy
	memset(buf, '#', sizeof(buf));
	strcpy(buf, "hello");
	printf("strcpy=%s len=%zu\n", buf, strlen(buf));

	memset(buf, '#', sizeof(buf));
	strncpy(buf, "hello", 10); // pads with NUL up to n
	printHex("strncpy(hello,10)", buf, 12);

	memset(buf, '#', sizeof(buf));
	strncpy(buf, "hello", 3); // no terminator written
	printHex("strncpy(hello,3)", buf, 6);

	// strcat / strncat
	strcpy(buf, "foo");
	strcat(buf, "bar");
	printf("strcat=%s\n", buf);

	strcpy(buf, "foo");
	strncat(buf, "barbaz", 3);
	printf("strncat(foo,barbaz,3)=%s\n", buf);

	strcpy(buf, "foo");
	strncat(buf, "bar", 0);
	printf("strncat(foo,bar,0)=%s\n", buf);

	// strdup
	char *d = strdup("duplicate me");
	printf("strdup=%s len=%zu\n", d, strlen(d));
	free(d);

	// strxfrm in "C" locale: produces a transform comparable like strcmp.
	// Only the resulting length and ordering are portable, not the bytes.
	char x1[32];
	char x2[32];
	size_t n1 = strxfrm(x1, "abc", sizeof(x1));
	size_t n2 = strxfrm(x2, "abd", sizeof(x2));
	printf("strxfrm len abc=%zu abd=%zu cmp=%d\n", n1, n2, sgn(strcmp(x1, x2)));
}

void performStringTokenTest() {
	// strtok (stateful)
	char buf[64];
	strcpy(buf, "  one,two;;three  four ");
	int i = 0;
	for (char *t = strtok(buf, " ,;"); t; t = strtok(nullptr, " ,;")) {
		printf("strtok[%d]=%s\n", i++, t);
	}

	// strtok_r (reentrant), interleaved to prove independent state
	char a[] = "a1:a2:a3";
	char b[] = "b1-b2-b3";
	char *sa = nullptr;
	char *sb = nullptr;
	char *ta = strtok_r(a, ":", &sa);
	char *tb = strtok_r(b, "-", &sb);
	while (ta || tb) {
		printf("strtok_r a=%s b=%s\n", ta ? ta : "(null)", tb ? tb : "(null)");
		ta = strtok_r(nullptr, ":", &sa);
		tb = strtok_r(nullptr, "-", &sb);
	}
}

void performMemoryTest() {
	unsigned char buf[32];

	// memset
	memset(buf, 0xAB, sizeof(buf));
	printHex("memset(0xAB,8)", buf, 8);
	memset(buf, 0, sizeof(buf));
	printHex("memset(0,4)", buf, 4);

	// memcpy
	const char *src = "0123456789ABCDEF";
	memcpy(buf, src, 16);
	printHex("memcpy(16)", buf, 16);

	// memmove with overlap (forward and backward)
	char ov[16];
	memcpy(ov, "abcdefgh", 9);
	memmove(ov + 2, ov, 6); // forward overlap
	printHex("memmove(+2<-0,6)", ov, 9);

	memcpy(ov, "abcdefgh", 9);
	memmove(ov, ov + 2, 6); // backward overlap
	printHex("memmove(0<-+2,6)", ov, 9);

	// memcmp — sign only
	printf("memcmp(abc,abc,3)=%d\n", sgn(memcmp("abc", "abc", 3)));
	printf("memcmp(abc,abd,3)=%d\n", sgn(memcmp("abc", "abd", 3)));
	printf("memcmp(abd,abc,3)=%d\n", sgn(memcmp("abd", "abc", 3)));
	printf("memcmp(abc,abd,2)=%d\n", sgn(memcmp("abc", "abd", 2)));
	printf("memcmp(x,y,0)=%d\n", sgn(memcmp("x", "y", 0)));
	// unsigned-char comparison semantics
	const unsigned char h1[] = {0x7f};
	const unsigned char h2[] = {0x80};
	printf("memcmp(7f,80,1)=%d\n", sgn(memcmp(h1, h2, 1)));

	// memchr
	const char *m = "Hello, World";
	printOffset("memchr(o)", m, memchr(m, 'o', 12), 12);
	printOffset("memchr(z)", m, memchr(m, 'z', 12), 12);
	printOffset("memchr(o,4)", m, memchr(m, 'o', 4), 4); // before first 'o'
}

} // namespace sprt::test
