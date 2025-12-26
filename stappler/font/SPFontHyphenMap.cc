/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPFontHyphenMap.h"
#include "thirdparty/hyphen/hyphen.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::font {

HyphenMap::~HyphenMap() {
	for (auto &it : _dicts) {
		hnj_hyphen_free(it.second);
	}
}
bool HyphenMap::init() {
	return true;
}

void HyphenMap::addHyphenDict(CharGroupId id, const FileInfo &file) {
	auto data = filesystem::readTextFile<Interface>(file);
	if (!data.empty()) {
		auto dict = hnj_hyphen_load_data(data.data(), data.size());
		if (dict) {
			auto it = _dicts.find(id);
			if (it == _dicts.end()) {
				_dicts.emplace(id, dict);
			} else {
				hnj_hyphen_free(it->second);
				it->second = dict;
			}
		}
	}
}

void HyphenMap::addHyphenDict(CharGroupId id, BytesView data) {
	if (!data.empty()) {
		auto dict = hnj_hyphen_load_data((const char *)data.data(), data.size());
		if (dict) {
			auto it = _dicts.find(id);
			if (it == _dicts.end()) {
				_dicts.emplace(id, dict);
			} else {
				hnj_hyphen_free(it->second);
				it->second = dict;
			}
		}
	}
}

auto HyphenMap::makeWordHyphens(const char16_t *ptr, size_t len) -> Vector<uint8_t> {
	if (len < 4 || len >= 255) {
		return Vector<uint8_t>();
	}

	HyphenDict *dict = nullptr;
	for (auto &it : _dicts) {
		if (inCharGroup(it.first, ptr[0])) {
			dict = it.second;
			break;
		}
	}

	if (!dict) {
		return Vector<uint8_t>();
	}

	String word = convertWord(dict, ptr, len);
	if (!word.empty()) {
		Vector<char> buf; buf.resize(word.size() + 5);

		char ** rep = nullptr;
		int * pos = nullptr;
		int * cut = nullptr;
		hnj_hyphen_hyphenate2(dict, word.data(), int(word.size()), buf.data(), nullptr, &rep, &pos, &cut);

		Vector<uint8_t> ret;
		uint8_t i = 0;
		for (auto &it : buf) {
			if (it > 0) {
				if ((it - '0') % 2 == 1) {
					ret.push_back(i + 1);
				}
			} else {
				break;
			}
			++ i;
		}
		return ret;
	}
	return Vector<uint8_t>();
}

auto HyphenMap::makeWordHyphens(const WideStringView &r) -> Vector<uint8_t> {
	return makeWordHyphens(r.data(), r.size());
}

void HyphenMap::purgeHyphenDicts() {
	for (auto &it : _dicts) {
		hnj_hyphen_free(it.second);
	}
}

auto HyphenMap::convertWord(HyphenDict *dict, const char16_t *ptr, size_t len) -> String {
	if (dict->utf8) {
		return string::toUtf8<Interface>(WideStringView(ptr, len));
	} else {
		if (strcmp("KOI8-R", dict->cset) == 0) {
			return string::toKoi8r<Interface>(WideStringView(ptr, len));
		}

		return String();
	}
}

}
