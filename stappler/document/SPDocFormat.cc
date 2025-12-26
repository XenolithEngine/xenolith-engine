/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#include "SPDocFormat.h"
#include "SPDocument.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct FormatStorageLess {
	bool operator()(Format *l, Format *r) const {
		if (l->priority == r->priority) {
			return l < r;
		} else {
			return l->priority > r->priority;
		}
	}
};

class FormatStorage {
public:
	static std::mutex &getMutex();
	static FormatStorage *getInstance();

	void emplace(Format *);
	void erase(Format *);

	std::set<Format *, FormatStorageLess> get();

private:
	std::set<Format *, FormatStorageLess> formatList;
};

std::mutex &FormatStorage::getMutex() {
	static std::mutex s_formatListMutex;
	return s_formatListMutex;
}

FormatStorage *FormatStorage::getInstance() {
	static FormatStorage *s_sharedInstance = nullptr;
	std::unique_lock mutex(getMutex());
	if (!s_sharedInstance) {
		s_sharedInstance = new FormatStorage();
	}
	return s_sharedInstance;
}

void FormatStorage::emplace(Format *ptr) {
	std::unique_lock mutex(getMutex());
	formatList.emplace(ptr);
}

void FormatStorage::erase(Format *ptr) {
	std::unique_lock mutex(getMutex());
	formatList.erase(ptr);
}

std::set<Format *, FormatStorageLess> FormatStorage::get() {
	std::set<Format *, FormatStorageLess> ret;

	std::unique_lock mutex(getMutex());
	ret = formatList;

	return ret;
}

bool Format::canOpenDocumnt(memory::pool_t *p, FileInfo path, StringView ct) {
	auto formatList = FormatStorage::getInstance()->get();

	for (auto &it : formatList) {
		if (it->check_file && it->check_file(p, path, ct)) {
			return true;
		}
	}

	return false;
}

bool Format::canOpenDocumnt(memory::pool_t *p, BytesView data, StringView ct) {
	auto formatList = FormatStorage::getInstance()->get();

	for (auto &it : formatList) {
		if (it->check_data && it->check_data(p, data, ct)) {
			return true;
		}
	}

	return false;
}

Rc<Document> Format::openDocument(memory::pool_t *p, FileInfo path, StringView ct) {
	Rc<Document> ret;
	auto formatList = FormatStorage::getInstance()->get();

	for (auto &it : formatList) {
		if (it->check_file && it->check_file(p, path, ct)) {
			ret = it->load_file(p, path, ct);
			break;
		}
	}
	return ret;
}

Rc<Document> Format::openDocument(memory::pool_t *p, BytesView data, StringView ct) {
	Rc<Document> ret;
	auto formatList = FormatStorage::getInstance()->get();

	for (auto &it : formatList) {
		if (it->check_data && it->check_data(p, data, ct)) {
			ret = it->load_data(p, data, ct);
			break;
		}
	}
	return ret;
}

Format::Format(check_file_fn chFileFn, load_file_fn ldFileFn, check_data_fn chDataFn,
		load_data_fn ldDataFn, size_t p)
: check_data(chDataFn)
, check_file(chFileFn)
, load_data(ldDataFn)
, load_file(ldFileFn)
, priority(p) {
	FormatStorage::getInstance()->emplace(this);
}

Format::~Format() { FormatStorage::getInstance()->erase(this); }

} // namespace stappler::document
