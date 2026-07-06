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

#include "XLSimpleStyleSheet.h"
#include "SPDocument.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

/* Subclass gives access to the protected rule map `_styles` and adds identity-based
matching plus an inline-style cache (all pool-backed, living in the sheet's pool). */
struct StyleSheet::Container : document::StyleContainer {
	using PoolString = memory::PoolInterface::StringType;

	template <typename K, typename V>
	using PoolMap = memory::PoolInterface::MapType<K, V>;

	Container(document::DocumentData *data) : StyleContainer(data) { }

	// mirrors document::StyleContainer::resolveNodeStyle match/merge order
	// (including the `*`-merges-inheritable-only quirk)
	void resolve(document::StyleList &target, StringView type, StringView id,
			SpanView<memory::StandartInterface::StringType> classes,
			SpanView<bool> mediaResolved) const {
		PoolString key;

		auto it = _styles.find(StringView("*"));
		if (it != _styles.end()) {
			target.merge(it->second, mediaResolved, true);
		}

		if (!type.empty()) {
			it = _styles.find(type);
			if (it != _styles.end()) {
				target.merge(it->second, mediaResolved);
			}
		}

		for (auto &cl : classes) {
			key.clear();
			key.append(1, '.').append(cl.data(), cl.size());
			it = _styles.find(StringView(key));
			if (it != _styles.end()) {
				target.merge(it->second, mediaResolved);
			}

			if (!type.empty()) {
				key.clear();
				key.append(type.data(), type.size()).append(1, '.').append(cl.data(), cl.size());
				it = _styles.find(StringView(key));
				if (it != _styles.end()) {
					target.merge(it->second, mediaResolved);
				}
			}
		}

		if (!id.empty()) {
			key.clear();
			key.append(1, '#').append(id.data(), id.size());
			it = _styles.find(StringView(key));
			if (it != _styles.end()) {
				target.merge(it->second, mediaResolved);
			}

			if (!type.empty()) {
				key.clear();
				key.append(type.data(), type.size()).append(1, '#').append(id.data(), id.size());
				it = _styles.find(StringView(key));
				if (it != _styles.end()) {
					target.merge(it->second, mediaResolved);
				}
			}
		}
	}

	// parse-once cache for inline `style="..."` declaration lists
	const document::StyleList *getInlineStyle(StringView css) {
		auto it = _inlineStyles.find(css);
		if (it != _inlineStyles.end()) {
			return it->second;
		}

		auto style = new (sprt::nothrow) document::StyleList();
		StringReader r(css.data(), css.size());
		readStyle(*style, r);
		_inlineStyles.emplace(PoolString(css.data(), css.size()), style);
		return style;
	}

	PoolMap<PoolString, document::StyleList *> _inlineStyles;
};

StyleSheet::~StyleSheet() {
	if (_pool) {
		// _data and _container are pool-allocated, destroyed with the pool
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

bool StyleSheet::init() {
	_pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	memory::perform([&] {
		_data = new (_pool) document::DocumentData(_pool);
		_container = new (_pool) Container(_data);
	}, _pool);
	return _data != nullptr && _container != nullptr;
}

bool StyleSheet::init(StringView css) {
	if (!init()) {
		return false;
	}
	if (!addStyle(css)) {
		return false;
	}
	return true;
}

bool StyleSheet::init(const FileInfo &file) {
	if (!init()) {
		return false;
	}
	if (!addStyle(file)) {
		return false;
	}
	return true;
}

bool StyleSheet::addStyle(StringView css) {
	bool ret = false;
	memory::perform([&] {
		Container::StringReader r(css.data(), css.size());
		ret = _container->readStyle(r);
	}, _pool);
	if (ret) {
		++_version;
	}
	return ret;
}

bool StyleSheet::addStyle(const FileInfo &file) {
	bool ret = false;
	memory::perform([&] { ret = _container->readStyle(file); }, _pool);
	if (ret) {
		++_version;
	}
	return ret;
}

void StyleSheet::resolveForIdentity(document::StyleList &target, StringView type, StringView id,
		SpanView<String> classes, SpanView<bool> mediaResolved) const {
	_container->resolve(target, type, id, classes, mediaResolved);
}

const document::StyleList *StyleSheet::getInlineStyle(StringView css) {
	if (css.empty()) {
		return nullptr;
	}

	const document::StyleList *ret = nullptr;
	memory::perform([&] { ret = _container->getInlineStyle(css); }, _pool);
	return ret;
}

Vector<bool> StyleSheet::resolveMedia(const document::MediaParameters &media) const {
	return media.resolveMediaQueries<memory::StandartInterface>(_data->queries);
}

SpanView<StringView> StyleSheet::getStrings() const { return _data->strings; }

} // namespace stappler::xenolith::simpleui
