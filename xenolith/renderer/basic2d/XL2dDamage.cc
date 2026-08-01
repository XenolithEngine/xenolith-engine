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

#include "XL2dDamage.h"
#include "XL2dFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

// Beyond this, per-element tracking costs more than it saves and the frame is treated as fully
// damaged. A scene with this many distinct drawn elements repaints most of the screen anyway.
static constexpr size_t DamageEntryLimit = 4'096;

bool DamageCollector::init(const FrameContextHandle2d *input,
		const core::FrameConstraints &constraints) {
	_input = input;
	_constraints = constraints;
	_state = Rc<core::FrameDamageState>::alloc();
	if (!_state) {
		return false;
	}

	// DrawStateValues::scissor is in screen coordinates; under a pre-rotated surface those differ
	// from image coordinates, and re-deriving the mapping here would duplicate the backend's
	// rotateScissor. Skipping the intersection only widens the damage, which is always safe.
	_scissorUsable = core::getPureTransform(_constraints.transform)
			== core::SurfaceTransformFlags::Identity;
	return true;
}

void DamageCollector::escalate() { _state->full = true; }

Rect DamageCollector::toPixels(const Rect &clip) const {
	const float w = float(_constraints.extent.width);
	const float h = float(_constraints.extent.height);

	// the engine projection already bakes surface pre-rotation, so clip space maps to the
	// swapchain image directly; clip y = -1 is the top row
	const float x0 = (clip.origin.x * 0.5f + 0.5f) * w;
	const float x1 = ((clip.origin.x + clip.size.width) * 0.5f + 0.5f) * w;
	const float y0 = (clip.origin.y * 0.5f + 0.5f) * h;
	const float y1 = ((clip.origin.y + clip.size.height) * 0.5f + 0.5f) * h;

	return Rect(sprt::min(x0, x1), sprt::min(y0, y1), sprt::abs(x1 - x0), sprt::abs(y1 - y0));
}

uint32_t DamageCollector::makeSignature(const CmdInfo *cmd, const InstanceVertexData &iv) const {
	// Everything that changes pixels without changing the geometry. StateId itself must NOT be
	// hashed: it is an insertion index into the frame's state list and is not stable across frames.
	uint64_t acc = sprt::hash64(reinterpret_cast<const char *>(&cmd->material),
			sizeof(cmd->material));

	auto mix = [&](const void *ptr, size_t size) {
		acc = acc * 31 + sprt::hash64(reinterpret_cast<const char *>(ptr), size);
	};

	auto level = toInt(cmd->renderingLevel);
	mix(&level, sizeof(level));

	auto instances = uint32_t(iv.instances.size());
	mix(&instances, sizeof(instances));

	if (auto state = _input->getState(cmd->state)) {
		if (auto data = dynamic_cast<const StateData *>(state->data.get())) {
			mix(&data->outlineColor, sizeof(data->outlineColor));
			mix(&data->outlineOffset, sizeof(data->outlineOffset));
			if (data->gradient) {
				// a gradient change repaints without touching any VertexData
				mix(&data->gradient->identity.id, sizeof(uint64_t));
				mix(&data->gradient->identity.generation, sizeof(uint32_t));
			}
		}
	}

	return uint32_t(acc) ^ uint32_t(acc >> 32);
}

void DamageCollector::addInstances(const Command *command, const CmdInfo *cmd,
		const InstanceVertexData &iv) {
	if (_state->full) {
		return;
	}

	if (hasFlag(command->flags, CommandFlags::UnknownBounds)) {
		escalate();
		return;
	}

	if (!iv.data || iv.instances.empty()) {
		return;
	}

	// an explicit box from the producer wins over deriving it from the data
	Rect model = cmd->bounds;
	if (model.size.width <= 0.0f || model.size.height <= 0.0f) {
		if (!iv.data->getBounds(model)) {
			// a producer promised explicit bounds and did not refresh them - do not trust a
			// stale box, take the safe route
			escalate();
			return;
		}
	}

	if (model.size.width <= 0.0f || model.size.height <= 0.0f) {
		return;
	}

	// the instance transforms already carry view * model, so this lands in clip space
	Rect clip;
	bool first = true;
	for (auto &inst : iv.instances) {
		auto r = TransformRect(model, inst.transform);
		if (first) {
			clip = r;
			first = false;
		} else {
			clip.merge(r);
		}
	}

	auto box = toPixels(clip);

	if (_scissorUsable) {
		if (auto state = _input->getState(cmd->state)) {
			if (state->isScissorEnabled()) {
				auto &sc = state->scissor;
				const Rect scissor(float(sc.x), float(sc.y), float(sc.width), float(sc.height));
				if (!box.intersectsRect(scissor)) {
					return;
				}

				const float minX = sprt::max(box.getMinX(), scissor.getMinX());
				const float minY = sprt::max(box.getMinY(), scissor.getMinY());
				const float maxX = sprt::min(box.getMaxX(), scissor.getMaxX());
				const float maxY = sprt::min(box.getMaxY(), scissor.getMaxY());
				box = Rect(minX, minY, maxX - minX, maxY - minY);
			}
		}
	}

	if (box.size.width <= 0.0f || box.size.height <= 0.0f) {
		return;
	}

	if (hasFlag(command->flags, CommandFlags::AlwaysDirty)) {
		_state->alwaysDirty.emplace_back(box);
		return;
	}

	if (_state->entries.size() >= DamageEntryLimit) {
		escalate();
		return;
	}

	_state->entries.emplace_back(core::DamageEntry{iv.data->identity.id,
		iv.data->identity.generation, makeSignature(cmd, iv), box});
}

Rc<core::FrameDamageState> DamageCollector::finalize() {
	if (_state && !_state->full) {
		_state->sort();
	}
	return sp::move(_state);
}

} // namespace stappler::xenolith::basic2d
