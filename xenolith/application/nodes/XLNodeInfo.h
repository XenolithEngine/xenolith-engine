/**
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

#ifndef XENOLITH_APPLICATION_NODES_XLNODEINFO_H_
#define XENOLITH_APPLICATION_NODES_XLNODEINFO_H_

#include "XLContextInfo.h"
#include "XLCoreRenderSession.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using RenderingLevel = core::RenderingLevel;

using StateId = uint32_t;

static constexpr StateId StateIdNone = maxOf<StateId>();

static constexpr uint64_t InvalidTag = maxOf<uint64_t>();

enum class NodeVisitFlags : uint32_t {
	None,
	TransformDirty = 1 << 0,
	ContentSizeDirty = 1 << 1,
	ComponentsDirty = 1 << 2,
	ReorderChildDirty = 1 << 3,
	MeasureDirty = 1 << 4,

	GlobalTransformDirtyMask = TransformDirty | ContentSizeDirty
};

SP_DEFINE_ENUM_AS_MASK(NodeVisitFlags)

// This flags used to alter Node::handle<X> behavior
// If some flag is set, corresponding function will be called not only when node's
// own dirty flag is set. but when dirty flag was set in some of node's parents
enum class NodeEventFlags : uint32_t {
	None,

	// Call Node::handleTransformDirty if parent transform was dirty
	HandleParentTransform = 1 << 0,

	// Call Node::handleContentSizeDirty if parent ContentSize was dirty
	HandleParentContentSize = 1 << 1,

	// NB: bit 1 << 2 was NodeEventFlags::HandleComponents; ancestor components-dirty is now
	// opted into per-System via SystemFlags::HandleAncestorComponents (or a Node subclass via
	// Node::setWantsAncestorComponents), driven by a subtree listener counter

	// Call Node::handleReorderChildDirty if parent childs was updated
	HandleParentReorderChild = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(NodeEventFlags)

/** What a node offers to the per-frame hit-test registry (see InputListenerStorage::addHitTest).

A node with any of these bits set publishes the rect it was DRAWN with, once per frame, from its own
visit. Whoever asks "what is under this point" walks that registry backwards - registration order is
paint order - instead of keeping a list of its own, and a node that is not visited is not registered,
so an invisible, clipped-away or detached subtree stops answering with no bookkeeping at all.

The bit is a CACHE of "this node carries the matching component", maintained by the setter functions
that attach those components (ui::setContextMenu, setDropTarget, ui::setTooltip). Never set it by
hand: a bit with no component behind it makes the node win a hit test and then offer nothing, which
looks exactly like a target that deliberately blocks. */
enum class HitTestFlags : uint32_t {
	None,

	// An InputListener is attached to this node. Maintained by the listener itself; this is what
	// lets the listener publish the geometry its own hit test reads back (see
	// InputListener::_shouldProcessEvent).
	Pointer = 1 << 0,

	// DropTargetComponent: a drag can be dropped here
	DropTarget = 1 << 1,

	// ui::ContextMenuComponent: a right click or a long press opens a menu here
	ContextMenu = 1 << 2,

	// ui::TooltipComponent: resting the pointer here shows a hint
	Tooltip = 1 << 3,

	// 1 << 16 and up are free for applications
	ApplicationMask = 0xFFFF'0000,
};

SP_DEFINE_ENUM_AS_MASK(HitTestFlags)

// How a content measurement request interprets its constraints.
// Semantics mirror font::Formatter::ContentRequest.
enum class MeasureMode : uint8_t {
	Normal, // preferred size under the given constraints (wrap to fit)
	MinContent, // smallest size that avoids overflow (widest unbreakable unit)
	MaxContent, // ideal size without any wrapping
};

// Constraints for a content measurement request (see System::handleMeasure);
// maxOf<float>() means the axis is unconstrained
struct SP_PUBLIC MeasureConstraints {
	MeasureMode mode = MeasureMode::Normal;
	float maxWidth = maxOf<float>();
	float maxHeight = maxOf<float>();
};

enum class CommandFlags : uint16_t {
	None,
	DoNotCount = 1 << 0,

	// The command is always treated as changed: the generation comparison is skipped for it and
	// its bounds go into the frame damage unconditionally. For content that changes without its
	// data set changing (particles, video, the FPS overlay).
	AlwaysDirty = 1 << 1,

	// Bounds cannot be determined for this command; the frame escalates to full-surface damage.
	UnknownBounds = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(CommandFlags)

// Identity + version for a copy-on-write data set, used to compute damage rectangles between
// frames. `id` is stable for the lifetime of the object, `generation` changes whenever the
// contents may have changed. A model-space AABB is cached alongside, so damage collection never
// has to rescan the data of an unchanged set.
struct SP_PUBLIC DataIdentity {
	uint64_t id = 0;
	uint32_t generation = 0;
	uint32_t boundsGeneration = maxOf<uint32_t>();
	Rect bounds;

	// Whether the AABB may be derived by scanning the data. False for atlas-driven geometry.
	bool derivable = true;

	DataIdentity() : id(allocate()) { }

	void invalidate() {
		++generation;
		boundsGeneration = maxOf<uint32_t>();
	}

	void setBounds(const Rect &r) {
		bounds = r;
		boundsGeneration = generation;
	}

	bool hasBounds() const { return boundsGeneration == generation; }

	static uint64_t allocate();
};

struct SP_PUBLIC MaterialInfo {
	sprt::array<uint64_t, config::MaxMaterialImages> images = {0};
	sprt::array<uint16_t, config::MaxMaterialImages> samplers = {0};
	sprt::array<core::ColorMode, config::MaxMaterialImages> colorModes = {core::ColorMode()};
	core::PipelineMaterialInfo pipeline;

	uint64_t hash() const {
		return sprt::hash64(reinterpret_cast<const char *>(this), sizeof(MaterialInfo));
	}

	String description() const;

	bool operator==(const MaterialInfo &info) const = default;
	bool operator!=(const MaterialInfo &info) const = default;

	bool hasImage(uint64_t id) const;

	MaterialInfo() { sprt::memset(this, 0, sizeof(MaterialInfo)); }
};

struct SP_PUBLIC ZOrderLess {
	bool operator()(const SpanView<ZOrder> &l, const SpanView<ZOrder> &r) const noexcept {
		auto len = sprt::max(l.size(), r.size());
		for (size_t i = 0; i < len; ++i) {
			auto valL = (i < l.size()) ? l.at(i) : ZOrder(0);
			auto valR = (i < r.size()) ? r.at(i) : ZOrder(0);
			if (valL != valR) {
				return valL < valR;
			}
		}
		return false;
	} // namespace stappler::xenolith
};

struct SP_PUBLIC DrawStateValues {
	core::DynamicState enabled = core::DynamicState::None;
	URect viewport;
	URect scissor;

	// used to extend state
	Rc<Ref> data;

	bool operator==(const DrawStateValues &) const = default;

	bool isScissorEnabled() const {
		return (enabled & core::DynamicState::Scissor) != core::DynamicState::None;
	}
	bool isViewportEnabled() const {
		return (enabled & core::DynamicState::Viewport) != core::DynamicState::None;
	}
};

using core::DrawStat;

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLNODEINFO_H_ */
