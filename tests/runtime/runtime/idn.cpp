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

#include <sprt/runtime/utils/idn.h>
#include <sprt/compat/idn2.h>
#include <sprt/runtime/stream.h>
#include <sprt/c/__sprt_string.h>

namespace sprt {

static int s_checks = 0;
static int s_failures = 0;

static void check(bool ok, StringView what) {
	++s_checks;
	if (!ok) {
		++s_failures;
		sprt::cerr << "  FAIL: " << what << "\n";
	}
}

// Runs to_ascii/to_unicode and compares both the Status and, on success, the result.
static void checkConvert(bool toAscii, StringView source, Status expectedStatus,
		StringView expectedResult, idn::Options options = idn::Options::Default) {
	StringView result;
	char buf[512] = {0};
	bool invoked = false;

	auto sink = [&](StringView str) {
		invoked = true;
		if (str.size() < sizeof(buf)) {
			::__sprt_memcpy(buf, str.data(), str.size());
			result = StringView(buf, str.size());
		}
	};

	auto status =
			toAscii ? idn::to_ascii(sink, source, options) : idn::to_unicode(sink, source, options);

	++s_checks;
	if (status != expectedStatus) {
		++s_failures;
		sprt::cerr << "  FAIL: " << (toAscii ? "to_ascii(" : "to_unicode(") << source << ") -> "
				   << status << ", expected " << expectedStatus << "\n";
		return;
	}

	// The callback must fire exactly on success, and never on failure - a caller
	// that reads its buffer after an error must find nothing there.
	++s_checks;
	if (invoked != (status == Status::Ok)) {
		++s_failures;
		sprt::cerr << "  FAIL: " << source << ": callback " << (invoked ? "fired" : "did not fire")
				   << " but status is " << status << "\n";
		return;
	}

	if (status == Status::Ok) {
		++s_checks;
		if (result != expectedResult) {
			++s_failures;
			sprt::cerr << "  FAIL: " << (toAscii ? "to_ascii(" : "to_unicode(") << source
					   << ") -> '" << result << "', expected '" << expectedResult << "'\n";
		}
	}
}

static void checkTransitionalDiffers(StringView source, bool expected) {
	bool differs = false;
	auto status = idn::to_ascii([](StringView) { }, source, idn::Options::Default, &differs);
	++s_checks;
	if (status != Status::Ok || differs != expected) {
		++s_failures;
		sprt::cerr << "  FAIL: transitionalDifferent(" << source << ") = " << differs << " (status "
				   << status << "), expected " << expected << "\n";
	}
}

void performIdnTests() {
	s_checks = 0;
	s_failures = 0;

	using idn::Options;

	// --- ASCII fast path -----------------------------------------------------
	checkConvert(true, "example.com", Status::Ok, "example.com");
	checkConvert(true, "EXAMPLE.COM", Status::Ok, "example.com");
	checkConvert(true, "example.com.", Status::Ok, "example.com.");
	checkConvert(true, "a.b.c", Status::Ok, "a.b.c");
	checkConvert(false, "example.com", Status::Ok, "example.com");

	// --- round trips ---------------------------------------------------------
	checkConvert(true, "münchen.de", Status::Ok, "xn--mnchen-3ya.de");
	checkConvert(false, "xn--mnchen-3ya.de", Status::Ok, "münchen.de");
	checkConvert(true, "日本.jp", Status::Ok, "xn--wgv71a.jp");
	checkConvert(false, "xn--wgv71a.jp", Status::Ok, "日本.jp");

	// --- transitional vs nontransitional -------------------------------------
	// The whole reason Options exists: the same name maps two ways.
	checkConvert(true, "faß.de", Status::Ok, "xn--fa-hia.de", Options::Default);
	checkConvert(true, "faß.de", Status::Ok, "fass.de",
			Options::CheckBidi | Options::CheckContextJ);
	checkConvert(true, "ς.com", Status::Ok, "xn--3xa.com", Options::Default);
	checkConvert(true, "ς.com", Status::Ok, "xn--3xa.com",
			Options::Default | Options::NonTransitionalToAscii);
	checkTransitionalDiffers("faß.de", true);
	checkTransitionalDiffers("example.com", false);

	// --- STD3 ----------------------------------------------------------------
	checkConvert(true, "a_b.com", Status::Ok, "a_b.com");
	checkConvert(true, "a_b.com", Status::ErrorIdnDisallowed, StringView(),
			Options::Default | Options::UseStd3Rules);

	// --- hyphen rules --------------------------------------------------------
	checkConvert(true, "-a.com", Status::ErrorIdnLeadingHyphen, StringView());
	checkConvert(true, "a-.com", Status::ErrorIdnTrailingHyphen, StringView());
	checkConvert(true, "ab--cd.com", Status::ErrorIdnHyphen34, StringView());
	checkConvert(true, "a--b.com", Status::Ok, "a--b.com");

	// --- lengths -------------------------------------------------------------
	checkConvert(true, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com",
			Status::Ok,
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com"); // 63
	checkConvert(true, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com",
			Status::ErrorIdnLabelTooLong, StringView()); // 64

	// --- malformed ACE labels ------------------------------------------------
	checkConvert(true, "xn--.com", Status::ErrorIdnInvalidAceLabel, StringView());
	checkConvert(true, "xn---.com", Status::ErrorIdnPunycode, StringView());
	checkConvert(true, "xn--a-.com", Status::ErrorIdnInvalidAceLabel, StringView());

	// --- empty label ---------------------------------------------------------
	checkConvert(true, "a..b", Status::ErrorIdnEmptyLabel, StringView());
	checkConvert(true, ".a", Status::ErrorIdnEmptyLabel, StringView());

	// --- CONTEXTJ ------------------------------------------------------------
	// A bare ZWNJ has no joining context.
	checkConvert(true, "a‌b.com", Status::ErrorIdnContextJ, StringView());
	// After a virama it is allowed (Devanagari KA + virama + ZWNJ + SSA).
	checkConvert(true, "क्‌ष.com", Status::Ok, "xn--11b2ezcs70k.com");

	// --- CONTEXTO (off by default, on in the IDNA2008 profile) ---------------
	checkConvert(true, "a·b.com", Status::Ok, "xn--ab-0ea.com");
	checkConvert(true, "a·b.com", Status::ErrorIdnContextOPunctuation, StringView(),
			Options::Idna2008);
	checkConvert(true, "l·l.com", Status::Ok, "xn--ll-0ea.com", Options::Idna2008);

	// --- Bidi ----------------------------------------------------------------
	// An RTL label may not end in an L character.
	checkConvert(true, "אבa.com", Status::ErrorIdnBidi, StringView());
	checkConvert(true, "אב.com", Status::Ok, "xn--4dbc.com");

	// --- argument contract ---------------------------------------------------
	checkConvert(true, StringView(), Status::ErrorInvalidArguemnt, StringView());
	checkConvert(false, StringView(), Status::ErrorInvalidArguemnt, StringView());

	// --- single-label entry points -------------------------------------------
	{
		StringView result;
		char buf[128] = {0};
		auto sink = [&](StringView str) {
			::__sprt_memcpy(buf, str.data(), str.size());
			result = StringView(buf, str.size());
		};
		check(idn::label_to_ascii(sink, "münchen") == Status::Ok && result == "xn--mnchen-3ya",
				"label_to_ascii");
		// A dot is a separator for a name and an error for a label.
		check(idn::label_to_ascii([](StringView) { }, "a.b") == Status::ErrorIdnLabelHasDot,
				"label_to_ascii rejects a dot");
	}

	// --- the raw punycode codec ----------------------------------------------
	{
		StringView result;
		char buf[128] = {0};
		auto sink = [&](StringView str) {
			::__sprt_memcpy(buf, str.data(), str.size());
			result = StringView(buf, str.size());
		};
		check(idn::puny_encode(sink, "münchen", true) == Status::Ok && result == "xn--mnchen-3ya",
				"puny_encode");
		check(idn::puny_decode(sink, "xn--mnchen-3ya", true) == Status::Ok && result == "münchen",
				"puny_decode");
		check(idn::puny_decode([](StringView) { }, "xn--!!!", true) != Status::Ok,
				"puny_decode rejects garbage");
	}

	// --- Status plumbing -----------------------------------------------------
	// If the range were not wired into runtime_core_status.cpp these would print as
	// "Status::Unknown(...)", which is exactly the kind of thing nobody notices.
	check(getStatusName(Status::ErrorIdnBidi) == "Status::ErrorIdnBidi", "getStatusName(Bidi)");
	check(getStatusName(Status::ErrorIdnPunycode) == "Status::ErrorIdnPunycode",
			"getStatusName(Punycode)");
	check(status::isIdn(Status::ErrorIdnDisallowed), "isIdn(Disallowed)");
	check(!status::isIdn(Status::ErrorNotFound), "!isIdn(ErrorNotFound)");
	check(!status::isWinApi(Status::ErrorIdnDisallowed), "IDN codes are not WinAPI codes");

	// --- the libidn2 C ABI ---------------------------------------------------
	{
		char *out = nullptr;
		check(idn2_lookup_u8((const uint8_t *)"münchen.de", (uint8_t **)&out, 0) == IDN2_OK && out
						&& StringView(out) == "xn--mnchen-3ya.de",
				"idn2_lookup_u8");
		idn2_free(out);

		out = nullptr;
		check(idn2_to_unicode_8z8z("xn--mnchen-3ya.de", &out, 0) == IDN2_OK && out
						&& StringView(out) == "münchen.de",
				"idn2_to_unicode_8z8z performs ToUnicode");
		idn2_free(out);

		out = nullptr;
		check(idn2_lookup_u8((const uint8_t *)"-a.com", (uint8_t **)&out, 0)
						== IDN2_HYPHEN_STARTEND,
				"idn2 error codes are idn2_rc");
		idn2_free(out);

		check(StringView(idn2_strerror(IDN2_OK)) == "success", "idn2_strerror");
		check(idn2_check_version(nullptr) != nullptr, "idn2_check_version");
	}

	// --- the TLD table -------------------------------------------------------
	check(idn::is_known_tld("com"), "is_known_tld(com)");
	check(idn::is_known_tld("рф"), "is_known_tld(рф)");
	check(!idn::is_known_tld("nosuchtld"), "!is_known_tld(nosuchtld)");

	sprt::cout << "idn tests: " << s_checks << " checks, " << s_failures << " failures\n";
}

} // namespace sprt
