/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DVERTEXPLAN_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DVERTEXPLAN_H_

#include "XL2dCommandList.h"
#include "XL2dFrameContext.h"
#include "XL2dDamage.h"
#include "XLCoreMaterial.h"
#include "glsl/include/XL2dGlslVertexData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

// The draw plan of a 2d frame, with no backend in it.
//
// A frame's command list is walked once into per-material/per-state write plans, the plans are
// then flattened into three flat arrays (vertexes, indexes, transforms) plus a list of VertexSpan
// draws over them. Nothing here touches a GPU: the arrays are plain memory and a VertexSpan is
// just the arguments of an indexed draw. A Vulkan backend maps device buffers and hands over their
// pointers; a software rasterizer allocates host arrays and reads the same bytes back. Extracted
// from XL2dVkVertexPass so the two cannot drift - the plan is subtle (packed vs instanced chains,
// painter-order sorting, CPU atlas resolution) and one wrong index shows up as a blank frame.

// Where the plan writes. Three raw pointers plus the running offsets into them; the caller sizes
// the allocations from VertexPlan::globalWritePlan after the command walk.
struct SP_PUBLIC VertexWriteTarget {
	TransformData *transform = nullptr;
	uint8_t *vertexes = nullptr;
	uint8_t *indexes = nullptr;

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = 0;
};

// The seam between the plan and whoever runs it: what the plan reads from the frame, and what it
// produces. Deliberately free of backend types - a backend keeps its buffers and devices in its
// own processor object and passes only this.
struct SP_PUBLIC VertexPlanContext {
	// inputs
	FrameContextHandle2d *input = nullptr;
	const core::MaterialSet *materialSet = nullptr;

	// Damage is collected by the very same walk: the commands are visited once anyway, and
	// deferred results are already resolved there, so their bounds are exact.
	bool collectDamage = false;
	DamageCollector *damage = nullptr;

	// outputs
	Vector<VertexSpan> materialSpans;
	Vector<VertexSpan> shadowSolidSpans;
	Vector<VertexSpan> shadowSdfSpans;

	// Kept apart from materialSpans, and that separation is the whole point: the backend has to be
	// able to draw these AFTER the frame has been copied out (see FrameCapture). Merged into
	// materialSpans they would be indistinguishable - the per-bucket counters below are statistics,
	// not boundaries.
	Vector<VertexSpan> overlaySpans;

	uint32_t solidCmds = 0;
	uint32_t surfaceCmds = 0;
	uint32_t transparentCmds = 0;
	uint32_t overlayCmds = 0;

	const core::Material *getMaterialById(core::MaterialId id) const {
		return materialSet ? materialSet->getMaterialById(id) : nullptr;
	}
};

// Lives in a frame-local pool and is deleted when the frame's vertex data has been written.
// Allocate with `new (pool) VertexPlan` and `delete` it (never sprt::__delete - see the pool rules).
struct SP_PUBLIC VertexPlan : public InterfaceObject<memory::PoolInterface>,
							  public memory::PoolInterface::AllocBaseType {
	using WriteTarget = VertexWriteTarget;
	using Context = VertexPlanContext;

	struct VertexDataPlanInfo : public memory::PoolInterface::AllocBaseType {
		VertexDataPlanInfo *next = nullptr;
		SpanView<InstanceVertexData> vertexes;
		SpanView<ZOrder> zOrder;
		float depthValue = 0.0f;

		uint32_t vertexOffset = 0;
		uint32_t vertexCount = 0;
		uint32_t transformOffset = 0;
		uint32_t transformCount = 0;

		// traversal order, i.e. the order the scene graph pushed the commands in. The chains below
		// keep it, so this is only needed as the tie-breaker for equal zPaths in flat mode.
		uint32_t order = 0;
	};

	struct StatePlanInfo {
		const StateData *stateData = nullptr;

		/* Command order IS painter's order, and the chains keep it (append at the tail). */
		VertexDataPlanInfo *instanced = nullptr;
		VertexDataPlanInfo *instancedTail = nullptr;
		VertexDataPlanInfo *packed = nullptr;
		VertexDataPlanInfo *packedTail = nullptr;

		void appendInstanced(VertexDataPlanInfo *info) { append(instanced, instancedTail, info); }
		void appendPacked(VertexDataPlanInfo *info) { append(packed, packedTail, info); }

		Vector<const CmdParticleEmitter *> particles;

		uint32_t gradientStart = 0;
		uint32_t gradientCount = 0;

	private:
		static void append(VertexDataPlanInfo *&head, VertexDataPlanInfo *&tail,
				VertexDataPlanInfo *info) {
			if (tail) {
				tail->next = info;
			} else {
				head = info;
			}
			tail = info;
		}
	};

	struct MaterialWritePlan {
		const core::Material *material = nullptr;
		Rc<core::DataAtlas> atlas;
		uint32_t vertexes = 0;
		uint32_t indexes = 0;
		uint32_t transforms = 0;
		uint32_t instances = 0;
		Map<StateId, StatePlanInfo> states;
	};

	Map<SpanView<ZOrder>, float, ZOrderLess> paths;

	// Overlay zPaths are kept apart from `paths` because they do not take part in the content's depth
	// ordering at all: updatePathsDepth puts every one of them at the near plane. See it for why
	// zero, and not merely a band below the content.
	Map<SpanView<ZOrder>, float, ZOrderLess> overlayPaths;

	// fill write plan
	MaterialWritePlan globalWritePlan;

	// write plan for objects, that do depth-write and can be drawn out of order
	Map<core::MaterialId, MaterialWritePlan> solidWritePlan;

	// write plan for objects without depth-write, that can be drawn out of order
	Map<core::MaterialId, MaterialWritePlan> surfaceWritePlan;

	// write plan for transparent objects, that should be drawn in order
	Map<SpanView<ZOrder>, Map<core::MaterialId, MaterialWritePlan>, ZOrderLess>
			transparentWritePlan;

	// write plan for the Overlay level: drawn last, after the frame has been captured. Shaped like
	// the transparent plan - keyed by zPath - so painter's order is what orders it.
	Map<SpanView<ZOrder>, Map<core::MaterialId, MaterialWritePlan>, ZOrderLess> overlayWritePlan;

	Extent3 surfaceExtent;
	core::SurfaceTransformFlags transform = core::SurfaceTransformFlags::Identity;
	Vec2 shadowSize = Vec2(1.0f, 1.0f);

	// When false, the glyph atlas is resolved here on the CPU instead of being probed by the
	// vertex shader; a backend without buffer device addresses (or without shaders at all) sets it.
	bool hasGpuSideAtlases = false;

	// The CPU atlas resolution normally consumes the object id: it becomes texture coordinates and
	// the field is cleared. A backend that draws each glyph from its own storage rather than from a
	// packed atlas image needs the id to survive into the vertex, and sets this.
	bool keepAtlasObjects = false;

	// FlatPass has no depth buffer: draws are emitted in painter's order and particles are dropped
	bool flatOrder = false;
	uint32_t orderCounter = 0;

#if XL_FRAME_ACCOUNT
	/* The frame's deferred account, gathered here because this is where the frame CONSUMES what
	was deferred - see pushDeferred for what each one means and why the first two may not be added
	together. Nanoseconds. */
	uint64_t deferredWorkTime = 0; // summed across worker threads; may exceed the frame
	uint64_t deferredWaitTime = 0; // this thread standing still; always part of the frame
	uint32_t deferredCount = 0; // results consumed
	uint32_t deferredWaited = 0; // of those, how many were not finished when we got there

	/* The stage's own two halves, filled by pushAll.

	Kept here rather than timed from the backend, because pushAll is where the two are and a caller
	outside it could only report their sum - which is the number that was already known and did not
	say anything. */
	uint64_t writeTime = 0; // copying vertexes, indexes and transforms into the buffers
	uint64_t spanTime = 0; // turning the write plans into draw spans, painter order included

	/* And the command walk splits in two as well: collecting a damage rectangle for every instance,
	or building the write plan itself. Only a measurement says which half is the cost. */
	uint64_t damageTime = 0;
	uint64_t planTime = 0;
#endif

	uint32_t excludeVertexes = 0;
	uint32_t excludeIndexes = 0;
	float maxShadowValue = 0.0f;

	memory::pool_t *pool = nullptr;

	// Walk one command into the plans. Call for every command of the frame, in list order.
	void pushCommand(Context &, const Command *c);

	StatePlanInfo *acquireStatePlan(FrameContextHandle2d *input, const core::Material *material,
			Map<core::MaterialId, MaterialWritePlan> &writePlan, const CmdInfo *c);

	void emplaceWritePlan(FrameContextHandle2d *input, const core::Material *material,
			Map<core::MaterialId, MaterialWritePlan> &writePlan, const Command *c,
			const CmdInfo *cmd, SpanView<InstanceVertexData> vertexes);

	void applyNormalized(SpanView<InstanceVertexData> &vertexes, const CmdDeferred *cmd);
	void pushVertexData(Context &, const Command *c, const CmdVertexArray *cmd);
	void pushDeferred(Context &, const Command *c, const CmdDeferred *cmd);
	void pushParticleEmitter(Context &, const Command *c, const CmdParticleEmitter *cmd);

	void updatePathsDepth();

	void pushInitial(WriteTarget &writeTarget);
	void pushPlanVertexes(WriteTarget &writeTarget,
			Map<core::MaterialId, MaterialWritePlan> &writePlan);
	// `out` takes the general spans; `withShadows` is false for the overlay, which is drawn after
	// everything and casts nothing.
	void drawWritePlan(Context &, WriteTarget &writeTarget,
			Map<core::MaterialId, MaterialWritePlan> &writePlan, mem_std::Vector<VertexSpan> &out,
			bool withShadows);

	// `overlay` picks which bucket is sorted and where the spans go. Two separate sorts, because the
	// two sets are recorded by two separate passes.
	void drawWritePlanFlat(Context &, WriteTarget &writeTarget, bool overlay);

	// The depth a zPath draws at, from whichever of the two bands it belongs to
	float getPathDepth(SpanView<ZOrder>) const;

	// The overlay sub-plan for one zPath, created on first use
	Map<core::MaterialId, MaterialWritePlan> &acquireOverlayPlan(SpanView<ZOrder>);
	void pushAll(Context &, WriteTarget &writeTarget);

	// Nothing was submitted this frame: only the prologue quad has to be written.
	bool isEmpty() const { return globalWritePlan.vertexes == 0 || globalWritePlan.indexes == 0; }
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DVERTEXPLAN_H_ */
