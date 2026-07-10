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

#ifndef XENOLITH_RENDERER_PUG_XLPUGNODEBUILDER_H_
#define XENOLITH_RENDERER_PUG_XLPUGNODEBUILDER_H_

#include "XLPugRegistry.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

struct SP_PUBLIC BuilderConfig {
	// tag registry; null - use Registry::createDefault()
	Rc<Registry> registry;

	// resolves handler-name attributes ("on-tap") into application callbacks
	Function<Function<void()>(StringView)> resolveHandler;

	Function<void(StringView)> onError;

	// strip leading/trailing whitespace from accumulated node text
	bool trimText = true;

	// CSS stylesheet: a simpleui::StyleSheetSystem is attached to the tree root
	// (or the target parent node) and styles auto-apply to every produced node
	Rc<simpleui::StyleSheet> styleSheet;

	// attach simpleui::StyleApplier to every produced node even without styleSheet
	// (styles then resolve against stylesheet scopes above the tree)
	bool enableStyles = false;
};

/* pug::NodeStream implementation that builds a xenolith node tree.

Must run on the scene/application thread, like any other node construction.
Everything received from the template run (pool-backed strings and values) is
copied into std memory immediately; the produced `Rc<Node>` tree safely
outlives the template's memory pool.

Nodes materialize lazily: the factory is called with the tag and the complete
attribute dict when the first child/text arrives or the node is closed. Text
pieces accumulate and are applied through the tag's `applyText` on `popNode`. */
class SP_PUBLIC NodeBuilder final : public spug::NodeStream {
public:
	NodeBuilder(NotNull<Node> root, BuilderConfig &&);
	virtual ~NodeBuilder() = default;

	virtual bool pushNode(StringView tag) override;
	virtual bool popNode() override;
	virtual bool setAttribute(StringView name, const spug::Value &, bool escaped) override;
	virtual bool pushString(StringView) override;
	virtual void onError(StringView) override;

	Node *getRoot() const { return _root; }

	// false after an unbalanced pop or a fatal factory error
	bool isValid() const { return _valid; }

protected:
	struct Pending {
		String tag;
		Value attrs; // xenolith Value dict (std memory)
		String text;
		const TagFactory *factory = nullptr;
		Rc<Node> node; // set after materialization
	};

	// create the node via its factory, apply attributes, add to the parent
	Node *materialize(Pending &, Node *parent);
	Node *materializeTop();
	BuilderContext makeContext(Node *parent);

	Vector<Pending> _stack;
	Rc<Node> _root;
	BuilderConfig _config;
	bool _valid = true;
};

/* Low-level entry point: run a template (compiled with `spug::Template::Options::getNodes()`)
with a caller-owned Context into a NodeBuilder. `pugui::TemplateSystem` (XLPugSystem.h) is the
high-level, Node-attached way to render a template source into a node tree. */
SP_PUBLIC bool runTemplate(const spug::Template &, spug::Context &, NodeBuilder &);

} // namespace stappler::xenolith::pugui

#endif /* XENOLITH_RENDERER_PUG_XLPUGNODEBUILDER_H_ */
