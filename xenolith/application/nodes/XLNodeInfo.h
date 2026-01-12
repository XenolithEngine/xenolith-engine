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

	// Call Node::handleComponentsDirty if parent components was updated
	HandleComponents = 1 << 2,

	// Call Node::handleReorderChildDirty if parent childs was updated
	HandleParentReorderChild = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(NodeEventFlags)

enum class CommandFlags : uint16_t {
	None,
	DoNotCount = 1 << 0
};

SP_DEFINE_ENUM_AS_MASK(CommandFlags)

struct SP_PUBLIC MaterialInfo {
	std::array<uint64_t, config::MaxMaterialImages> images = {0};
	std::array<uint16_t, config::MaxMaterialImages> samplers = {0};
	std::array<core::ColorMode, config::MaxMaterialImages> colorModes = {core::ColorMode()};
	core::PipelineMaterialInfo pipeline;

	uint64_t hash() const {
		return sprt::hash64(reinterpret_cast<const char *>(this), sizeof(MaterialInfo));
	}

	String description() const;

	bool operator==(const MaterialInfo &info) const = default;
	bool operator!=(const MaterialInfo &info) const = default;

	bool hasImage(uint64_t id) const;

	MaterialInfo() { memset(this, 0, sizeof(MaterialInfo)); }
};

struct SP_PUBLIC ZOrderLess {
	bool operator()(const SpanView<ZOrder> &l, const SpanView<ZOrder> &r) const noexcept {
		auto len = std::max(l.size(), r.size());
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

struct SP_PUBLIC DrawStat {
	uint32_t vertexes;
	uint32_t triangles;
	uint32_t zPaths;
	uint32_t drawCalls;

	uint32_t cachedImages;
	uint32_t cachedFramebuffers;
	uint32_t cachedImageViews;
	uint32_t materials;

	uint32_t solidCmds;
	uint32_t surfaceCmds;
	uint32_t transparentCmds;
	uint32_t shadowsCmds;

	uint32_t vertexInputTime;
};
} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLNODEINFO_H_ */
