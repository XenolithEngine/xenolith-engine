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

#ifndef XENOLITH_RENDERER_PUG_XLPUGREGISTRY_H_
#define XENOLITH_RENDERER_PUG_XLPUGREGISTRY_H_

#include "XLPugConfig.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

class NodeBuilder;

/* Environment passed to tag factories and attribute appliers.

All strings and values here are std-memory (xenolith `Value`), already copied
out of the template's memory pool. */
struct SP_PUBLIC BuilderContext {
	NodeBuilder *builder = nullptr;

	// node the newly created child will be added to
	Node *parent = nullptr;

	// resolves handler-name attributes ("on-tap") into application callbacks
	const Function<Function<void()>(StringView)> *resolveHandler = nullptr;

	const Function<void(StringView)> *onError = nullptr;

	Function<void()> resolve(StringView name) const;
	void error(StringView msg) const;
};

/* One template tag: how to construct a node and apply its content. */
struct SP_PUBLIC TagFactory {
	// required: create the node from the tag and the full collected attribute dict
	Function<Rc<Node>(const BuilderContext &, StringView tag, const Value &attrs)> create;

	// optional: consume a tag-specific attribute, return true when handled
	// (runs before the generic Node attribute applier)
	Function<bool(const BuilderContext &, Node *, StringView name, const Value &value)>
			applyAttribute;

	// optional: apply accumulated text content (Label::setString and alike)
	Function<void(const BuilderContext &, Node *, StringView text)> applyText;
};

/* Extensible tag -> node factory registry.

`createDefault()` registers the built-in tags:
 - `node`         plain Node
 - `layer`        basic2d::Layer (`color` attribute)
 - `label`        basic2d::Label (`font-size`, `align`, `width`; text content)
 - `sprite`/`image` basic2d::VectorSprite (`src` attribute)
 - `flex`         Node + simpleui::LayoutSystem in flex mode (container attributes: `direction`,
                  `wrap`, `justify-content`, `align-items`, `align-content`,
                  `gap`, `row-gap`, `column-gap`, `padding`)
 - `button`       simpleui::Button (`on-tap` handler name, `enabled`)
 - `button-label` simpleui::ButtonWithLabel (`on-tap`; text content)

Generic Node attributes handled for every tag: `position` ([x,y]), `x`, `y`,
`size`/`content-size` ([w,h]), `anchor`/`anchor-point` ([x,y] or a keyword like
`center`, `top-left`, `middle-left`), `color` (#RRGGBB[AA] or named), `opacity`,
`visible`, `scale` (float or [x,y]), `rotation` (degrees), `z-index`/`z-order`,
`id`/`name` (-> setName), `tag` (-> setTag), `class` and `data-*` (stored into
the node's data value). Flex item attributes (`flex-grow`, `flex-shrink`,
`flex-basis`, `cross-size`, `align-self`, `order`, `min-main`, `max-main`,
`margin`) accumulate into a `FlexItemInfo` component. */
class SP_PUBLIC Registry : public Ref {
public:
	virtual ~Registry() = default;

	static Rc<Registry> createDefault();

	// register or replace a tag (application extension point)
	void set(StringView tag, TagFactory &&);

	const TagFactory *get(StringView tag) const;

	// generic Node attribute applier, used for every node after the per-tag applier;
	// returns true when the attribute was consumed
	static bool applyGenericAttribute(const BuilderContext &, Node *, StringView name,
			const Value &);

protected:
	Map<String, TagFactory, sprt::less<void>> _tags;
};

} // namespace stappler::xenolith::pugui

#endif /* XENOLITH_RENDERER_PUG_XLPUGREGISTRY_H_ */
