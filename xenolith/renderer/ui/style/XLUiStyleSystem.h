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

/* Custom CSS properties declared on one node, outside any stylesheet (per-node values such as a
tree row's depth). They cascade like a `--name: value` declaration for that node, are inherited,
and beat every matched rule. Values are raw text, parsed where substituted:

	setStyleVariable(row, "--depth", "3");
	.fs-row { padding-left: calc(8px + var(--depth, 0) * var(--indent)); }

Changing it re-resolves the node and its subtree via components-dirty. */
struct SP_PUBLIC StyleVariables {
	static ComponentId Id;

	// "--name" -> raw value text. Names are stored normalised (leading "--", lower case), so
	// `get("depth")` and `get("--DEPTH")` find the same entry.
	Map<String, String> vars;

	// raw text of a property, or empty when this node does not declare it; ancestors are not
	// consulted
	StringView get(StringView name) const;

	bool operator==(const StyleVariables &) const = default;
};

// Declare or replace a custom property on `node`. `name` may be written with or without the
// leading "--". Returns true when the value actually changed (and the node was marked dirty).
SP_PUBLIC bool setStyleVariable(NotNull<Node>, StringView name, StringView value);

// Drop a property declared by setStyleVariable. Returns true when it was there.
SP_PUBLIC bool removeStyleVariable(NotNull<Node>, StringView name);

// Marker: the StyleResolver created the node's LayoutSystem. It only removes layouts it added,
// so pug `flex` tags and programmatic LayoutSystems are left untouched.
struct StyleManagedLayout {
	static ComponentId Id;
};

// Marker: the StyleResolver created the node's ScrollSystem from a non-`visible` `overflow`;
// only such a system may the resolver remove.
struct StyleManagedScroll {
	static ComponentId Id;
};

// Marker: a system on this node (e.g. ui::DockSystem) lays out its children. The resolver then
// passes a child's CSS width/height as a MeasureComponent input instead of setContentSize, and
// `display: flex|grid` on this node creates no LayoutSystem. Publishes no per-item parameters.
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

	/* Set or clear one `@media (x-option: name)` flag, and re-resolve the subtree. The engine
	seeds `rtl` from `locale::getTextDirection()` in updateMedia and on locale change, e.g.
	`@media (x-option: rtl) { :root { direction: rtl; } }`. */
	void setMediaOption(StringView name, bool enabled);
	bool hasMediaOption(StringView name) const { return _media.hasOption(name); }

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

	// keeps the `rtl` media flag in step with `locale::setLocale`; added to the owner in
	// handleAdded, subscribed in handleEnter
	EventListener *_localeListener = nullptr;
	sprt::dispatch::BusDelegate *_localeDelegate = nullptr; // owned by the listener

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
