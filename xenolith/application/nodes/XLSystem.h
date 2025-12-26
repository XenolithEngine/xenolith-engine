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

#ifndef XENOLITH_APPLICATION_NODES_XLSYSTEM_H_
#define XENOLITH_APPLICATION_NODES_XLSYSTEM_H_

#include "XLNodeInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct FrameInfo;
class Node;
class Scene;

enum class SystemFlags : uint32_t {
	None,

	HandleOwnerEvents = 1 << 0, // Added/Removed
	HandleSceneEvents = 1 << 1, // Enter/Exit
	HandleNodeEvents = 1 << 2, // ContentSize/Transform/Reorder
	HandleVisitSelf = 1 << 3, // VisitSelf
	HandleVisitControl = 1 << 4, // VisitBegin/VisitNodesBelow/VisitNodesAbove/VisitEnd
	HandleComponents = 1 << 5, // Components

	// This flags reflects what kind of Node's events system can handle
	// To work effectively, set flags you actually needed
	EventFlagMask = HandleOwnerEvents | HandleSceneEvents | HandleNodeEvents | HandleVisitSelf
			| HandleVisitControl | HandleComponents,

	// When this flag is set and FrameTag != InvalidTag, system will be added to frame stack by it's owner.
	// It means, that child nodes can access this system with FrameInfo::systemStack and FrameTag
	AddToFrameStack = 1 << 6,

	Default = HandleOwnerEvents | HandleSceneEvents | HandleNodeEvents | HandleVisitSelf
};

SP_DEFINE_ENUM_AS_MASK(SystemFlags)

/** System is the way to implement or change Node's behavior on scene

System can handle all key node's events and modify basic node's paramenters in response

In most cases, you should consider to implement some System instead of subclassing Node

To accees some additional data from Node, consider to implement Component subclass

Regular System examples is:
- InputListener
- EventListener
- Some layout engines like ScrollContrller
*/

class SP_PUBLIC System : public Ref {
public:
	static uint64_t GetNextSystemId();

	virtual ~System() = default;

	System();

	virtual bool init();

	virtual void handleAdded(Node *owner);
	virtual void handleRemoved();

	virtual void handleEnter(Scene *);
	virtual void handleExit();

	virtual void handleVisitBegin(FrameInfo &);
	virtual void handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags);
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags);
	virtual void handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags);
	virtual void handleVisitEnd(FrameInfo &);

	virtual void update(const UpdateTime &time);

	virtual void handleContentSizeDirty();
	virtual void handleComponentsDirty();
	virtual void handleTransformDirty(const Mat4 &);
	virtual void handleReorderChildDirty();
	virtual void handleLayout(Node *);

	virtual bool isRunning() const;

	virtual bool isEnabled() const;
	virtual void setEnabled(bool b);

	virtual void setSystemFlags(SystemFlags);
	virtual SystemFlags getSystemFlags() const { return _systemFlags; }

	bool isScheduled() const;
	void scheduleUpdate();
	void unscheduleUpdate();

	Node *getOwner() const { return _owner; }

	void setFrameTag(uint64_t);
	uint64_t getFrameTag() const { return _frameTag; }

protected:
	Node *_owner = nullptr;
	bool _enabled = true;
	bool _running = false;
	bool _scheduled = false;
	uint64_t _frameTag = InvalidTag;
	SystemFlags _systemFlags = SystemFlags::Default;
};

class SP_PUBLIC CallbackSystem : public System {
public:
	virtual ~CallbackSystem() = default;

	CallbackSystem();

	virtual void handleAdded(Node *owner) override;
	virtual void handleRemoved() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleVisitBegin(FrameInfo &) override;
	virtual void handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>,
			NodeVisitFlags flags) override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags) override;
	virtual void handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>,
			NodeVisitFlags flags) override;
	virtual void handleVisitEnd(FrameInfo &) override;

	virtual void update(const UpdateTime &time) override;

	virtual void handleContentSizeDirty() override;
	virtual void handleComponentsDirty() override;
	virtual void handleTransformDirty(const Mat4 &) override;
	virtual void handleReorderChildDirty() override;
	virtual void handleLayout(Node *) override;

	virtual void setUserdata(Rc<Ref> &&d) { _userdata = move(d); }
	virtual Ref *getUserdata() const { return _userdata; }

	virtual void setAddedCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getAddedCallback() -> const Function<void(CallbackSystem *, Node *)> & {
		return _handleAdded;
	}

	virtual void setRemovedCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getRemovedCallback() -> const Function<void(CallbackSystem *, Node *)> & {
		return _handleRemoved;
	}

	virtual void setEnterCallback(Function<void(CallbackSystem *, Scene *)> &&);
	virtual auto getEnterCallback() -> const Function<void(CallbackSystem *, Scene *)> & {
		return _handleEnter;
	}

	virtual void setExitCallback(Function<void(CallbackSystem *)> &&);
	virtual auto getExitCallback() -> const Function<void(CallbackSystem *)> & {
		return _handleExit;
	}

	virtual void setVisitBeginCallback(Function<void(CallbackSystem *, FrameInfo &)> &&);
	virtual auto getVisitBeginCallback() -> const Function<void(CallbackSystem *, FrameInfo &)> & {
		return _handleVisitBegin;
	}

	virtual void setVisitNodesBelowCallback(
			Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags)> &&);
	virtual auto getVisitNodesBelowCallback() -> const
			Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags)> & {
		return _handleVisitNodesBelow;
	}

	virtual void setVisitSelfCallback(
			Function<void(CallbackSystem *, FrameInfo &, Node *, NodeVisitFlags)> &&);
	virtual auto getVisitSelfCallback()
			-> const Function<void(CallbackSystem *, FrameInfo &, Node *, NodeVisitFlags)> & {
		return _handleVisitSelf;
	}

	virtual void setVisitNodesAboveCallback(
			Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags)> &&);
	virtual auto getVisitNodesAboveCallback() -> const
			Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags)> & {
		return _handleVisitNodesAbove;
	}

	virtual void setVisitEndCallback(Function<void(CallbackSystem *, FrameInfo &)> &&);
	virtual auto getVisitEndCallback() -> const Function<void(CallbackSystem *, FrameInfo &)> & {
		return _handleVisitEnd;
	}

	virtual void setUpdateCallback(Function<void(CallbackSystem *, const UpdateTime &)> &&);
	virtual auto getUpdateCallback()
			-> const Function<void(CallbackSystem *, const UpdateTime &)> & {
		return _handleUpdate;
	}

	virtual void setContentSizeDirtyCallback(Function<void(CallbackSystem *)> &&);
	virtual auto getContentSizeDirtyCallback() -> const Function<void(CallbackSystem *)> & {
		return _handleContentSizeDirty;
	}

	virtual void setComponentsDirtyCallback(Function<void(CallbackSystem *)> &&);
	virtual auto getComponentsDirtyCallback() -> const Function<void(CallbackSystem *)> & {
		return _handleComponentsDirty;
	}

	virtual void setTransformDirtyCallback(Function<void(CallbackSystem *, const Mat4 &)> &&);
	virtual auto getTransformDirtyCallback()
			-> const Function<void(CallbackSystem *, const Mat4 &)> & {
		return _handleTransformDirty;
	}

	virtual void setReorderChildDirtyCallback(Function<void(CallbackSystem *)> &&);
	virtual auto getReorderChildDirtyCallback() -> const Function<void(CallbackSystem *)> & {
		return _handleReorderChildDirty;
	}

	virtual void setLayoutCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getLayoutCallback() -> const Function<void(CallbackSystem *, Node *)> & {
		return _handleLayout;
	}

protected:
	virtual void updateFlags();

	Rc<Ref> _userdata;

	Function<void(CallbackSystem *, Node *)> _handleAdded;
	Function<void(CallbackSystem *, Node *)> _handleRemoved;
	Function<void(CallbackSystem *, Scene *)> _handleEnter;
	Function<void(CallbackSystem *)> _handleExit;
	Function<void(CallbackSystem *, FrameInfo &)> _handleVisitBegin;
	Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags)>
			_handleVisitNodesBelow;
	Function<void(CallbackSystem *, FrameInfo &, Node *, NodeVisitFlags flags)> _handleVisitSelf;
	Function<void(CallbackSystem *, FrameInfo &, SpanView<Rc<Node>>, NodeVisitFlags flags)>
			_handleVisitNodesAbove;
	Function<void(CallbackSystem *, FrameInfo &)> _handleVisitEnd;
	Function<void(CallbackSystem *, const UpdateTime &time)> _handleUpdate;
	Function<void(CallbackSystem *)> _handleContentSizeDirty;
	Function<void(CallbackSystem *)> _handleComponentsDirty;
	Function<void(CallbackSystem *, const Mat4 &)> _handleTransformDirty;
	Function<void(CallbackSystem *)> _handleReorderChildDirty;
	Function<void(CallbackSystem *, Node *)> _handleLayout;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLSYSTEM_H_ */
