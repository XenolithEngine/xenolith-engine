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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNREGISTRY_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNREGISTRY_H_

#include "XLUiMarkdownTypes.h" // IWYU pragma: keep
#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class MarkdownBuilder;

// What a tag factory is told: the document node being turned into a scene node, the parent it goes
// under, and the builder itself for a factory that wants to recurse (`buildChildren`) or to compose
// text (`buildText`).
struct SP_PUBLIC MarkdownBuilderContext {
	MarkdownBuilder *builder = nullptr;
	Node *parent = nullptr;
	const document::Node *source = nullptr;
};

/* One markdown tag: what node it becomes, and who fills it.

- `create` makes the node; nullptr drops the node and its subtree;
- `textContent` makes the node's inline content its string, with a style range per construct
  (Labels only);
- `buildContent` takes over the children (a list item and its marker, a table); return false to
  fall back to the default walk. */
struct SP_PUBLIC MarkdownTagFactory {
	Function<Rc<Node>(const MarkdownBuilderContext &)> create;

	bool textContent = false;

	Function<bool(const MarkdownBuilderContext &, Node *)> buildContent;
};

/* Extensible html-tag -> node factory, keyed by the tag the Markdown parser produced.

`createDefault()` registers every block tag the parser emits (see stappler/markdown). Inline tags
are not here: they never become nodes (see MarkdownInline).

An application replaces one tag and keeps the rest:

	auto registry = MarkdownRegistry::createDefault();
	registry->set("pre", MarkdownTagFactory{ .create = [] (const auto &) -> Rc<Node> {
		return Rc<MyCodeBlock>::create();
	}});
	view->setRegistry(sp::move(registry)); */
class SP_PUBLIC MarkdownRegistry : public Ref {
public:
	virtual ~MarkdownRegistry() = default;

	static Rc<MarkdownRegistry> createDefault();

	void set(StringView tag, MarkdownTagFactory &&);

	const MarkdownTagFactory *get(StringView tag) const;

protected:
	Map<String, MarkdownTagFactory, sprt::less<void>> _tags;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNREGISTRY_H_
