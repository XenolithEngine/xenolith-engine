/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/


#include "form/FormLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* THE ROTATION, and Persian is in it because a demo of direction needs a direction to change to.

`fa-ir` and not `fa`: LocaleIdentifier reads the subtag after the dash as a TERRITORY, so a bare
language is not a valid id. Russian is here to make the point that a THIRD language changes the
words without changing the direction - the mirroring follows the script, not the switch. */
struct DemoLocale {
	StringView id;
	StringView nativeName;
};

static constexpr DemoLocale s_locales[] = {
	{StringView("en-us"), StringView("English")},
	{StringView("ru-ru"), StringView("Русский")},
	{StringView("fa-ir"), StringView("فارسی")},
};

static size_t s_current = 0;

} // namespace

void defineFormLocales() {
	using locale::define;

	/* Every table is defined in the SAME order and with the same keys, which is what makes a
	missing translation a compile-time-visible hole rather than an empty label at run time. The
	lookup chain falls back to `en-us` per key, so en-us is the one table that may not have one. */
	define("en-us",
			{
				pair("Form:Bar:SubmitLeft", "Submit left"),
				pair("Form:Bar:SubmitRight", "Submit right"),
				pair("Form:Bar:ResetBoth", "Reset both"),
				pair("Form:Bar:ExpandAll", "Expand all"),
				pair("Form:Bar:CollapseAll", "Collapse all"),
				pair("Form:Bar:SelfCheck", "Self-check"),
				pair("Form:Column:Plain", "Plain widgets — one form"),
				pair("Form:Column:Accordion", "Accordion — a second, independent form"),
				pair("Form:Name", "Name"),
				pair("Form:Email", "Email"),
				pair("Form:Password", "Password"),
				pair("Form:Notes", "Notes (transient)"),
				pair("Form:Count", "Count (int)"),
				pair("Form:Ratio", "Ratio (real)"),
				pair("Form:Volume", "Volume"),
				pair("Form:Offset", "Offset (vec3)"),
				pair("Form:Subscribe", "Subscribe"),
				pair("Form:Role", "Role"),
				pair("Form:Country", "Country (search)"),
				pair("Form:Tags", "Tags"),
				pair("Form:Accent", "Accent (built-in)"),
				pair("Form:Overlay", "Overlay (auto + alpha)"),
				pair("Form:TextView", "TextView"),
				pair("Form:CodeEditor", "CodeEditor"),
			});

	define("ru-ru",
			{
				pair("Form:Bar:SubmitLeft", "Отправить левую"),
				pair("Form:Bar:SubmitRight", "Отправить правую"),
				pair("Form:Bar:ResetBoth", "Сбросить обе"),
				pair("Form:Bar:ExpandAll", "Раскрыть всё"),
				pair("Form:Bar:CollapseAll", "Свернуть всё"),
				pair("Form:Bar:SelfCheck", "Самопроверка"),
				pair("Form:Column:Plain", "Обычные виджеты — одна форма"),
				pair("Form:Column:Accordion", "Аккордеон — вторая, независимая форма"),
				pair("Form:Name", "Имя"),
				pair("Form:Email", "Почта"),
				pair("Form:Password", "Пароль"),
				pair("Form:Notes", "Заметки (без сохранения)"),
				pair("Form:Count", "Количество (целое)"),
				pair("Form:Ratio", "Доля (дробное)"),
				pair("Form:Volume", "Громкость"),
				pair("Form:Offset", "Смещение (vec3)"),
				pair("Form:Subscribe", "Подписаться"),
				pair("Form:Role", "Роль"),
				pair("Form:Country", "Страна (поиск)"),
				pair("Form:Tags", "Метки"),
				pair("Form:Accent", "Акцент (встроенный)"),
				pair("Form:Overlay", "Наложение (авто + альфа)"),
				pair("Form:TextView", "Просмотр текста"),
				pair("Form:CodeEditor", "Редактор кода"),
			});

	define("fa-ir",
			{
				pair("Form:Bar:SubmitLeft", "فرستادن چپ"),
				pair("Form:Bar:SubmitRight", "فرستادن راست"),
				pair("Form:Bar:ResetBoth", "بازنشاندن هر دو"),
				pair("Form:Bar:ExpandAll", "گشودن همه"),
				pair("Form:Bar:CollapseAll", "بستن همه"),
				pair("Form:Bar:SelfCheck", "خودآزمایی"),
				pair("Form:Column:Plain", "ابزارک‌های ساده — یک فرم"),
				pair("Form:Column:Accordion", "آکاردئون — فرمی جدا و دوم"),
				pair("Form:Name", "نام"),
				pair("Form:Email", "رایانامه"),
				pair("Form:Password", "گذرواژه"),
				pair("Form:Notes", "یادداشت‌ها (ناپایدار)"),
				pair("Form:Count", "شمار (درست)"),
				pair("Form:Ratio", "نسبت (اعشاری)"),
				pair("Form:Volume", "بلندی صدا"),
				pair("Form:Offset", "جابه‌جایی (vec3)"),
				pair("Form:Subscribe", "اشتراک"),
				pair("Form:Role", "نقش"),
				pair("Form:Country", "کشور (جست‌وجو)"),
				pair("Form:Tags", "برچسب‌ها"),
				pair("Form:Accent", "رنگ تأکید (درون‌ساخت)"),
				pair("Form:Overlay", "پوشش (خودکار + آلفا)"),
				pair("Form:TextView", "نمای متن"),
				pair("Form:CodeEditor", "ویرایشگر کد"),
			});

	// Start where the machine is, if the machine is somewhere this demo knows.
	auto current = locale::getLocale();
	for (size_t i = 0; i < sizeof(s_locales) / sizeof(s_locales[0]); ++i) {
		if (s_locales[i].id == current) {
			s_current = i;
			break;
		}
	}
	locale::setLocale(s_locales[s_current].id);
}

StringView cycleFormLocale() {
	s_current = (s_current + 1) % (sizeof(s_locales) / sizeof(s_locales[0]));

	/* THE WHOLE SWITCH. `setLocale` fires `locale::onLocale`; every Label in the process marks
	itself dirty and re-expands its tag, and ui::StyleSystem re-seeds its `rtl` media flag and
	invalidates the subtree - which re-resolves `direction` and re-runs the layout. Nothing in this
	demo listens for anything. */
	locale::setLocale(s_locales[s_current].id);
	return s_locales[s_current].nativeName;
}

StringView currentFormLocaleName() { return s_locales[s_current].nativeName; }

} // namespace stappler::xenolith::examples
