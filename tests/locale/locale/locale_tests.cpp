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

// The localization module's own tests. Five topics, and each one is a defect this suite exists to
// keep fixed rather than a feature it describes.

#include "SPCommon.h"
#include "XLFontLocale.h"
#include "XLFontLabelBase.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::locale {

using stappler::test::check;
using stappler::test::checkEq;

// UTF-8 in, UTF-8 out, so an expectation can be written as a literal.
static String lookup(StringView key) {
	return string::toUtf8<mem_std::Interface>(string(string::toUtf16<mem_std::Interface>(key)));
}

static String resolve(StringView str) {
	return string::toUtf8<mem_std::Interface>(
			resolveLocaleTags(string::toUtf16<mem_std::Interface>(str)));
}

static String pickWord(StringView key, uint32_t form) {
	return string::toUtf8<mem_std::Interface>(
			numeric(string::toUtf16<mem_std::Interface>(key), form));
}

// The tables every section below reads. Defined once, because `define` is `emplace` and the FIRST
// writer of a key wins - a second definition of the same key would be silently ignored and a test
// that re-defined would be asserting against the wrong table.
static void defineTestTables() {
	static bool defined = false;
	if (defined) {
		return;
	}
	defined = true;

	define("en-us",
			{
				pair("Test:Both", "both"),
				pair("Test:EnOnly", "english only"),
				pair("Test:Template", "saved %1% into %2%"),
				pair("Test:Words", "file:files"),
				pair("Test:Counted", "%1% file:%1% files"),
			});

	define("ru-ru",
			{
				pair("Test:Both", "оба"),
				pair("Test:Template", "%2%: сохранён %1%"),
				pair("Test:Words", "файл:файла:файлов"),
				pair("Test:Counted", "%1% файл:%1% файла:%1% файлов"),
			});

	// Deliberately declared for a territory nobody will ask for, so the language step of the chain is
	// the only thing that can find it.
	define("fa-af",
			{
				pair("Test:Both", "هر دو"),
			});

	define("zh-cn",
			{
				pair("Test:Both", "两者"),
				pair("Test:Words", "文件"),
				pair("Test:Counted", "%1% 个文件"),
			});
}

void performTableTests() {
	defineTestTables();

	setDefault("en-us");
	setLocale("ru-ru");
	checkEq(getLocale(), "ru-ru", "locale.set");
	checkEq(lookup("Test:Both"), "оба", "table.ru");

	setLocale("en-us");
	checkEq(lookup("Test:Both"), "both", "table.en");

	setLocale("zh-cn");
	checkEq(lookup("Test:Both"), "两者", "table.zh");

	checkEq(lookup("Test:NoSuchKeyAnywhere"), "", "table.missing-is-empty");

	/* `"Key"_locale` AND THE METASTRING OVERLOADS EXIST ONLY IF THEY COMPILE.
	They called `metastring::to_std_ustring()`, which is not a member of anything, for as long as
	nobody used them - a template is only checked when instantiated, so three public entry points were
	broken and the tree still built. These two lines are the instantiation. */
	setLocale("en-us");
	checkEq(string::toUtf8<mem_std::Interface>(string("Test:Both"_meta)), "both",
			"table.metastring-key");
	checkEq(string::toUtf8<mem_std::Interface>(numeric("Test:Words"_meta, 0)), "file",
			"table.metastring-numeric");
	checkEq(StringView("Test:Both"_locale.string<mem_std::String>()), "@Locale:Test:Both",
			"table.locale-literal");
}

void performFallbackTests() {
	defineTestTables();
	setDefault("en-us");

	/* THE KEY, NOT THE TABLE. `ru-ru` exists and does not carry this key; the search used to stop
	there and answer empty, so a half-translated locale rendered those labels as nothing at all. */
	setLocale("ru-ru");
	checkEq(lookup("Test:EnOnly"), "english only", "fallback.key-falls-through");
	checkEq(lookup("Test:Both"), "оба", "fallback.key-present-wins");

	// A territory nobody defined reads the table of its own LANGUAGE before it reads the default.
	setLocale("ru-by");
	checkEq(lookup("Test:Both"), "оба", "fallback.language");

	setLocale("fa-ir");
	checkEq(lookup("Test:Both"), "هر دو", "fallback.language-other-territory");

	// And `en-us` is the floor: a language with no table anywhere still reads something.
	setLocale("sv-se");
	checkEq(lookup("Test:Both"), "both", "fallback.floor");

	// The engine's own System strings now exist for all four shipped locales.
	setLocale("zh-cn");
	checkEq(lookup("SystemCopy"), "复制", "fallback.system-zh");
	setLocale("fa-ir");
	checkEq(lookup("SystemCopy"), "رونوشت", "fallback.system-fa");
}

void performPluralTests() {
	defineTestTables();
	setDefault("en-us");

	setLocale("ru-ru");
	checkEq(pluralForm(1), 0, "plural.ru.1");
	checkEq(pluralForm(21), 0, "plural.ru.21");
	checkEq(pluralForm(11), 2, "plural.ru.11-is-many");
	checkEq(pluralForm(2), 1, "plural.ru.2");
	checkEq(pluralForm(24), 1, "plural.ru.24");
	checkEq(pluralForm(12), 2, "plural.ru.12-is-many");
	checkEq(pluralForm(5), 2, "plural.ru.5");
	checkEq(pluralForm(0), 2, "plural.ru.0");
	checkEq(pickWord("Test:Words", pluralForm(1)), "файл", "plural.ru.word.1");
	checkEq(pickWord("Test:Words", pluralForm(3)), "файла", "plural.ru.word.3");
	checkEq(pickWord("Test:Words", pluralForm(7)), "файлов", "plural.ru.word.7");

	setLocale("en-us");
	checkEq(pluralForm(1), 0, "plural.en.1");
	checkEq(pluralForm(0), 1, "plural.en.0");
	checkEq(pluralForm(2), 1, "plural.en.2");
	checkEq(pickWord("Test:Words", pluralForm(1)), "file", "plural.en.word.1");
	checkEq(pickWord("Test:Words", pluralForm(2)), "files", "plural.en.word.2");

	setLocale("zh-cn");
	checkEq(pluralForm(1), 0, "plural.zh.1");
	checkEq(pluralForm(7), 0, "plural.zh.7");
	checkEq(pickWord("Test:Words", pluralForm(7)), "文件", "plural.zh.word");

	// Persian counts zero as `one`, which is the whole reason it is not the English rule.
	setLocale("fa-ir");
	checkEq(pluralForm(0), 0, "plural.fa.0-is-one");
	checkEq(pluralForm(1), 0, "plural.fa.1");
	checkEq(pluralForm(2), 1, "plural.fa.2");

	/* A DEFINITION WITH FEWER FORMS THAN THE LANGUAGE ASKS FOR gives the last form, not nothing.
	`Test:Words` for `zh-cn` is one word, and a Russian-shaped index of 2 used to fall off the end. */
	setLocale("zh-cn");
	checkEq(pickWord("Test:Words", 2), "文件", "plural.short-definition-clamps");

	// `pluralFormat` is the whole of it in one call: the form AND the count, which is what a call
	// site would otherwise have to branch for.
	setLocale("en-us");
	checkEq(pluralFormat("Test:Counted", 1), "1 file", "plural.format.en.1");
	checkEq(pluralFormat("Test:Counted", 5), "5 files", "plural.format.en.5");
	setLocale("ru-ru");
	checkEq(pluralFormat("Test:Counted", 1), "1 файл", "plural.format.ru.1");
	checkEq(pluralFormat("Test:Counted", 3), "3 файла", "plural.format.ru.3");
	checkEq(pluralFormat("Test:Counted", 11), "11 файлов", "plural.format.ru.11");
	setLocale("zh-cn");
	checkEq(pluralFormat("Test:Counted", 7), "7 个文件", "plural.format.zh");
	checkEq(pluralFormat("Test:NoSuchCount", 2), "%Test:NoSuchCount%",
			"plural.format.missing-is-visible");
}

void performTagTests() {
	defineTestTables();
	setDefault("en-us");
	setLocale("en-us");

	checkEq(resolve("@Locale:Test:Both"), "both", "tag.whole-string");
	checkEq(resolve("say %Test:Both% now"), "say both now", "tag.inline");
	checkEq(resolve("say %Test:NoSuchKey% now"), "say %Test:NoSuchKey% now", "tag.unresolved-verbatim");

	// Percent signs in ordinary text must survive: a space is not a tag character, so the scan stops.
	checkEq(resolve("100% of 50% done"), "100% of 50% done", "tag.percent-is-safe");

	/* THE WORD-LIST TAG, which was unreachable twice over: `?` was not a tag character, so the parser
	never produced the token, and the branch behind it passed the whole `?N:Key` to the lookup instead
	of the key. Index 0 was rejected outright, which is the singular in every three-form language. */
	check(hasLocaleTags(string::toUtf16<mem_std::Interface>("%?0:Test:Words%")),
			"tag.plural-is-detected");
	checkEq(resolve("one %?0:Test:Words%"), "one file", "tag.plural.0");
	checkEq(resolve("two %?1:Test:Words%"), "two files", "tag.plural.1");

	// Positional arguments: the TEMPLATE carries the word order, which is the point of having them.
	Vector<StringView> args{StringView("report.txt"), StringView("Documents")};
	checkEq(format("Test:Template", args), "saved report.txt into Documents", "tag.format.en");

	setLocale("ru-ru");
	checkEq(format("Test:Template", args), "Documents: сохранён report.txt", "tag.format.ru-reorders");

	// With the `@Locale:` prefix spelled out, and with an index nobody passed.
	checkEq(format("@Locale:Test:Template", args), "Documents: сохранён report.txt",
			"tag.format.prefixed");
	Vector<StringView> one{StringView("x")};
	checkEq(format("Test:Template", one), "%2%: сохранён x", "tag.format.missing-arg-verbatim");

	// A numeric tag with NO arguments is still a table key, so the old behaviour is untouched.
	setLocale("en-us");
	checkEq(resolve("%1%"), "%1%", "tag.numeric-without-args");

	checkEq(getTextDirection(StringView("fa")) == TextDirection::RightToLeft, 1,
			"tag.direction.fa-is-rtl");
	checkEq(getTextDirection(StringView("ru")) == TextDirection::LeftToRight, 1,
			"tag.direction.ru-is-ltr");
	setLocale("fa-ir");
	checkEq(getTextDirection() == TextDirection::RightToLeft, 1, "tag.direction.current-locale");
}

namespace {

// `LabelBase` is a plain class, so the flag can be asserted without a Director. The override only
// counts, because what is under test is whether the string was taken as localizable at all.
struct TestLabel : font::LabelBase {
	uint32_t dirtyCount = 0;
	virtual void setLabelDirty() override {
		++dirtyCount;
		font::LabelBase::setLabelDirty();
	}
};

} // namespace

void performLabelTests() {
	defineTestTables();
	setDefault("en-us");
	setLocale("en-us");

	// Auto-detection is the ergonomic: a tag needs no ceremony at the call site.
	TestLabel autoLabel;
	autoLabel.setString(StringView("@Locale:Test:Both"));
	check(autoLabel.isLocaleEnabled(), "label.auto-detects-tag");

	TestLabel plainLabel;
	plainLabel.setString(StringView("ordinary text"));
	check(!plainLabel.isLocaleEnabled(), "label.plain-stays-off");

	/* AND AN EXPLICIT `false` LATCHES. Every line of an open file is a Label, so a source line
	starting with `@Locale:` used to render as NOTHING: turning the flag off did not help, because the
	next `setString` saw it off, detected tags and turned it back on. */
	TestLabel codeLine;
	codeLine.setLocaleEnabled(false);
	codeLine.setString(StringView("@Locale:Test:Both"));
	check(!codeLine.isLocaleEnabled(), "label.explicit-off-survives-setString");
	codeLine.setString(StringView("width: %Test:Both%;"));
	check(!codeLine.isLocaleEnabled(), "label.explicit-off-survives-inline-tag");

	// The stored string is the UNRESOLVED one, which is what makes re-localizing a dirty flag rather
	// than a second copy of every caption.
	checkEq(autoLabel.getString8(), "@Locale:Test:Both", "label.stores-unresolved");

	// And an explicit `true` is equally sticky against a plain string.
	TestLabel forced;
	forced.setLocaleEnabled(true);
	forced.setString(StringView("ordinary text"));
	check(forced.isLocaleEnabled(), "label.explicit-on-stays-on");
}

} // namespace stappler::xenolith::locale
