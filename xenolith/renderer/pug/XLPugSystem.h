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

#ifndef XENOLITH_RENDERER_PUG_XLPUGSYSTEM_H_
#define XENOLITH_RENDERER_PUG_XLPUGSYSTEM_H_

#include "XLPugNodeBuilder.h"
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

/* A System that renders a PUG template into its owner node.

Attach it to a Node and give it a template source (an inline string or a file);
on entering the scene it reads the template and builds the child nodes inside the
owner. It keeps a `spug::Context` alive for its whole lifetime, so variables and
functions set via `setVariable`/`setFunction` persist across rebuilds.

Cascading resolution: when the template runs, the system links its Context to the
nearest ancestor `TemplateSystem`'s Context (found by walking up the node parent
chain). Because pug resolves both variables and functions through the same
`VarScope` chain, a template lower in the hierarchy transparently sees the
variables and functions defined by a `TemplateSystem` on an ancestor node. Only
the first `TemplateSystem` per node participates in the cascade.

Constraints:
 - templates must not assign root-level variables (use `setVariable`); root-level
   writes would land in the per-build scratch pool and dangle;
 - the ancestor system (its Context/pool) must outlive its descendants - this holds
   naturally since an ancestor node owns its descendants;
 - `include` directives are unsupported unless an include callback is wired via
   `updateContext` + `spug::Context::setIncludeCallback`. */
class SP_PUBLIC TemplateSystem : public System {
public:
	virtual ~TemplateSystem();

	// inline string template source
	bool init(StringView inlineTemplate, BuilderConfig && = BuilderConfig());
	// file template source
	bool init(const FileInfo &file, BuilderConfig && = BuilderConfig());

	// replace the source; takes effect on the next build()
	void setTemplate(StringView inlineTemplate);
	void setTemplateFile(const FileInfo &file);

	// variables/functions written directly into the persistent Context
	void setVariable(StringView name, Value &&);
	void setVariable(StringView name, const Value &);
	void setFunction(StringView name, spug::VarClass::Callback &&); // Var(VarStorage&, Var*, argc)

	// arbitrary population escape hatch, runs with the system's memory pool active
	void updateContext(const Callback<void(spug::Context &)> &);

	// the persistent Context; descendants link to it for the cascade
	spug::Context *getContext() const { return _context; }

	// tear down the current subtree and rebuild it from the template
	void rebuild();

	// root nodes produced by the last build, appended into the owner
	SpanView<Rc<Node>> getBuiltNodes() const { return _builtNodes; }
	void setBuildCallback(Function<void(TemplateSystem *, SpanView<Rc<Node>>)> &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleRemoved() override;

protected:
	bool initContext(BuilderConfig &&);
	void ensureTemplate();
	void build();
	void teardown();
	TemplateSystem *findAncestor() const;

	memory::pool_t *_pool = nullptr;
	spug::Context *_context = nullptr;
	spug::Template *_template = nullptr;

	String _source; // inline content OR file path
	FileCategory _category = FileCategory::Custom;
	bool _isFile = false;
	bool _templateDirty = true;
	bool _styleSheetAttached = false;

	BuilderConfig _config;
	Vector<Rc<Node>> _builtNodes;
	bool _built = false;
	Function<void(TemplateSystem *, SpanView<Rc<Node>>)> _buildCallback;
};

} // namespace stappler::xenolith::pugui

#endif /* XENOLITH_RENDERER_PUG_XLPUGSYSTEM_H_ */
