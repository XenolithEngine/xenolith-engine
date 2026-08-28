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

// The Unicode IdnaTestV2 conformance suite. This is the test that matters: the
// UTS-46 engine is a transcription of a specification, and a mis-port does not
// crash - it returns a plausible, wrong answer for a handful of inputs.
//
// Per row, for each of the three operations the file defines, this asserts what the
// file actually specifies:
//   1. accept vs reject must match exactly, and
//   2. when the operation is expected to succeed, the output string must match.
//
// It does NOT assert which rule a rejection is blamed on. Neither does the standard:
// the [B5 B6] style codes are informational, and ICU's own driver for this file
// (icu4c source/test/intltest/uts46test.cpp, checkIdnaTestResult) compares hasErrors() as a
// boolean and nothing more. The specific Status a given input produces is pinned
// case by case in idn.cpp instead, where each expectation was verified against the
// reference implementation.

#include <sprt/runtime/utils/idn.h>
#include <sprt/runtime/stream.h>
#include <sprt/c/__sprt_string.h>

#include "data/idna_test_v2.cc"

namespace sprt {

struct ConformanceResult {
	int checks = 0;
	int failures = 0;
	int reported = 0;

	static constexpr int MaxReported = 20;

	// Prints the first few failures and counts the rest: 6000 rows of output would
	// bury the summary line that is the actual signal.
	bool report() {
		++failures;
		return reported++ < MaxReported;
	}
};

static void runOne(ConformanceResult &res, StringView source, StringView opName, bool toAscii,
		idn::Options options, StringView expectedResult, bool expectedToFail,
		bool rootLabelOnly = false) {
	char buf[1'024] = {0};
	StringView result;
	auto sink = [&](StringView str) {
		if (str.size() < sizeof(buf)) {
			::__sprt_memcpy(buf, str.data(), str.size());
			result = StringView(buf, str.size());
		}
	};

	auto status =
			toAscii ? idn::to_ascii(sink, source, options) : idn::to_unicode(sink, source, options);

	// An empty source is "no input" to this API, but an empty label to the standard.
	bool failed = status != Status::Ok;

	// Since the 2025 revision the file marks a trailing dot as an A4_2 VerifyDnsLength
	// error. This engine follows ICU in not reporting the empty ROOT label (an empty
	// label anywhere else is still an error), so when A4_2 is the only expected error
	// and the result really is just root-terminated, the expectation is cleared - the
	// same workaround ICU applies in its own driver for this file (uts46test.cpp
	// checkIdnaTestResult, ICU-22882).
	if (toAscii && rootLabelOnly && !failed && result.ends_with('.')
			&& result.find("..") == Max<size_t>) {
		expectedToFail = false;
	}

	++res.checks;
	if (failed != expectedToFail) {
		if (res.report()) {
			if (failed) {
				sprt::cerr << "  FAIL " << opName << "('" << source << "'): rejected with "
						   << status << ", the standard accepts it as '" << expectedResult << "'\n";
			} else {
				sprt::cerr << "  FAIL " << opName << "('" << source
						   << "'): accepted, the standard rejects it\n";
			}
		}
		return;
	}

	if (!expectedToFail) {
		++res.checks;
		if (result != expectedResult) {
			if (res.report()) {
				sprt::cerr << "  FAIL " << opName << "('" << source << "'): got '" << result
						   << "', expected '" << expectedResult << "'\n";
			}
		}
	}
}

void performIdnConformanceTests() {
	ConformanceResult res;

	// The two profiles the file is written for. Every optional check is ON,
	// UseStd3Rules included - the same set ICU's own driver uses (uts46test.cpp,
	// OptionsCommon / OptionsNonTrans). Running with a laxer profile makes hundreds
	// of rows "pass" that the file expects to be rejected.
	const auto transitional = idn::Options::UseStd3Rules | idn::Options::CheckBidi
			| idn::Options::CheckContextJ | idn::Options::CheckContextO;
	const auto nontransitional = transitional | idn::Options::NonTransitionalToAscii
			| idn::Options::NonTransitionalToUnicode;

	for (auto &test : s_idnaTestCases) {
		runOne(res, test.source, "toUnicode", false, nontransitional, test.toUnicode,
				test.unicodeFails);
		runOne(res, test.source, "toAsciiN", true, nontransitional, test.toAsciiN,
				test.asciiNFails, test.asciiNRootLabelOnly);
		runOne(res, test.source, "toAsciiT", true, transitional, test.toAsciiT, test.asciiTFails,
				test.asciiTRootLabelOnly);
	}

	if (res.reported > ConformanceResult::MaxReported) {
		sprt::cerr << "  ... " << (res.reported - ConformanceResult::MaxReported)
				   << " more failures not shown\n";
	}
	sprt::cout << "idn conformance (IdnaTestV2, " << sizeof(s_idnaTestCases) / sizeof(IdnaTestCase)
			   << " cases): " << res.checks << " checks, " << res.failures << " failures\n";
}

} // namespace sprt
