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

#include "XLUiStyleSheet.h"
#include "XLNode.h"
#include "XLUiInteractiveComponent.h"
#include "SPDocument.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Adapts the scene-graph `Node` tree to StyleContainer::matchComplex. Ancestors walk via
`getParent()`; the preceding sibling is the entry before the node in its parent's child list
(insertion / z-order). A compound matches a node's `NodeIdentity` (type/id/classes) and its
interactive pseudo-state (`:hover` etc.) read live from its `InteractiveComponent`; a node
without an identity component matches only the universal `*`. */
struct SceneNodeAccess {
	bool valid(Node *n) const { return n != nullptr; }

	Node *parent(Node *n) const { return n->getParent(); }

	Node *prevSibling(Node *n) const {
		auto p = n->getParent();
		if (!p) {
			return nullptr;
		}
		auto children = p->getChildren();
		for (size_t i = 0; i < children.size(); ++i) {
			if (children[i].get() == n) {
				return (i == 0) ? nullptr : children[i - 1].get();
			}
		}
		return nullptr;
	}

	bool matchCompound(Node *n, const document::StyleContainer::CompoundSelector &c) const {
		auto identity = n ? n->getComponent<NodeIdentity>() : nullptr;
		if (!c.universal && !c.tag.empty()) {
			if (!identity || StringView(identity->type) != StringView(c.tag.data(), c.tag.size())) {
				return false;
			}
		}
		if (!c.id.empty()) {
			if (!identity || StringView(identity->name) != StringView(c.id.data(), c.id.size())) {
				return false;
			}
		}
		for (auto &cl : c.classes) {
			if (!identity
					|| identity->classes.find(StringView(cl.data(), cl.size()))
							== identity->classes.end()) {
				return false;
			}
		}
		// interactive pseudo-classes (:hover/:focus/:active/:enabled/:disabled/:checked) read
		// the node's live InteractiveComponent state (its InteractiveFlags bits)
		if (c.pseudoRequire != 0 || c.pseudoForbid != 0) {
			uint32_t state = 0;
			if (auto ic = n ? n->getComponent<InteractiveComponent>() : nullptr) {
				state = uint32_t(ic->state);
			}
			if (!c.matchesPseudo(state)) {
				return false;
			}
		}
		return true;
	}
};

/* Subclass gives access to the protected rule map `_styles` and adds identity-based
matching plus an inline-style cache (all pool-backed, living in the sheet's pool). */
struct StyleSheet::Container : document::StyleContainer {
	using PoolString = memory::PoolInterface::StringType;

	template <typename K, typename V>
	using PoolMap = memory::PoolInterface::MapType<K, V>;

	Container(document::DocumentData *data) : StyleContainer(data) { }

	// Append every rule matching `node` (simple string-keyed + structured combinator/pseudo)
	// as a MatchedRule carrying its specificity + source order, WITHOUT merging. The caller
	// (StyleResolver) gathers across scopes, sorts by (specificity, order) and merges - so the
	// CSS cascade is honored across the whole set instead of a fixed lookup order.
	// `orderBias` folds the sheet's scope rank into the tie-break; `media` is stamped per rule.
	template <typename Vec>
	void collectMatches(Vec &out, Node *node, uint64_t filterBits, uint64_t orderBias,
			SpanView<bool> media) const {
		auto identity = node->getComponent<NodeIdentity>();
		PoolString key;

		// simple rules
		auto addSimple = [&](StringView k) {
			auto it = _styles.find(k);
			if (it != _styles.end()) {
				out.push_back(MatchedRule{&it->second.style, media, it->second.specificity,
					orderBias | it->second.order});
			}
		};
		addSimple(StringView("*"));
		if (identity) {
			if (!identity->type.empty()) {
				addSimple(StringView(identity->type));
			}
			for (auto &cl : identity->classes) {
				key.clear();
				key.append(1, '.').append(cl.data(), cl.size());
				addSimple(StringView(key));
				if (!identity->type.empty()) {
					key.clear();
					key.append(identity->type.data(), identity->type.size())
							.append(1, '.')
							.append(cl.data(), cl.size());
					addSimple(StringView(key));
				}
			}
			if (!identity->name.empty()) {
				key.clear();
				key.append(1, '#').append(identity->name.data(), identity->name.size());
				addSimple(StringView(key));
				if (!identity->type.empty()) {
					key.clear();
					key.append(identity->type.data(), identity->type.size())
							.append(1, '#')
							.append(identity->name.data(), identity->name.size());
					addSimple(StringView(key));
				}
			}
		}

		// structured combinator/pseudo rules, matched right-to-left
		if (!_complexStyles.empty()) {
			SceneNodeAccess access;
			auto tryBucket = [&](StringView bkey) {
				auto bit = _complexStyles.find(bkey);
				if (bit == _complexStyles.end()) {
					return;
				}
				for (auto sel : bit->second) {
					// ancestor Bloom fast-reject before the expensive backtracking walk
					if ((filterBits & sel->ancestorFilterBits) != sel->ancestorFilterBits) {
						continue;
					}
					if (matchComplex(*sel, node, access)) {
						out.push_back(MatchedRule{&sel->style, media, sel->specificity,
							orderBias | sel->order});
					}
				}
			};
			tryBucket(StringView("*"));
			if (identity) {
				if (!identity->type.empty()) {
					tryBucket(StringView(identity->type));
				}
				for (auto &cl : identity->classes) {
					key.clear();
					key.append(1, '.').append(cl.data(), cl.size());
					tryBucket(StringView(key));
				}
				if (!identity->name.empty()) {
					key.clear();
					key.append(1, '#').append(identity->name.data(), identity->name.size());
					tryBucket(StringView(key));
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

bool StyleSheet::init(uint32_t initVersion) {
	_version = initVersion;
	_pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	memory::perform([&] {
		_data = new (_pool) document::DocumentData(_pool);
		_container = new (_pool) Container(_data);
	}, _pool);
	return _data != nullptr && _container != nullptr;
}

bool StyleSheet::init(StringView css, uint32_t initVersion) {
	if (!init(initVersion)) {
		return false;
	}
	if (!addStyle(css)) {
		return false;
	}
	return true;
}

bool StyleSheet::init(const FileInfo &file, uint32_t initVersion) {
	if (!init(initVersion)) {
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

void StyleSheet::collectMatches(Vector<document::StyleContainer::MatchedRule> &out,
		NotNull<Node> node, uint64_t ancestorFilterBits, uint64_t orderBias,
		SpanView<bool> mediaResolved) const {
	_container->collectMatches(out, node, ancestorFilterBits, orderBias, mediaResolved);
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

} // namespace stappler::xenolith::ui
