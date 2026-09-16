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


#include "dndtree/DndTreeLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

struct DemoLocale {
	StringView id;
	StringView nativeName;
};

// `fa-ir` and not `fa`: LocaleIdentifier reads the subtag after the dash as a TERRITORY, so a bare
// language is not a valid id. Russian is in the rotation to show that a change of WORDS is not a
// change of direction - the mirroring follows the script.
static constexpr DemoLocale s_locales[] = {
	{StringView("en-us"), StringView("English")},
	{StringView("ru-ru"), StringView("Русский")},
	{StringView("fa-ir"), StringView("فارسی")},
};

static size_t s_current = 0;

} // namespace

void defineDndTreeLocales() {
	using locale::define;

	define("en-us",
			{
				pair("Dnd:Bar:ExpandAll", "Expand all"),
				pair("Dnd:Bar:CollapseAll", "Collapse all"),
				pair("Dnd:Bar:Reset", "Reset content"),
				pair("Dnd:Tree:Library", "Library"),
				pair("Dnd:Tree:Project", "Project"),
			});

	define("ru-ru",
			{
				pair("Dnd:Bar:ExpandAll", "Раскрыть всё"),
				pair("Dnd:Bar:CollapseAll", "Свернуть всё"),
				pair("Dnd:Bar:Reset", "Сбросить содержимое"),
				pair("Dnd:Tree:Library", "Библиотека"),
				pair("Dnd:Tree:Project", "Проект"),
			});

	define("fa-ir",
			{
				pair("Dnd:Bar:ExpandAll", "گشودن همه"),
				pair("Dnd:Bar:CollapseAll", "بستن همه"),
				pair("Dnd:Bar:Reset", "بازنشاندن محتوا"),
				pair("Dnd:Tree:Library", "کتابخانه"),
				pair("Dnd:Tree:Project", "پروژه"),
			});

	auto current = locale::getLocale();
	for (size_t i = 0; i < sizeof(s_locales) / sizeof(s_locales[0]); ++i) {
		if (s_locales[i].id == current) {
			s_current = i;
			break;
		}
	}
	locale::setLocale(s_locales[s_current].id);
}

StringView cycleDndTreeLocale() {
	s_current = (s_current + 1) % (sizeof(s_locales) / sizeof(s_locales[0]));

	// The whole switch: `setLocale` fires `onLocale`, every Label re-expands its tag and
	// ui::StyleSystem re-seeds its `rtl` media flag and invalidates the subtree.
	locale::setLocale(s_locales[s_current].id);
	return s_locales[s_current].nativeName;
}

StringView currentDndTreeLocaleName() { return s_locales[s_current].nativeName; }

} // namespace stappler::xenolith::examples
