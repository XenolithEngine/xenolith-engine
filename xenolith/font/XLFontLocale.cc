/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLFontLocale.h"
#include "SPMemInterface.h"
#include "SPString.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::locale {

class LocaleManager : public memory::PoolObject {
public:
	using StringMap = mem_pool::HashMap<mem_pool::WideString, mem_pool::WideString>;
	using LocaleMap = mem_pool::Map<mem_pool::String, StringMap>;

	using StringIndexMap = mem_pool::HashMap<size_t, mem_pool::WideString>;
	using LocaleIndexMap = mem_pool::Map<mem_pool::String, StringIndexMap>;

	using Interface = memory::PoolInterface;

	static LocaleManager *s_sharedInstance;

	static LocaleManager *getInstance();

	LocaleManager(Ref *ref, memory::pool_t *p);

	bool init();

	void define(const StringView &locale, LocaleInitList &&init);
	void define(const StringView &locale, LocaleIndexList &&init);
	void define(const StringView &locale,
			const sprt::array<StringView, toInt(TimeTokens::Max)> &arr);
	WideStringView string(const WideStringView &str);

	WideStringView string(size_t index);

	WideStringView numeric(const WideStringView &str, uint32_t num);

	void setDefault(StringView def);
	StringView getDefault();
	LocaleInfo getDefaultInfo();

	void setLocale(StringView loc);
	StringView getLocale();
	LocaleInfo getLocaleInfo();

	bool hasLocaleTagsFast(WideStringView r);
	bool hasLocaleTags(WideStringView r);

	uint32_t pluralForm(uint32_t n) const;

	WideStringView resolveTag(WideStringView, SpanView<WideStringView> args);

	mem_std::WideString resolveLocaleTags(WideStringView r, SpanView<WideStringView> args);

	StringView timeToken(TimeTokens tok);
	const sprt::array<mem_pool::String, toInt(TimeTokens::Max)> &timeTokenTable();

protected:
	LocaleIdentifier _default;
	LocaleIdentifier _locale;

	LocaleMap _strings;
	LocaleIndexMap _indexes;
	mem_pool::Map<mem_pool::String, sprt::array<mem_pool::String, toInt(TimeTokens::Max)>>
			_timeTokens;
	sprt::array<mem_pool::String, toInt(TimeTokens::Max)> _defaultTime;

	memory::pool_t *_pool = nullptr;
};

struct LocaleInterface {
	static void initialize(void *ptr) { reinterpret_cast<LocaleInterface *>(ptr)->init(); }
	static void terminate(void *ptr) { reinterpret_cast<LocaleInterface *>(ptr)->term(); }

	LocaleInterface() { addInitializer(this, initialize, terminate); }

	void init() {
		manager = Rc<SharedRef<LocaleManager>>::create(SharedRefMode::Allocator);
		manager->setDefault(sprt::platform::getOsLocale());
		manager->setLocale(sprt::platform::getOsLocale());
	}
	void term() { manager = nullptr; }

	Rc<SharedRef<LocaleManager>> manager;
};

static LocaleInterface s_interface;

LocaleManager *LocaleManager::getInstance() { return s_interface.manager; }

LocaleManager::LocaleManager(Ref *ref, memory::pool_t *p)
: memory::PoolObject(ref, p)
, _defaultTime{"today", "yesterday", "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep",
	  "oct", "nov", "dec"}
, _pool(p) {
	define("ru-ru",
			{
				pair("SystemSearch", "Найти"),
				pair("SystemFontSize", "Размер шрифта"),
				pair("SystemTheme", "Оформление"),
				pair("SystemThemeLight", "Светлая тема"),
				pair("SystemThemeNeutral", "Нейтральная тема"),
				pair("SystemThemeDark", "Темная тема"),
				pair("SystemMore", "Ещё"),
				pair("SystemRestore", "Восстановить"),
				pair("SystemRemoved", "Удалено"),
				pair("SystemCopy", "Копировать"),
				pair("SystemCut", "Вырезать"),
				pair("SystemPaste", "Вставить"),
				pair("SystemTapExit", "Нажмите ещё раз для выхода"),

				pair("SystemErrorOverflowChars", "Слишком много символов"),
				pair("SystemErrorInvalidChar", "Недопустимый символ"),

				pair("Shortcut:Megabytes", "Мб"),
				pair("Shortcut:Pages", "с"),
			});

	define("en-us",
			{
				pair("SystemSearch", "Search"),
				pair("SystemFontSize", "Font size"),
				pair("SystemTheme", "Theme"),
				pair("SystemThemeLight", "Light theme"),
				pair("SystemThemeNeutral", "Neutral theme"),
				pair("SystemThemeDark", "Dark theme"),
				pair("SystemMore", "More"),
				pair("SystemRestore", "Restore"),
				pair("SystemRemoved", "Removed"),
				pair("SystemCopy", "Copy"),
				pair("SystemCut", "Cut"),
				pair("SystemPaste", "Paste"),
				pair("SystemTapExit", "Tap one more time to exit"),

				pair("SystemErrorOverflowChars", "Too many characters"),
				pair("SystemErrorInvalidChar", "Invalid character"),

				pair("Shortcut:Megabytes", "Mb"),
				pair("Shortcut:Pages", "p"),
			});

	/* zh and fa tables for engine-drawn widgets, which would otherwise fall back to English. */
	define("zh-cn",
			{
				pair("SystemSearch", "搜索"),
				pair("SystemFontSize", "字号"),
				pair("SystemTheme", "主题"),
				pair("SystemThemeLight", "浅色主题"),
				pair("SystemThemeNeutral", "中性主题"),
				pair("SystemThemeDark", "深色主题"),
				pair("SystemMore", "更多"),
				pair("SystemRestore", "恢复"),
				pair("SystemRemoved", "已删除"),
				pair("SystemCopy", "复制"),
				pair("SystemCut", "剪切"),
				pair("SystemPaste", "粘贴"),
				pair("SystemTapExit", "再按一次退出"),

				pair("SystemErrorOverflowChars", "字符过多"),
				pair("SystemErrorInvalidChar", "无效字符"),

				pair("Shortcut:Megabytes", "MB"),
				pair("Shortcut:Pages", "页"),
			});

	define("fa-ir",
			{
				pair("SystemSearch", "جست‌وجو"),
				pair("SystemFontSize", "اندازه قلم"),
				pair("SystemTheme", "پوسته"),
				pair("SystemThemeLight", "پوسته روشن"),
				pair("SystemThemeNeutral", "پوسته خنثی"),
				pair("SystemThemeDark", "پوسته تاریک"),
				pair("SystemMore", "بیشتر"),
				pair("SystemRestore", "بازگردانی"),
				pair("SystemRemoved", "حذف شد"),
				pair("SystemCopy", "رونوشت"),
				pair("SystemCut", "برش"),
				pair("SystemPaste", "چسباندن"),
				pair("SystemTapExit", "برای خروج یک بار دیگر بزنید"),

				pair("SystemErrorOverflowChars", "نویسه‌ها بیش از حد است"),
				pair("SystemErrorInvalidChar", "نویسه نامعتبر"),

				pair("Shortcut:Megabytes", "مگابایت"),
				pair("Shortcut:Pages", "ص"),
			});
}

bool LocaleManager::init() { return true; }

void LocaleManager::define(const StringView &locale, LocaleInitList &&init) {
	memory::context ctx(_pool);
	auto it = _strings.find(locale);
	if (it == _strings.end()) {
		it = _strings.emplace(locale.str<Interface>(), StringMap()).first;
	}
	for (auto &iit : init) {
		it->second.emplace(string::toUtf16<Interface>(iit.first),
				string::toUtf16<Interface>(iit.second));
	}
}

void LocaleManager::define(const StringView &locale, LocaleIndexList &&init) {
	memory::context ctx(_pool);
	auto it = _indexes.find(locale);
	if (it == _indexes.end()) {
		it = _indexes.emplace(locale.str<Interface>(), StringIndexMap()).first;
	}
	for (auto &iit : init) {
		it->second.emplace(iit.first, string::toUtf16<Interface>(iit.second));
	}
}

void LocaleManager::define(const StringView &locale,
		const sprt::array<StringView, toInt(TimeTokens::Max)> &arr) {
	memory::context ctx(_pool);
	auto it = _timeTokens.find(locale);
	if (it == _timeTokens.end()) {
		it = _timeTokens
					 .emplace(locale.str<Interface>(),
							 sprt::array<mem_pool::String, toInt(TimeTokens::Max)>())
					 .first;
	}

	size_t i = 0;
	for (auto &arr_it : arr) {
		it->second[i] = arr_it.str<Interface>();
		++i;
	}
}

/* The lookup chain, walked per key rather than per table, so a partially translated locale falls
back for missing keys instead of rendering empty. `find` says whether this table answered:

  1. the exact locale          `fa-ir`
  2. any table of the same language - `fa-af` answers for an undefined `fa-ir`
  3. the default locale, then its language, by the same two rules
  4. `en-us`, the one id this module defines for itself, so there is always a floor
*/
template <typename Map, typename Fn>
static bool LocaleManager_lookup(Map &map, const LocaleIdentifier &locale,
		const LocaleIdentifier &def, const Fn &find) {
	auto exact = [&](StringView id) {
		if (id.empty()) {
			return false;
		}
		auto it = map.find(id);
		return it != map.end() && find(it->second);
	};

	// A language matches `<lang>-<anything>`; the prefix test alone would let `fake-xx` answer for
	// `fa`, so the separator is part of it.
	auto byLanguage = [&](StringView lang) {
		if (lang.empty()) {
			return false;
		}
		for (auto &it : map) {
			StringView id(it.first);
			if (id.size() > lang.size() && id.starts_with(lang) && id[lang.size()] == '-'
					&& find(it.second)) {
				return true;
			}
		}
		return false;
	};

	return exact(locale.id) || byLanguage(locale.language) || exact(def.id)
			|| byLanguage(def.language) || exact(StringView("en-us"));
}

WideStringView LocaleManager::string(const WideStringView &str) {
	WideStringView ret;
	LocaleManager_lookup(_strings, _locale, _default, [&](auto &table) {
		auto sit = table.find(str);
		if (sit == table.end()) {
			return false;
		}
		ret = sit->second;
		return true;
	});
	return ret;
}

WideStringView LocaleManager::string(size_t index) {
	WideStringView ret;
	LocaleManager_lookup(_indexes, _locale, _default, [&](auto &table) {
		auto sit = table.find(index);
		if (sit == table.end()) {
			return false;
		}
		ret = sit->second;
		return true;
	});
	return ret;
}

WideStringView LocaleManager::numeric(const WideStringView &str, uint32_t numEq) {
	auto fmt = string(str);
	WideStringView r(fmt);
	WideStringView last;
	while (!r.empty()) {
		WideStringView def = r.readUntil<WideStringView::Chars<':'>>();
		if (r.is(':')) {
			++r;
		}

		last = def;
		if (numEq == 0) {
			return def;
		}
		--numEq;
	}

	/* A definition with fewer forms than the language needs gives its last form, so text is
	visible (e.g. a two-word table for a Russian count of five). */
	return last;
}

/* The plural form the count takes, as an index into a `numeric` definition. A subset of CLDR
cardinal rules; unknown languages get the two-form rule:

  one form    zh ja ko vi th id ms
  two forms   en de fr es it fa tr ... - `one` then `other`; fa counts 0 as `one`
  three forms ru uk be - `one` (1, 21, 31 ... but not 11), `few` (2-4, 22-24 ... but not 12-14),
              `many` (0, 5-20, 25-30 ...)
*/
uint32_t LocaleManager::pluralForm(uint32_t n) const {
	auto lang = _locale.language;
	if (lang.empty()) {
		lang = _default.language;
	}

	if (lang == "zh" || lang == "ja" || lang == "ko" || lang == "vi" || lang == "th"
			|| lang == "id" || lang == "ms") {
		return 0;
	}

	if (lang == "ru" || lang == "uk" || lang == "be") {
		auto mod10 = n % 10;
		auto mod100 = n % 100;
		if (mod10 == 1 && mod100 != 11) {
			return 0;
		}
		if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) {
			return 1;
		}
		return 2;
	}

	// Persian counts 0 as `one`, unlike English.
	if (lang == "fa" || lang == "hi") {
		return n <= 1 ? 0 : 1;
	}

	return n == 1 ? 0 : 1;
}

void LocaleManager::setDefault(StringView def) {
	auto locid = LocaleIdentifier(def);

	if (locid) {
		if (_default != locid) {
			_default = locid;
		}
	}
}

StringView LocaleManager::getDefault() { return _default.id; }

LocaleInfo LocaleManager::getDefaultInfo() { return LocaleInfo::get(_default); }

void LocaleManager::setLocale(StringView loc) {
	auto locid = LocaleIdentifier(loc);

	if (locid) {
		if (_locale != locid) {
			_locale = locid;
			onLocale(nullptr, loc);
		}
	}
}
StringView LocaleManager::getLocale() { return _locale.id; }

LocaleInfo LocaleManager::getLocaleInfo() { return LocaleInfo::get(_locale); }

bool LocaleManager::hasLocaleTagsFast(WideStringView r) {
	if (r.empty()) {
		return false;
	}

	if (r.is(u"@Locale:")) { // raw locale string
		return true;
	} else if (r.is(u"%=")) {
		r += 2;
		auto tmp = r.readChars<WideStringView::CharGroup<CharGroupId::Numbers>>();
		if (!tmp.empty() && r.is('%')) {
			return true;
		}
	} else {
		constexpr size_t maxChars = config::MaxFastLocaleChars;
		WideStringView shortView(r.data(), sprt::min(r.size(), maxChars));
		shortView.skipUntil<WideStringView::Chars<'%'>>();
		if (shortView.is('%')) {
			++shortView;
			if (shortView.is('=')) {
				++shortView;
				shortView = WideStringView(shortView.data(),
						sprt::min(maxChars, r.size() - (shortView.data() - r.data())));
				shortView.skipChars<WideStringView::CharGroup<CharGroupId::Numbers>>();
				if (shortView.is('%')) {
					return true;
				}
			} else {
				shortView = WideStringView(shortView.data(),
						sprt::min(maxChars, r.size() - (shortView.data() - r.data())));
				shortView.skipChars< WideStringView::CharGroup<CharGroupId::Alphanumeric>,
						WideStringView::Chars<':', '.', '-', '_', '[', ']', '+', '=', '?'>>();
				if (shortView.is('%')) {
					return true;
				}
			}
		}
	}
	return false;
}

bool LocaleManager::hasLocaleTags(WideStringView r) {
	if (r.empty()) {
		return false;
	}

	if (r.is(u"@Locale:")) { // raw locale string
		return true;
	} else if (r.is(u"%=")) {
		r += 2;
		auto tmp = r.readChars<WideStringView::CharGroup<CharGroupId::Numbers>>();
		if (!tmp.empty() && r.is('%')) {
			return true;
		}
	} else {
		while (!r.empty()) {
			r.skipUntil<WideStringView::Chars<'%'>>();
			if (r.is('%')) {
				++r;
				if (r.is('=')) {
					++r;
					r.skipChars<WideStringView::CharGroup<CharGroupId::Numbers>>();
					if (r.is('%')) {
						return true;
					}
				} else {
					r.skipChars< WideStringView::CharGroup<CharGroupId::Alphanumeric>,
							WideStringView::Chars<':', '.', '-', '_', '[', ']', '+', '=', '?'>>();
					if (r.is('%')) {
						return true;
					}
				}
			}
		}
	}

	return false;
}

WideStringView LocaleManager::resolveTag(WideStringView token, SpanView<WideStringView> args) {
	WideStringView replacement;
	if (token.is('=')) {
		auto numToken = token;
		++numToken;
		if (numToken.is<WideStringView::CharGroup<CharGroupId::Numbers>>()) {
			numToken.readInteger().unwrap([&, this](int64_t id) {
				if (numToken.empty()) {
					replacement = string(size_t(id));
				}
			});
		}
	} else if (token.is('?')) {
		auto numToken = token;
		++numToken;
		auto numValue = numToken.readInteger();
		/* `>= 0`, not `> 0`: form 0 is the singular in three-form languages. */
		if (numValue.valid() && numValue.get() >= 0 && numToken.is(':')) {
			++numToken;
		/* `numToken`, not `token`: `numeric` looks up the key that follows the colon. */
			replacement = numeric(numToken, uint32_t(numValue.get()));
		}
	} else if (!args.empty()
			&& token.is<WideStringView::CharGroup<CharGroupId::Numbers>>()) {
		/* A positional argument, `%1%` .. `%9%`, one-based. Digits only, so no key is shadowed;
		without args the tag falls through to the table lookup. */
		auto numToken = token;
		auto numValue = numToken.readInteger();
		if (numValue.valid() && numToken.empty()) {
			auto idx = numValue.get();
			if (idx > 0 && size_t(idx) <= args.size()) {
				replacement = args[size_t(idx) - 1];
			}
		}
	} else {
		replacement = string(token);
	}
	return replacement;
}

WideString LocaleManager::resolveLocaleTags(WideStringView r, SpanView<WideStringView> args) {
	if (r.is(u"@Locale:")) { // raw locale string
		r += "@Locale:"_len;
		/* A whole-string key may carry tags of its own (so `%1%` works from table values); resolved
		only when there is something to resolve. */
		auto value = string(r);
		if (!args.empty() && hasLocaleTags(value)) {
			return resolveLocaleTags(value, args);
		}
		return value.str<mem_std::Interface>();
	} else {
		mem_std::WideString ret;
		ret.reserve(r.size());
		while (!r.empty()) {
			auto tmp = r.readUntil<WideStringView::Chars<'%'>>();
			ret.append(tmp.data(), tmp.size());
			if (r.is('%')) {
				++r;
				auto token = r.readChars< WideStringView::CharGroup<CharGroupId::Alphanumeric>,
						WideStringView::Chars<':', '.', '-', '_', '[', ']', '+', '=', '?'>>();
				if (!r.is('%')) {
					ret.push_back(u'%');
					ret.append(token.data(), token.size());
				} else {
					++r;
					auto replacement = resolveTag(token, args);
					if (replacement.empty()) {
						ret.push_back(u'%');
						ret.append(token.data(), token.size());
						ret.push_back(u'%');
					} else {
						ret.append(replacement.str<mem_std::Interface>());
					}
				}
			}
		}
		return ret;
	}
	return mem_std::WideString();
}

StringView LocaleManager::timeToken(TimeTokens tok) {
	StringView ret;
	// A blank table entry is a miss, not an answer, so partially filled tables fall back.
	LocaleManager_lookup(_timeTokens, _locale, _default, [&](auto &table) {
		if (table[toInt(tok)].empty()) {
			return false;
		}
		ret = table[toInt(tok)];
		return true;
	});
	return ret.empty() ? StringView(_defaultTime[toInt(tok)]) : ret;
}

const sprt::array<mem_pool::String, toInt(TimeTokens::Max)> &LocaleManager::timeTokenTable() {
	sprt::array<mem_pool::String, toInt(TimeTokens::Max)> *ret = nullptr;
	LocaleManager_lookup(_timeTokens, _locale, _default, [&](auto &table) {
		ret = &table;
		return true;
	});
	return ret ? *ret : _defaultTime;
}

LocaleManager *LocaleManager::s_sharedInstance = nullptr;

EventHeader onLocale("Locale::onLocale");

void define(const StringView &locale, LocaleInitList &&init) {
	LocaleManager::getInstance()->define(locale, sp::move(init));
}

void define(const StringView &locale, LocaleIndexList &&init) {
	LocaleManager::getInstance()->define(locale, sp::move(init));
}

void define(const StringView &locale, const sprt::array<StringView, toInt(TimeTokens::Max)> &arr) {
	LocaleManager::getInstance()->define(locale, arr);
}

WideStringView string(const WideStringView &str) {
	return LocaleManager::getInstance()->string(str);
}

WideStringView string(size_t idx) { return LocaleManager::getInstance()->string(idx); }

WideStringView numeric(const WideStringView &str, uint32_t num) {
	return LocaleManager::getInstance()->numeric(str, num);
}

void setDefault(StringView def) { LocaleManager::getInstance()->setDefault(def); }
StringView getDefault() { return LocaleManager::getInstance()->getDefault(); }
LocaleInfo getDefaultInfo() { return LocaleManager::getInstance()->getDefaultInfo(); }

void setLocale(StringView loc) { LocaleManager::getInstance()->setLocale(loc); }
StringView getLocale() { return LocaleManager::getInstance()->getLocale(); }
LocaleInfo getLocaleInfo() { return LocaleManager::getInstance()->getLocaleInfo(); }

bool hasLocaleTagsFast(const WideStringView &r) {
	return LocaleManager::getInstance()->hasLocaleTagsFast(r);
}

bool hasLocaleTags(const WideStringView &r) {
	return LocaleManager::getInstance()->hasLocaleTags(r);
}

WideString resolveLocaleTags(const WideStringView &r) {
	return LocaleManager::getInstance()->resolveLocaleTags(r, SpanView<WideStringView>());
}

WideString resolveLocaleTags(const WideStringView &r, SpanView<WideStringView> args) {
	return LocaleManager::getInstance()->resolveLocaleTags(r, args);
}

uint32_t pluralForm(uint32_t n) { return LocaleManager::getInstance()->pluralForm(n); }

/* The base direction of a locale's language: right-to-left for Arabic, Hebrew and Thaana scripts.
`LanguageInfo` has no direction field, hence the list. Never `Neutral`, which is a property of
the text, not the locale. */
TextDirection getTextDirection(StringView language) {
	if (language == "ar" || language == "fa" || language == "he" || language == "ur"
			|| language == "ps" || language == "sd" || language == "ug" || language == "yi"
			|| language == "dv" || language == "ckb") {
		return TextDirection::RightToLeft;
	}
	return TextDirection::LeftToRight;
}

TextDirection getTextDirection() {
	auto info = LocaleManager::getInstance()->getLocaleInfo();
	auto lang = info.id.language;
	if (lang.empty()) {
		lang = LocaleManager::getInstance()->getDefaultInfo().id.language;
	}
	return getTextDirection(lang);
}

String pluralFormat(StringView key, uint32_t count, SpanView<StringView> extra) {
	auto mgr = LocaleManager::getInstance();

	// The form first, then the substitution: `numeric` splits the definition on ':' and returns
	// one template, which carries `%1%`.
	auto form = mgr->numeric(string::toUtf16<mem_std::Interface>(key), mgr->pluralForm(count));
	if (form.empty()) {
		// Return the tag so a missing definition is visible rather than blank.
		return toString("%", key, "%");
	}

	auto countStr = toString(count);

	Vector<WideString> storage;
	Vector<WideStringView> args;
	storage.reserve(extra.size() + 1);
	args.reserve(extra.size() + 1);
	storage.emplace_back(string::toUtf16<mem_std::Interface>(countStr));
	args.emplace_back(storage.back());
	for (auto &it : extra) {
		storage.emplace_back(string::toUtf16<mem_std::Interface>(it));
		args.emplace_back(storage.back());
	}

	return string::toUtf8<mem_std::Interface>(mgr->resolveLocaleTags(form, args));
}

String format(StringView key, SpanView<StringView> args) {
	// Arguments are widened to UTF-16, which the resolver and the table use throughout.
	Vector<WideString> storage;
	Vector<WideStringView> views;
	storage.reserve(args.size());
	views.reserve(args.size());
	for (auto &it : args) {
		storage.emplace_back(string::toUtf16<mem_std::Interface>(it));
		views.emplace_back(storage.back());
	}

	auto tag = string::toUtf16<mem_std::Interface>(key);
	if (!WideStringView(tag).is(u"@Locale:")) {
		// A bare key is accepted as well as a tag.
		tag = string::toUtf16<mem_std::Interface>(toString("@Locale:", key));
	}

	auto out = string::toUtf8<mem_std::Interface>(
			LocaleManager::getInstance()->resolveLocaleTags(tag, views));
	if (out.empty()) {
		// A key nothing defines: return the tag so the miss is visible rather than blank.
		return toString("%", key, "%");
	}
	return out;
}

StringView timeToken(TimeTokens tok) { return LocaleManager::getInstance()->timeToken(tok); }

const sprt::array<mem_pool::String, toInt(TimeTokens::Max)> &timeTokenTable() {
	return LocaleManager::getInstance()->timeTokenTable();
}

static bool isToday(struct tm &tm, struct tm &now) {
	return tm.tm_year == now.tm_year && tm.tm_yday == now.tm_yday;
}

static uint32_t getNumDaysInYear(int y) {
	return ((y & 3) || (((y % 100) == 0) && (((y % 400) != 100)))) ? 355 : 356;
}

static uint32_t getYday(struct tm &now, int y) {
	auto ndays = getNumDaysInYear(y);
	return ((now.tm_year == y) ? 0 : ndays) + now.tm_yday;
}

static bool isYesterday(struct tm &tm, struct tm &now) {
	auto n1 = getYday(tm, now.tm_year);
	auto n2 = getYday(now, now.tm_year);

	return n1 + 1 == n2;
}

static void sp_localtime_r(time_t *sec_now, struct tm *tm_now) {
	__sprt_localtime_r(sec_now, tm_now);
}

template <typename T>
static String localDate_impl(const sprt::array<T, toInt(TimeTokens::Max)> &table, Time t) {
	auto sec_now = time_t(Time::now().toSeconds());
	struct tm tm_now;
	sp_localtime_r(&sec_now, &tm_now);

	auto sec_time = time_t(t.toSeconds());
	struct tm tm_time;
	sp_localtime_r(&sec_time, &tm_time);

	if (isToday(tm_time, tm_now)) {
		return String(table[toInt(TimeTokens::Today)].data(),
				table[toInt(TimeTokens::Today)].size());
	} else if (isYesterday(tm_time, tm_now)) {
		return String(table[toInt(TimeTokens::Yesterday)].data(),
				table[toInt(TimeTokens::Yesterday)].size());
	}
	if (tm_time.tm_year == tm_now.tm_year) {
		return toString(tm_time.tm_mday, " ", table[tm_time.tm_mon + 2]);
	} else {
		return toString(tm_time.tm_mday, " ", table[tm_time.tm_mon + 2], " ",
				1'900 + tm_time.tm_year);
	}
}

String localDate(Time t) {
	return localDate_impl(LocaleManager::getInstance()->timeTokenTable(), t);
}

String localDate(const sprt::array<StringView, toInt(TimeTokens::Max)> &table, Time t) {
	return localDate_impl(table, t);
}

} // namespace stappler::xenolith::locale
