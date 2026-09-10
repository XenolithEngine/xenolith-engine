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

#include "XLUiMarkdownStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using ParameterName = document::ParameterName;

// One node's CSS identity, written so that two nodes a selector cannot tell apart produce the
// same text. Classes come out of a hash set, so they are sorted: an unordered key would split
// the cache into as many entries as the set happens to iterate in.
static void MarkdownStyle_appendIdentity(StringStream &out, Node *node) {
	auto identity = node->getComponent<NodeIdentity>();
	if (!identity) {
		out << '*';
		return;
	}

	out << identity->type;

	if (!identity->classes.empty()) {
		Vector<StringView> classes;
		classes.reserve(identity->classes.size());
		for (auto &it : identity->classes) { classes.emplace_back(it); }
		sprt::sort(classes.begin(), classes.end());
		for (auto &it : classes) { out << '.' << it; }
	}

	if (!identity->name.empty()) {
		out << '#' << identity->name;
	}
}

/* The whole ancestor path, root first.

All of it, not just up to the nearest stylesheet: `resolveStyleForNode` collects EVERY sheet on
the chain, and an application that puts its own sheet above the view (as the test stand does) is
exactly the case where the part above the view decides the answer. */
static String MarkdownStyle_contextKey(Node *label) {
	Vector<Node *> chain;
	for (Node *p = label; p != nullptr; p = p->getParent()) { chain.emplace_back(p); }

	StringStream out;
	for (size_t i = chain.size(); i-- > 0;) {
		MarkdownStyle_appendIdentity(out, chain[i]);
		if (i > 0) {
			out << '/';
		}
	}
	return out.str();
}

/* What the probe changes about the block, property by property.

Both halves of the test matter. `has()` alone would take a property the probe merely INHERITED
from the block and re-emit it as a range - harmless for a colour, wrong for anything the label
computes from its own value. A value comparison alone would take a property nobody declared,
because an undeclared property answers with the CSS default rather than with nothing: a block at
`font-weight: bold` (every heading) would have each of its inlines forced back to normal. */
static void MarkdownStyle_delta(const ResolvedStyle &probe, const ResolvedStyle &base,
		font::FontController *controller, basic2d::Label::Style &out) {
	if (probe.has(ParameterName::CssFontSize) && probe.fontSize() != base.fontSize()) {
		out.set(probe.fontSize());
	}
	if (probe.has(ParameterName::CssFontStyle)
			&& probe.fontStyle().get() != base.fontStyle().get()) {
		out.set(probe.fontStyle());
	}
	if (probe.has(ParameterName::CssFontWeight)
			&& probe.fontWeight().get() != base.fontWeight().get()) {
		out.set(probe.fontWeight());
	}
	if (probe.has(ParameterName::CssFontStretch)
			&& probe.fontStretch().get() != base.fontStretch().get()) {
		out.set(probe.fontStretch());
	}
	if (probe.has(ParameterName::CssColor) && probe.color() != base.color()) {
		out.set(probe.color());
	}
	if (probe.has(ParameterName::CssTextDecoration)
			&& probe.textDecoration() != base.textDecoration()) {
		out.set(probe.textDecoration());
	}
	if (probe.has(ParameterName::CssTextTransform)
			&& probe.textTransform() != base.textTransform()) {
		out.set(probe.textTransform());
	}
	if (probe.has(ParameterName::CssVerticalAlign)
			&& probe.verticalAlign() != base.verticalAlign()) {
		out.set(probe.verticalAlign());
	}
	if (probe.has(ParameterName::CssHyphens) && probe.hyphens() != base.hyphens()) {
		out.set(probe.hyphens());
	}

	// Opacity is NOT inherited, so a block's own opacity never reaches the probe and the two are
	// compared against different things. `has()` carries the whole test here: only a rule that
	// named the inline itself may dim it.
	if (probe.has(ParameterName::CssOpacity) && probe.opacity() != base.opacity()) {
		out.set(basic2d::Label::Opacity(probe.opacity()));
	}

	// A range carries the family as the controller's index, not as a name. With no controller
	// there is nothing to resolve it against, and a wrong index renders in a wrong face - so the
	// property is dropped instead of guessed.
	if (controller && probe.has(ParameterName::CssFontFamily)) {
		auto family = probe.fontFamily();
		if (family != base.fontFamily()) {
			auto index = controller->getFamilyIndex(family);
			if (index != maxOf<uint32_t>()) {
				out.set(basic2d::Label::FontFamily(index));
			}
		}
	}
}

MarkdownInlineResolver::MarkdownInlineResolver(font::FontController *controller)
: _controller(controller) { }

const ResolvedStyle *MarkdownInlineResolver::acquireBaseline(basic2d::Label *label,
		StringView contextKey) {
	auto it = _baselines.find(contextKey);
	if (it != _baselines.end()) {
		return &it->second;
	}

	auto resolved = StyleResolver::resolveStyleForNode(label);
	if (!resolved.valid()) {
		return nullptr;
	}

	_valid = true;
	_structural = _structural || resolved.hasStructuralSelectors();
	++_probes;

	return &_baselines.emplace(contextKey.str<Interface>(), sp::move(resolved)).first->second;
}

bool MarkdownInlineResolver::resolve(NotNull<basic2d::Label> label, StringView chain,
		basic2d::Label::Style &out) {
	if (chain.empty()) {
		return false;
	}

	auto contextKey = MarkdownStyle_contextKey(label.get());
	auto cacheKey = toString(contextKey, '|', chain);

	if (!_structural) {
		auto it = _chains.find(cacheKey);
		if (it != _chains.end()) {
			out = it->second;
			return !out.params.empty();
		}
	}

	auto base = acquireBaseline(label.get(), contextKey);
	if (!base) {
		return false;
	}

	// The chain has to be a real chain of nodes: descendant and child combinators are matched by
	// walking parents, and inheritance travels the same way. There is no API for a hypothetical
	// element path, and inventing one would mean reimplementing the matcher.
	Rc<Node> root;
	Node *parent = label.get();
	Node *inner = nullptr;

	chain.split<StringView::Chars<'>'>>([&](StringView tag) {
		auto probe = Rc<Node>::create();
		probe->setType(tag);
		probe->addStyleClass(toString("md-", tag));

		// Nothing to draw and nothing to lay out; it exists for one call and is gone.
		probe->setVisible(false);

		inner = parent->addChild(sp::move(probe));
		if (!root) {
			root = inner;
		}
		parent = inner;
	});

	if (!inner) {
		return false;
	}

	auto resolved = StyleResolver::resolveStyleForNode(inner);
	++_probes;

	// Before anything can look at the tree again: the probe is not part of the document, and a
	// Label that kept one would carry it into the frame.
	root->removeFromParent();

	if (!resolved.valid()) {
		return false;
	}

	_structural = _structural || resolved.hasStructuralSelectors();

	basic2d::Label::Style style;
	MarkdownStyle_delta(resolved, *base, _controller, style);

	if (!_structural) {
		_chains.emplace(sp::move(cacheKey), style);
	}

	out = sp::move(style);
	return !out.params.empty();
}

} // namespace stappler::xenolith::ui
