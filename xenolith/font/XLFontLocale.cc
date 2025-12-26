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
#include "SPBytesReader.h"
#include "SPMemInterface.h"
#include "SPRef.h"
#include "SPString.h"
#include "SPStringDetail.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::locale {

class LocaleManager : public memory::PoolObject {
public:
	using StringMap = memory::dict<memory::u16string, memory::u16string>;
	using LocaleMap = memory::map<memory::string, StringMap>;

	using StringIndexMap = memory::dict<size_t, memory::u16string>;
	using LocaleIndexMap = memory::map<memory::string, StringIndexMap>;

	using Interface = memory::PoolInterface;

	static LocaleManager *s_sharedInstance;

	static LocaleManager *getInstance();

	LocaleManager(Ref *ref, memory::pool_t *p);

	bool init();

	void define(const StringView &locale, LocaleInitList &&init);
	void define(const StringView &locale, LocaleIndexList &&init);
	void define(const StringView &locale,
			const std::array<StringView, toInt(TimeTokens::Max)> &arr);
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

	WideStringView resolveTag(WideStringView);

	mem_std::WideString resolveLocaleTags(WideStringView r);
	StringView language(const StringView &locale);

	StringView timeToken(TimeTokens tok);
	const std::array<memory::string, toInt(TimeTokens::Max)> &timeTokenTable();

protected:
	LocaleIdentifier _default;
	LocaleIdentifier _locale;

	LocaleMap _strings;
	LocaleIndexMap _indexes;
	memory::map<memory::string, std::array<memory::string, toInt(TimeTokens::Max)>> _timeTokens;
	std::array<memory::string, toInt(TimeTokens::Max)> _defaultTime;

	memory::pool_t *_pool = nullptr;
};

struct LocaleInterface {
	static void initialize(void *ptr) { reinterpret_cast<LocaleInterface *>(ptr)->init(); }
	static void terminate(void *ptr) { reinterpret_cast<LocaleInterface *>(ptr)->term(); }

	LocaleInterface() { addInitializer(this, initialize, terminate); }

	void init() {
		manager = Rc<SharedRef<LocaleManager>>::create(SharedRefMode::Allocator);
		manager->setDefault(stappler::platform::getOsLocale());
		manager->setLocale(stappler::platform::getOsLocale());
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
		const std::array<StringView, toInt(TimeTokens::Max)> &arr) {
	memory::context ctx(_pool);
	auto it = _timeTokens.find(locale);
	if (it == _timeTokens.end()) {
		it = _timeTokens
					 .emplace(locale.str<Interface>(),
							 std::array<memory::string, toInt(TimeTokens::Max)>())
					 .first;
	}

	size_t i = 0;
	for (auto &arr_it : arr) {
		it->second[i] = arr_it.str<Interface>();
		++i;
	}
}

WideStringView LocaleManager::string(const WideStringView &str) {
	auto it = _strings.find(_locale.id);
	if (it == _strings.end()) {
		it = _strings.find(_default.id);
	}
	if (it == _strings.end()) {
		it = _strings.begin();
	}

	if (it != _strings.end()) {
		auto sit = it->second.find(str);
		if (sit != it->second.end()) {
			return sit->second;
		}
	}

	return WideStringView();
}

WideStringView LocaleManager::string(size_t index) {
	auto it = _indexes.find(_locale.id);
	if (it == _indexes.end()) {
		it = _indexes.find(_default.id);
	}
	if (it == _indexes.end()) {
		it = _indexes.begin();
	}

	if (it != _indexes.end()) {
		auto sit = it->second.find(index);
		if (sit != it->second.end()) {
			return sit->second;
		}
	}

	return WideStringView();
}

WideStringView LocaleManager::numeric(const WideStringView &str, uint32_t numEq) {
	auto fmt = string(str);
	WideStringView r(fmt);
	while (!r.empty()) {
		WideStringView def = r.readUntil<WideStringView::Chars<':'>>();
		if (r.is(':')) {
			++r;
		}

		if (numEq == 0) {
			return def;
		} else {
			--numEq;
		}
	}

	return WideStringView();
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
		WideStringView shortView(r.data(), std::min(r.size(), maxChars));
		shortView.skipUntil<WideStringView::Chars<'%'>>();
		if (shortView.is('%')) {
			++shortView;
			if (shortView.is('=')) {
				++shortView;
				shortView = WideStringView(shortView.data(),
						std::min(maxChars, r.size() - (shortView.data() - r.data())));
				shortView.skipChars<WideStringView::CharGroup<CharGroupId::Numbers>>();
				if (shortView.is('%')) {
					return true;
				}
			} else {
				shortView = WideStringView(shortView.data(),
						std::min(maxChars, r.size() - (shortView.data() - r.data())));
				shortView.skipChars< WideStringView::CharGroup<CharGroupId::Alphanumeric>,
						WideStringView::Chars<':', '.', '-', '_', '[', ']', '+', '='>>();
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
							WideStringView::Chars<':', '.', '-', '_', '[', ']', '+', '='>>();
					if (r.is('%')) {
						return true;
					}
				}
			}
		}
	}

	return false;
}

WideStringView LocaleManager::resolveTag(WideStringView token) {
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
	} else if (token.is(u"?")) {
		auto numToken = token;
		++numToken;
		auto numValue = numToken.readInteger();
		if (numValue.valid() && numValue.get() > 0) {
			auto index = numValue.get();
			if (numToken.is(':')) {
				++numToken;
				replacement = numeric(token, uint32_t(index));
			}
		}
	} else {
		if (replacement.empty() && !token.is('=')) {
			replacement = string(token);
		}
	}
	return replacement;
}

WideString LocaleManager::resolveLocaleTags(WideStringView r) {
	if (r.is(u"@Locale:")) { // raw locale string
		r += "@Locale:"_len;
		return string(r).str<memory::StandartInterface>();
	} else {
		mem_std::WideString ret;
		ret.reserve(r.size());
		while (!r.empty()) {
			auto tmp = r.readUntil<WideStringView::Chars<'%'>>();
			ret.append(tmp.data(), tmp.size());
			if (r.is('%')) {
				++r;
				auto token = r.readChars< WideStringView::CharGroup<CharGroupId::Alphanumeric>,
						WideStringView::Chars<':', '.', '-', '_', '[', ']', '+', '='>>();
				if (!r.is('%')) {
					ret.push_back(u'%');
					ret.append(token.data(), token.size());
				} else {
					++r;
					auto replacement = resolveTag(token);
					if (replacement.empty()) {
						ret.push_back(u'%');
						ret.append(token.data(), token.size());
						ret.push_back(u'%');
					} else {
						ret.append(replacement.str<memory::StandartInterface>());
					}
				}
			}
		}
		return ret;
	}
	return mem_std::WideString();
}

StringView LocaleManager::language(const StringView &locale) {
	if (locale == "ru-ru") {
		return "Русский";
	} else if (locale.starts_with("en-")) {
		return "English";
	}
	return StringView();
}

StringView LocaleManager::timeToken(TimeTokens tok) {
	auto it = _timeTokens.find(_locale.id);
	if (it == _timeTokens.end()) {
		it = _timeTokens.find(_default.id);
	}

	auto &table = it == _timeTokens.end() ? _defaultTime : it->second;
	return table[toInt(tok)];
}

const std::array<memory::string, toInt(TimeTokens::Max)> &LocaleManager::timeTokenTable() {
	auto it = _timeTokens.find(_locale.id);
	if (it == _timeTokens.end()) {
		it = _timeTokens.find(_default.id);
	}

	return it == _timeTokens.end() ? _defaultTime : it->second;
}

LocaleManager *LocaleManager::s_sharedInstance = nullptr;

EventHeader onLocale("Locale::onLocale");

void define(const StringView &locale, LocaleInitList &&init) {
	LocaleManager::getInstance()->define(locale, sp::move(init));
}

void define(const StringView &locale, LocaleIndexList &&init) {
	LocaleManager::getInstance()->define(locale, sp::move(init));
}

void define(const StringView &locale, const std::array<StringView, toInt(TimeTokens::Max)> &arr) {
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
	return LocaleManager::getInstance()->resolveLocaleTags(r);
}

StringView language(const StringView &locale) {
	return LocaleManager::getInstance()->language(locale);
}

StringView timeToken(TimeTokens tok) { return LocaleManager::getInstance()->timeToken(tok); }

const std::array<memory::string, toInt(TimeTokens::Max)> &timeTokenTable() {
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
#if WIN32
	localtime_s(tm_now, sec_now);
#else
	localtime_r(sec_now, tm_now);
#endif
}

template <typename T>
static String localDate_impl(const std::array<T, toInt(TimeTokens::Max)> &table, Time t) {
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

String localDate(const std::array<StringView, toInt(TimeTokens::Max)> &table, Time t) {
	return localDate_impl(table, t);
}

} // namespace stappler::xenolith::locale
