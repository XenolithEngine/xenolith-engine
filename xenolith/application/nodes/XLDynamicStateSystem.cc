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

#include "XLDynamicStateSystem.h"
#include "XLNode.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool DynamicStateSystem::init() { return init(DynamicStateApplyMode::DoNotApply); }

bool DynamicStateSystem::init(DynamicStateApplyMode value) {
	_applyMode = DynamicStateApplyMode::DoNotApply;
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents;

	setStateApplyMode(value);
	return System::init();
}

void DynamicStateSystem::handleVisitBegin(FrameInfo &frameInfo) { _isStateValuesActual = false; }

void DynamicStateSystem::handleVisitNodesBelow(FrameInfo &frameInfo, SpanView<Rc<Node>> nodes,
		NodeVisitFlags flags) {
	if (!nodes.empty() && hasFlag(_applyMode, DynamicStateApplyMode::ApplyForNodesBelow)) {
		pushState(frameInfo);
	}
}

void DynamicStateSystem::handleVisitSelf(FrameInfo &frameInfo, Node *, NodeVisitFlags flags) {
	if (hasFlag(_applyMode, DynamicStateApplyMode::ApplyForSelf)) {
		pushState(frameInfo);
	} else {
		popState(frameInfo);
	}
}

void DynamicStateSystem::handleVisitNodesAbove(FrameInfo &frameInfo, SpanView<Rc<Node>> nodes,
		NodeVisitFlags flags) {
	if (!nodes.empty() && hasFlag(_applyMode, DynamicStateApplyMode::ApplyForNodesAbove)) {
		pushState(frameInfo);
	} else {
		popState(frameInfo);
	}
}

void DynamicStateSystem::handleVisitEnd(FrameInfo &frameInfo) {
	popState(frameInfo);
	_isStateValuesActual = false;
}

void DynamicStateSystem::setStateApplyMode(DynamicStateApplyMode value) {
	if (value != _applyMode) {
		_applyMode = value;
		if (_applyMode != DynamicStateApplyMode::DoNotApply) {
			setSystemFlags(SystemFlags::HandleVisitSelf | SystemFlags::HandleVisitControl
					| SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);
		} else {
			setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);
		}
	}
}

void DynamicStateSystem::setIgnoreParentState(bool val) { _ignoreParentState = val; }

void DynamicStateSystem::enableScissor(Padding outline) {
	_scissorEnabled = true;
	_scissorOutline = outline;
}

void DynamicStateSystem::disableScissor() { _scissorEnabled = false; }

DrawStateValues DynamicStateSystem::updateDynamicState(const DrawStateValues &values) const {
	auto getViewRect = [&, this] {
		auto contentSize = _owner->getContentSize();
		Vec2 bottomLeft =
				_owner->convertToWorldSpace(Vec2(-_scissorOutline.left, -_scissorOutline.bottom));
		Vec2 topRight = _owner->convertToWorldSpace(Vec2(contentSize.width + _scissorOutline.right,
				contentSize.height + _scissorOutline.top));

		if (bottomLeft.x > topRight.x) {
			float b = topRight.x;
			topRight.x = bottomLeft.x;
			bottomLeft.x = b;
		}

		if (bottomLeft.y > topRight.y) {
			float b = topRight.y;
			topRight.y = bottomLeft.y;
			bottomLeft.y = b;
		}

		const float x0 = sprt::max(roundf(bottomLeft.x), 0.0f);
		const float y0 = sprt::max(roundf(bottomLeft.y), 0.0f);
		const float x1 = sprt::max(roundf(topRight.x), x0);
		const float y1 = sprt::max(roundf(topRight.y), y0);

		return URect{uint32_t(x0), uint32_t(y0), uint32_t(x1 - x0), uint32_t(y1 - y0)};
	};


	DrawStateValues ret(_ignoreParentState ? DrawStateValues() : values);
	if (_scissorEnabled) {
		auto viewRect = getViewRect();
		if ((ret.enabled & core::DynamicState::Scissor) == core::DynamicState::None) {
			ret.enabled |= core::DynamicState::Scissor;
			ret.scissor = viewRect;
		} else {
			// A nested scissor is the INTERSECTION of the two boxes, so the extents are derived
			// from the clamped EDGES: min(width) alone would let a child that starts inside the
			// parent run past its far edge, and boxes that do not overlap at all collapse to an
			// empty rect rather than to the parent's.
			const uint32_t minX = sprt::max(ret.scissor.x, viewRect.x);
			const uint32_t minY = sprt::max(ret.scissor.y, viewRect.y);
			const uint32_t maxX =
					sprt::min(ret.scissor.x + ret.scissor.width, viewRect.x + viewRect.width);
			const uint32_t maxY =
					sprt::min(ret.scissor.y + ret.scissor.height, viewRect.y + viewRect.height);

			ret.scissor = URect{
				minX,
				minY,
				(maxX > minX) ? maxX - minX : 0U,
				(maxY > minY) ? maxY - minY : 0U,
			};
		}
	}
	return ret;
}

void DynamicStateSystem::pushState(FrameInfo &info) {
	if (_isStateActive) {
		return;
	}

	auto &ctx = info.contextStack.back();
	auto prevStateId = ctx->getCurrentState();

	if (!_isStateValuesActual) {
		_currentStateId = rebuildState(*ctx);
	}

	if (_currentStateId == prevStateId) {
		// no need to enable anything
		_isStateActive = true;
		return;
	}

	ctx->stateStack.emplace_back(_currentStateId, static_cast<FrameStateOwnerInterface *>(this));
	_isStateActive = true;
	_isStatePushed = true;
}

void DynamicStateSystem::popState(FrameInfo &info) {
	if (!_isStateActive) {
		return;
	}

	if (!_isStatePushed) {
		_isStateActive = false;
		return;
	}

	auto &ctx = info.contextStack.back();

	if (ctx->stateStack.back().second == this) {
		ctx->stateStack.pop_back();
	} else {
		Vector<sprt::pair<StateId, FrameStateOwnerInterface *>> owners;
		while (!ctx->stateStack.empty() && ctx->stateStack.back().second != this) {
			owners.emplace_back(ctx->stateStack.back());
			ctx->stateStack.pop_back();
		}

		if (ctx->stateStack.back().second == this) {
			ctx->stateStack.pop_back();
		}

		sprt::reverse(owners.begin(), owners.end());

		for (auto &it : owners) {
			if (it.second) {
				ctx->stateStack.emplace_back(it.second->rebuildState(*ctx), it.second);
			} else {
				ctx->stateStack.emplace_back(it);
			}
		}
	}
	_isStatePushed = false;
	_isStateActive = false;
}

StateId DynamicStateSystem::rebuildState(FrameContextHandle &ctx) {
	auto prevStateId = ctx.getCurrentState();
	auto currentState = ctx.getState(prevStateId);

	_stateValues = updateDynamicState(currentState ? *currentState : DrawStateValues());
	_isStateValuesActual = true;

	if (_stateValues.enabled == core::DynamicState::None) {
		_currentStateId = maxOf<StateId>();
	} else {
		_currentStateId = ctx.addState(_stateValues);
	}
	return _currentStateId;
}

} // namespace stappler::xenolith
