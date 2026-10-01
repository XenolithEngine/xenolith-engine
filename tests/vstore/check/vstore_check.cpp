/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Shared check utilities. Everything here is deliberately generic: helpers that need
// arena internals (save/verify/describe) live on the Arena itself, both because they need the
// internals and because they are useful as production diagnostics, not only in tests.

#include "SPCommon.h"

#include "vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler::test {

void fillPattern(uint8_t *dst, uint32_t size, uint32_t addr) {
	for (uint32_t i = 0; i < size; ++i) { dst[i] = patternByte(addr, i); }
}

bool checkPattern(const uint8_t *src, uint32_t size, uint32_t addr, uint32_t *firstBad) {
	for (uint32_t i = 0; i < size; ++i) {
		if (src[i] != patternByte(addr, i)) {
			if (firstBad) {
				*firstBad = i;
			}
			return false;
		}
	}
	return true;
}

bool compareBytes(BytesView got, BytesView expect, StringView label, uint32_t maxReported) {
	if (got.size() != expect.size()) {
		sprt::cout << "       " << label << ": size " << got.size() << " != " << expect.size()
				   << "\n";
		return false;
	}

	uint32_t reported = 0;
	size_t total = 0;
	for (size_t i = 0; i < got.size(); ++i) {
		if (got[i] != expect[i]) {
			++total;
			if (reported < maxReported) {
				sprt::cout << "       " << label << ": byte " << i << " is " << int(got[i])
						   << ", expected " << int(expect[i]) << "\n";
				++reported;
			}
		}
	}

	if (total > reported) {
		sprt::cout << "       " << label << ": " << (total - reported) << " more differing bytes\n";
	}
	return total == 0;
}

namespace {

// Recursive so the report names the path to the first difference rather than just "not equal" -
// a whole-arena dump is far too large to diff by eye.
bool compareValuesImpl(const mem_std::Value &got, const mem_std::Value &expect, StringView label,
		StringView path) {
	if (got.getType() != expect.getType()) {
		sprt::cout << "       " << label << ": at " << (path.empty() ? StringView("<root>") : path)
				   << " type " << int(got.getType()) << " != " << int(expect.getType()) << "\n";
		return false;
	}

	if (expect.isDictionary()) {
		auto &gd = got.getDict();
		auto &ed = expect.getDict();
		for (auto &it : ed) {
			auto git = gd.find(it.first);
			if (git == gd.end()) {
				sprt::cout << "       " << label << ": at " << path << "/" << it.first
						   << " key is missing\n";
				return false;
			}
			if (!compareValuesImpl(git->second, it.second, label,
						mem_std::toString(path, "/", it.first))) {
				return false;
			}
		}
		for (auto &it : gd) {
			if (ed.find(it.first) == ed.end()) {
				sprt::cout << "       " << label << ": at " << path << "/" << it.first
						   << " unexpected key\n";
				return false;
			}
		}
		return true;
	}

	if (expect.isArray()) {
		auto &ga = got.getArray();
		auto &ea = expect.getArray();
		if (ga.size() != ea.size()) {
			sprt::cout << "       " << label << ": at " << path << " size " << ga.size()
					   << " != " << ea.size() << "\n";
			return false;
		}
		for (size_t i = 0; i < ea.size(); ++i) {
			if (!compareValuesImpl(ga[i], ea[i], label, mem_std::toString(path, "/", i))) {
				return false;
			}
		}
		return true;
	}

	if (!(got == expect)) {
		sprt::cout << "       " << label << ": at " << (path.empty() ? StringView("<root>") : path)
				   << " value " << data::toString(got, false)
				   << " != " << data::toString(expect, false) << "\n";
		return false;
	}
	return true;
}

} // namespace

bool compareValues(const mem_std::Value &got, const mem_std::Value &expect, StringView label) {
	return compareValuesImpl(got, expect, label, StringView());
}

} // namespace stappler::test
