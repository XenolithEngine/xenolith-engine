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

#ifndef XENOLITH_RENDERER_UI_STYLE_XLUISTYLESYSTEM_H_
#define XENOLITH_RENDERER_UI_STYLE_XLUISTYLESYSTEM_H_

#include "XLUiStyleSheet.h"
#include <sprt/runtime/dispatch/handle.h> // WatchHandle

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Marker component on a stylesheet owner node. */
struct SP_PUBLIC StyleSystemState {
	static ComponentId Id;

	uint64_t systemId = 0; // track, which system is owner to update when tree is changed
	uint32_t version = 0; // track system content update
};

/* Custom CSS properties declared on ONE node, outside any stylesheet.

This is the per-element channel a stylesheet cannot express: a rule reaches a SET of nodes, so a
value that differs per node — a tree row's depth, a progress bar's ratio, a chart bar's height —
has nowhere to live in the sheet. Declared here, it participates in the cascade exactly like a
`--name: value` declaration written for that node: it is inherited by the subtree, it is visible
to `var()` in any declaration that node resolves, and being node-local it beats every rule that
matched the same node.

	setStyleVariable(row, "--depth", "3");

	.fs-row { padding-left: calc(8px + var(--depth, 0) * var(--indent)); }

The value is raw text, like every custom property: it is parsed only where it is substituted, so
the same variable can carry a length, a colour or a whole shorthand — and a typo is diagnosed at
the use, not here.

Changing it re-resolves the node and its subtree through the ordinary components-dirty path. */
struct SP_PUBLIC StyleVariables {
	static ComponentId Id;

	// "--name" -> raw value text. Names are stored normalised (leading "--", lower case), so
	// `get("depth")` and `get("--DEPTH")` find the same entry.
	Map<String, String> vars;

	// Raw text of a property, or empty when this node does not declare it. Does NOT consult
	// ancestors — inheritance happens during resolution, not here.
	StringView get(StringView name) const;

	bool operator==(const StyleVariables &) const = default;
};

// Declare or replace a custom property on `node`. `name` may be written with or without the
// leading "--". Returns true when the value actually changed (and the node was marked dirty).
SP_PUBLIC bool setStyleVariable(NotNull<Node>, StringView name, StringView value);

// Drop a property declared by setStyleVariable. Returns true when it was there.
SP_PUBLIC bool removeStyleVariable(NotNull<Node>, StringView name);

// Marker recording that THIS StyleResolver created the node's LayoutSystem. The
// applier only removes layouts it added, so pug `flex` tags and programmatic
// LayoutSystems (which carry no marker) are left untouched.
struct StyleManagedLayout {
	static ComponentId Id;
};

// Marker recording that THIS StyleResolver created the node's ScrollSystem, from a non-`visible`
// `overflow`. Same contract as StyleManagedLayout: only a system the resolver added may the
// resolver take away, so a scroll container built in code survives a pass that matched nothing.
struct StyleManagedScroll {
	static ComponentId Id;
};

// Marker recording that a SYSTEM on this node owns the layout of its children - it writes their
// ContentSize and positions them itself. The exact counterpart of StyleManagedLayout: that one says
// "the resolver created this layout", this one says "the resolver keeps out of it".
//
// It changes two decisions in StyleResolver:
//
// - a CSS width/height on a CHILD is handed to the owner as a MeasureComponent input instead of
//   being committed with setContentSize, exactly as under a flex/grid container. Without it a
//   container that lays its children out by other means (ui::DockSystem) would fight the resolver
//   for ContentSize on every frame: style writes the size, the owner overwrites it, the resulting
//   ContentSizeDirty re-runs the resolver;
//
// - `display: flex|grid` on THIS node neither creates nor reconfigures a LayoutSystem, so a
//   stylesheet can neither silently reshape a hand-built layout nor add a second writer of the
//   children's geometry beside the system that already owns them.
//
// Unlike FlexLayoutInfo / GridLayoutInfo it publishes no parameters, so no per-item component is
// derived from it either - the owner system reads whatever it needs by itself.
struct SP_PUBLIC SystemManagedLayout {
	static ComponentId Id;

	bool operator==(const SystemManagedLayout &) const = default;
};

/* Attaches a StyleSheet to a node: the sheet applies to the owner and its subtree.

Multiple systems may be present on the ancestor chain - outer sheets apply
first, inner (nearer) sheets override. Media parameters default from the
Director's frame constraints on enter (surface size in points, density for
fonts) and may be set explicitly. */
class SP_PUBLIC StyleSystem : public System {
public:
	// should be before layout
	static constexpr uint32_t StyleDefaultPriority = System::DefaultPriority - 1'000;

	virtual ~StyleSystem() = default;

	virtual bool init() override;
	virtual bool init(Rc<StyleSheet> &&);
	virtual bool init(StringView css);
	virtual bool init(const FileInfo &);

	bool addStyle(StringView css);
	bool addStyle(const FileInfo &);

	void setStyleSheet(Rc<StyleSheet> &&);
	StyleSheet *getStyleSheet() const { return _sheet; }

	void setMediaParameters(const document::MediaParameters &);
	const document::MediaParameters &getMediaParameters() const { return _media; }

	// lazily re-evaluated when the sheet version changes
	SpanView<bool> getMediaResolved();

	// force re-resolution in the owner's subtree (bumps StyleSheetState)
	void invalidateStyles();

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

protected:
	void updateMedia();

	// (re)build _sheet from the recorded sources and swap it in (bumps the sheet version
	// and invalidates the owner subtree). Used for the initial build and for live reload.
	bool rebuildFromSources();

	// watch every file-backed source for changes; on change, rebuild + reload on the app
	// thread. No-op when there are no file sources or the platform has no watch backend.
	void registerWatches();
	void cancelWatches();

	// a recorded stylesheet input, replayed when a watched file changes
	struct StyleSource {
		bool file = false;
		String value; // css text, or file path when `file`
		FileCategory category = FileCategory::Custom;
		FileFlags flags = FileFlags::None;
	};

	Rc<StyleSheet> _sheet;
	Vector<StyleSource> _sources;
	Vector<Rc<sprt::dispatch::WatchHandle>> _watches;
	document::MediaParameters _media;
	Vector<bool> _mediaResolved;
	uint32_t _resolvedForVersion = maxOf<uint32_t>();
	bool _mediaExplicit = false;
	uint64_t _systemId = 0;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_STYLE_XLUISTYLESYSTEM_H_ */
