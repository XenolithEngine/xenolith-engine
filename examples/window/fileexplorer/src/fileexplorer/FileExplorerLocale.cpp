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
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "fileexplorer/FileExplorerLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

struct DemoLocale {
	StringView id;
	StringView nativeName;
};

// A bare language is not a valid LocaleIdentifier - the subtag after the dash is read as a
// TERRITORY - so both entries carry one.
static constexpr DemoLocale s_locales[] = {
	{StringView("en-us"), StringView("English")},
	{StringView("ru-ru"), StringView("Русский")},
};

static size_t s_current = 0;

} // namespace

void defineFileExplorerLocales() {
	using locale::define;

	define("en-us",
			{
				pair("FE:Places:Title", "Places"),
				pair("FE:Places:Home", "Home"),
				pair("FE:Places:Desktop", "Desktop"),
				pair("FE:Places:Documents", "Documents"),
				pair("FE:Places:Downloads", "Downloads"),
				pair("FE:Places:Music", "Music"),
				pair("FE:Places:Pictures", "Pictures"),
				pair("FE:Places:Videos", "Videos"),
				pair("FE:Places:Filesystem", "Filesystem"),
				pair("FE:Browser:Title", "Files"),
				pair("FE:Bar:Icons", "Icons"),
				pair("FE:Bar:Table", "Table"),
				pair("FE:Bar:TileSize", "Icon size"),
				pair("FE:Bar:FontSize", "Text size"),
				pair("FE:Bar:Hidden", "Hidden files"),
				pair("FE:Bar:Refresh", "Refresh"),
				pair("FE:Col:Name", "Name"),
				pair("FE:Col:Size", "Size"),
				pair("FE:Col:Modified", "Modified"),
				pair("FE:Status:Empty", "empty directory"),
				pair("FE:Status:Loading", "reading…"),
			});

	define("ru-ru",
			{
				pair("FE:Places:Title", "Места"),
				pair("FE:Places:Home", "Домашний каталог"),
				pair("FE:Places:Desktop", "Рабочий стол"),
				pair("FE:Places:Documents", "Документы"),
				pair("FE:Places:Downloads", "Загрузки"),
				pair("FE:Places:Music", "Музыка"),
				pair("FE:Places:Pictures", "Изображения"),
				pair("FE:Places:Videos", "Видео"),
				pair("FE:Places:Filesystem", "Файловая система"),
				pair("FE:Browser:Title", "Файлы"),
				pair("FE:Bar:Icons", "Значки"),
				pair("FE:Bar:Table", "Таблица"),
				pair("FE:Bar:TileSize", "Размер значков"),
				pair("FE:Bar:FontSize", "Размер текста"),
				pair("FE:Bar:Hidden", "Скрытые файлы"),
				pair("FE:Bar:Refresh", "Обновить"),
				pair("FE:Col:Name", "Имя"),
				pair("FE:Col:Size", "Размер"),
				pair("FE:Col:Modified", "Изменён"),
				pair("FE:Status:Empty", "пустой каталог"),
				pair("FE:Status:Loading", "чтение…"),
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

StringView cycleFileExplorerLocale() {
	s_current = (s_current + 1) % (sizeof(s_locales) / sizeof(s_locales[0]));

	// `setLocale` fires `onLocale`, and every Label re-expands its tag by itself.
	locale::setLocale(s_locales[s_current].id);
	return s_locales[s_current].nativeName;
}

StringView currentFileExplorerLocaleName() { return s_locales[s_current].nativeName; }

} // namespace stappler::xenolith::examples
