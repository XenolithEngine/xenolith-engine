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

#include "XLSystem.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLScheduler.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t System::GetNextSystemId() {
	static std::atomic<uint64_t> s_value = 1;
	return s_value.fetch_add(1);
}

System::System() : _owner(nullptr), _enabled(true) { }

bool System::init() { return true; }

void System::handleAdded(Node *owner) { _owner = owner; }
void System::handleRemoved() { _owner = nullptr; }

void System::handleEnter(Scene *sc) {
	_running = true;
	if (_scheduled) {
		sc->getDirector()->getScheduler()->scheduleUpdate(this, 0, false);
	}
}

void System::handleExit() {
	if (_scheduled) {
		unscheduleUpdate();
		_scheduled = true; // -re-enable after restart;
	}
	_running = false;
}

void System::handleVisitBegin(FrameInfo &) { }

void System::handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags) { }

void System::handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags) { }

void System::handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags) { }

void System::handleVisitEnd(FrameInfo &) { }

void System::update(const UpdateTime &time) { }

void System::handleContentSizeDirty() { }
void System::handleComponentsDirty() { }
void System::handleTransformDirty(const Mat4 &) { }
void System::handleReorderChildDirty() { }
void System::handleLayout(Node *parent) { }

bool System::isRunning() const { return _running; }

bool System::isEnabled() const { return _enabled; }

void System::setEnabled(bool b) { _enabled = b; }

void System::setSystemFlags(SystemFlags flags) { _systemFlags = flags; }

bool System::isScheduled() const { return _scheduled; }

void System::scheduleUpdate() {
	if (!_scheduled) {
		_scheduled = true;
		if (_running) {
			_owner->getScheduler()->scheduleUpdate(this, 0, false);
		}
	}
}

void System::unscheduleUpdate() {
	if (_scheduled) {
		if (_running) {
			_owner->getScheduler()->unschedule(this);
		}
		_scheduled = false;
	}
}

void System::setFrameTag(uint64_t tag) { _frameTag = tag; }

CallbackSystem::CallbackSystem() { _systemFlags = SystemFlags::None; }

void CallbackSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	if (_handleAdded) {
		_handleAdded(this, owner);
	}
}

void CallbackSystem::handleRemoved() {
	if (_handleRemoved) {
		_handleRemoved(this, _owner);
	}
	System::handleRemoved();
}

void CallbackSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	if (_handleEnter) {
		_handleEnter(this, scene);
	}
}

void CallbackSystem::handleExit() {
	System::handleExit();

	if (_handleExit) {
		_handleExit(this);
	}
}

void CallbackSystem::handleVisitBegin(FrameInfo &info) {
	System::handleVisitBegin(info);

	if (_handleVisitBegin) {
		_handleVisitBegin(this, info);
	}
}

void CallbackSystem::handleVisitNodesBelow(FrameInfo &info, SpanView<Rc<Node>> nodes,
		NodeVisitFlags flags) {
	System::handleVisitNodesBelow(info, nodes, flags);

	if (_handleVisitNodesBelow) {
		_handleVisitNodesBelow(this, info, nodes, flags);
	}
}

void CallbackSystem::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);

	if (_handleVisitSelf) {
		_handleVisitSelf(this, info, node, flags);
	}
}

void CallbackSystem::handleVisitNodesAbove(FrameInfo &info, SpanView<Rc<Node>> nodes,
		NodeVisitFlags flags) {
	System::handleVisitNodesAbove(info, nodes, flags);

	if (_handleVisitNodesAbove) {
		_handleVisitNodesAbove(this, info, nodes, flags);
	}
}

void CallbackSystem::handleVisitEnd(FrameInfo &info) {
	System::handleVisitEnd(info);

	if (_handleVisitEnd) {
		_handleVisitEnd(this, info);
	}
}

void CallbackSystem::update(const UpdateTime &time) {
	System::update(time);

	if (_handleUpdate) {
		_handleUpdate(this, time);
	}
}

void CallbackSystem::handleContentSizeDirty() {
	System::handleContentSizeDirty();

	if (_handleContentSizeDirty) {
		_handleContentSizeDirty(this);
	}
}

void CallbackSystem::handleComponentsDirty() {
	System::handleComponentsDirty();

	if (_handleComponentsDirty) {
		_handleComponentsDirty(this);
	}
}

void CallbackSystem::handleTransformDirty(const Mat4 &t) {
	System::handleTransformDirty(t);

	if (_handleTransformDirty) {
		_handleTransformDirty(this, t);
	}
}

void CallbackSystem::handleReorderChildDirty() {
	System::handleReorderChildDirty();

	if (_handleReorderChildDirty) {
		_handleReorderChildDirty(this);
	}
}

void CallbackSystem::handleLayout(Node *parent) {
	System::handleLayout(parent);

	if (_handleLayout) {
		_handleLayout(this, parent);
	}
}

void CallbackSystem::setAddedCallback(Function<void(CallbackSystem *, Node *)> &&cb) {
	_handleAdded = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setRemovedCallback(Function<void(CallbackSystem *, Node *)> &&cb) {
	_handleRemoved = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setEnterCallback(Function<void(CallbackSystem *, Scene *)> &&cb) {
	_handleEnter = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setExitCallback(Function<void(CallbackSystem *)> &&cb) {
	_handleExit = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setVisitBeginCallback(Function<void(CallbackSystem *, FrameInfo &)> &&cb) {
	_handleVisitBegin = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setVisitNodesBelowCallback(
		Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags)>
				&&cb) {
	_handleVisitNodesBelow = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setVisitSelfCallback(
		Function<void(CallbackSystem *, FrameInfo &, Node *, NodeVisitFlags flags)> &&cb) {
	_handleVisitSelf = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setVisitNodesAboveCallback(
		Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags)>
				&&cb) {
	_handleVisitNodesAbove = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setVisitEndCallback(Function<void(CallbackSystem *, FrameInfo &)> &&cb) {
	_handleVisitEnd = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setUpdateCallback(Function<void(CallbackSystem *, const UpdateTime &)> &&cb) {
	_handleUpdate = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setContentSizeDirtyCallback(Function<void(CallbackSystem *)> &&cb) {
	_handleContentSizeDirty = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setComponentsDirtyCallback(Function<void(CallbackSystem *)> &&cb) {
	_handleComponentsDirty = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setTransformDirtyCallback(
		Function<void(CallbackSystem *, const Mat4 &)> &&cb) {
	_handleTransformDirty = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setReorderChildDirtyCallback(Function<void(CallbackSystem *)> &&cb) {
	_handleReorderChildDirty = sp::move(cb);
	updateFlags();
}

void CallbackSystem::setLayoutCallback(Function<void(CallbackSystem *, Node *)> &&cb) {
	_handleLayout = sp::move(cb);
	updateFlags();
}

void CallbackSystem::updateFlags() {
	if (_handleAdded || _handleRemoved) {
		_systemFlags |= SystemFlags::HandleOwnerEvents;
	} else {
		_systemFlags &= ~SystemFlags::HandleOwnerEvents;
	}

	if (_handleEnter || _handleExit) {
		_systemFlags |= SystemFlags::HandleSceneEvents;
	} else {
		_systemFlags &= ~SystemFlags::HandleSceneEvents;
	}

	if (_handleContentSizeDirty || _handleReorderChildDirty || _handleTransformDirty
			|| _handleLayout) {
		_systemFlags |= SystemFlags::HandleNodeEvents;
	} else {
		_systemFlags &= ~SystemFlags::HandleNodeEvents;
	}

	if (_handleVisitSelf) {
		_systemFlags |= SystemFlags::HandleVisitSelf;
	} else {
		_systemFlags &= ~SystemFlags::HandleVisitSelf;
	}

	if (_handleVisitBegin || _handleVisitNodesBelow || _handleVisitNodesAbove || _handleVisitEnd) {
		_systemFlags |= SystemFlags::HandleVisitControl;
	} else {
		_systemFlags &= ~SystemFlags::HandleVisitControl;
	}

	if (_handleComponentsDirty) {
		_systemFlags |= SystemFlags::HandleComponents;
	} else {
		_systemFlags &= ~SystemFlags::HandleComponents;
	}

	if (_handleUpdate) {
		scheduleUpdate();
	} else {
		unscheduleUpdate();
	}
}

} // namespace stappler::xenolith
