/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DDAMAGE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DDAMAGE_H_

#include "XL2dCommandList.h"
#include "XLCoreFrameDamage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

// Accumulates a core::FrameDamageState while a backend walks the frame's command list.
//
// It is deliberately not a walker of its own: the backends already visit every command to build
// their write plans, and deferred results are already resolved there, so damage is collected by
// the same pass - no second traversal, no second acquireResult, and the bounds of a deferred
// command are exact instead of unknown.
class SP_PUBLIC DamageCollector {
public:
	bool init(const FrameContextHandle2d *, const core::FrameConstraints &);

	// One entry per InstanceVertexData, with the instances already carrying their final
	// (view * model) transforms - i.e. mapping model space straight to clip space.
	void addInstances(const Command *, const CmdInfo *, const InstanceVertexData &);

	// The element cannot be bounded (a GPU-simulated particle system): escalate the whole frame.
	void escalate();

	Rc<core::FrameDamageState> finalize();

protected:
	// clip space -> swapchain-image pixels, y-down
	Rect toPixels(const Rect &clip) const;

	uint32_t makeSignature(const CmdInfo *, const InstanceVertexData &) const;

	const FrameContextHandle2d *_input = nullptr;
	core::FrameConstraints _constraints;
	Rc<core::FrameDamageState> _state;

	// scissor is expressed in screen coordinates; only intersect when they coincide with image
	// coordinates, otherwise leave the (larger, still correct) unclipped box
	bool _scissorUsable = true;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DDAMAGE_H_ */
