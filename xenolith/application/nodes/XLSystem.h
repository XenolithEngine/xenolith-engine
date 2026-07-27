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
#include "XLComponent.h"

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
	HandleComponents = 1 << 5, // Components (owner's own components dirty)
	HandleMeasure = 1 << 7, // Measure/LayoutApplied (content measurement protocol)
	HandleChildNodeEvents = 1 << 8, // descendant ContentSizeDirty (via frame stack)
	HandleAncestorComponents = 1 << 9, // system wants ancestor (parent) handleComponentsDirty
	HandleLayoutChildren =
			1 << 10, // LayoutChildren (position/size children, own size + order fixed)
	HandleChildMeasure = 1 << 11, // descendant Measure (via frame stack)
	HandleChildLayoutChildren = 1 << 12, // descendant LayoutChildren (via frame stack)
	HandleChildComponents = 1 << 13, // descendant ComponentsDirty (via frame stack)

	// This flags reflects what kind of Node's events system can handle
	// To work effectively, set flags you actually needed
	EventFlagMask = HandleOwnerEvents | HandleSceneEvents | HandleNodeEvents | HandleVisitSelf
			| HandleVisitControl | HandleComponents | HandleMeasure | HandleChildNodeEvents
			| HandleAncestorComponents | HandleLayoutChildren | HandleChildMeasure
			| HandleChildLayoutChildren | HandleChildComponents,

	// When this flag is set and FrameTag != InvalidTag, system will be added to frame stack by it's owner.
	// It means, that descendant nodes can access this system with FrameInfo::systemStack and FrameTag.
	// Publishing on the stack is also what lets a descendant deliver its own events back UP to this
	// system: pair AddToFrameStack with HandleChild{NodeEvents,Measure,LayoutChildren} and the nearest
	// opted-in ancestor on the stack receives the descendant's ContentSize / Measure / LayoutChildren
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
	// Default dispatch priority for a system. Systems are dispatched in ascending
	// priority order (lower priority receives events earlier). The default sits at the
	// middle of the range so systems can be ordered both before and after it
	static constexpr uint32_t DefaultPriority = maxOf<uint32_t>() / 2;

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
	virtual void handleComponentsDirty(const ComponentMask &);
	virtual void handleTransformDirty(const Mat4 &);
	virtual void handleReorderChildDirty();
	// Position the owner within its parent (parent's content size changed)
	virtual void handleLayoutInParent(Node *);

	// Content measurement protocol (requires SystemFlags::HandleMeasure).
	// Return true and fill `result` with the owner's natural content size
	// under the given constraints; must not commit any node state
	virtual bool handleMeasure(const MeasureConstraints &, Size2 &result);

	// A layout engine has committed `size` to the owner of this system
	// (requires SystemFlags::HandleMeasure); adapt content synchronously
	virtual void handleLayoutApplied(const Size2 &);

	// A descendant node changed its content size, delivered during the descendant's visit via the
	// frame stack (requires SystemFlags::HandleChildNodeEvents + AddToFrameStack + a valid FrameTag).
	// `child` is the nearest descendant whose size changed; this system is the nearest opted-in ancestor
	virtual void handleChildContentSizeDirty(Node *child);

	// A descendant node ran its measure phase, delivered via the frame stack
	// (requires SystemFlags::HandleChildMeasure + AddToFrameStack + a valid FrameTag)
	virtual void handleChildMeasure(Node *child);

	// A descendant node laid out its children, delivered via the frame stack
	// (requires SystemFlags::HandleChildLayoutChildren + AddToFrameStack + a valid FrameTag)
	virtual void handleChildLayoutChildren(Node *child);

	// A descendant node's own components changed, delivered during the descendant's visit via the
	// frame stack (requires SystemFlags::HandleChildComponents + AddToFrameStack + a valid FrameTag).
	// `child` is the nearest descendant whose components went dirty; this system is the nearest
	// opted-in ancestor. Unlike HandleAncestorComponents (which pushes an ancestor's change DOWN to
	// descendant systems), this bubbles a descendant's change UP - e.g. so a subtree-wide style
	// resolver can re-resolve a node whose interactive :hover/:focus/:active state just flipped
	virtual void handleChildComponentsDirty(Node *child, const ComponentMask &);

	// Lay out the owner's children (requires SystemFlags::HandleLayoutChildren).
	// Runs after child reorder with the owner's own size and child order fixed;
	// this is where a layout engine positions and sizes children
	virtual void handleLayoutChildren();

	virtual bool isRunning() const;

	virtual bool isEnabled() const;
	virtual void setEnabled(bool b);

	virtual void setSystemFlags(SystemFlags);
	virtual SystemFlags getSystemFlags() const { return _systemFlags; }

	// Dispatch priority within the owner's system list (lower is dispatched earlier).
	// The priority set before adding is the system's default, used unless an explicit one
	// is passed to Node::addSystem. Changing it on an already-added system re-sorts it in
	// the owner's list immediately (live update)
	virtual void setSystemPriority(uint32_t);
	virtual uint32_t getSystemPriority() const { return _systemPriority; }

	bool isScheduled() const;
	void scheduleUpdate();
	void unscheduleUpdate();

	Node *getOwner() const { return _owner; }

	void setFrameTag(uint64_t);
	uint64_t getFrameTag() const { return _frameTag; }

	// Whether this system currently contributes to its owner's ancestor-components
	// listener counter (used by Node's system-removal paths to release the contribution)
	bool isAncestorComponentsCounted() const { return _ancestorComponentsCounted; }
	void clearAncestorComponentsCounted() { _ancestorComponentsCounted = false; }

protected:
	// Reconcile this system's contribution to the owner's ancestor-components listener
	// counter with its current enabled state and flags. Idempotent.
	void reconcileAncestorComponents();

	Node *_owner = nullptr;
	bool _enabled = true;
	bool _running = false;
	bool _scheduled = false;
	bool _ancestorComponentsCounted = false;
	uint32_t _systemPriority = DefaultPriority;
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
	virtual void handleComponentsDirty(const ComponentMask &) override;
	virtual void handleTransformDirty(const Mat4 &) override;
	virtual void handleReorderChildDirty() override;
	virtual void handleLayoutInParent(Node *) override;

	virtual bool handleMeasure(const MeasureConstraints &, Size2 &result) override;
	virtual void handleLayoutApplied(const Size2 &) override;
	virtual void handleLayoutChildren() override;

	virtual void handleChildContentSizeDirty(Node *child) override;
	virtual void handleChildMeasure(Node *child) override;
	virtual void handleChildLayoutChildren(Node *child) override;
	virtual void handleChildComponentsDirty(Node *child, const ComponentMask &) override;

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

	virtual void setComponentsDirtyCallback(
			Function<void(CallbackSystem *, const ComponentMask &mask)> &&);
	virtual auto getComponentsDirtyCallback()
			-> const Function<void(CallbackSystem *, const ComponentMask &mask)> & {
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

	// feeds handleLayoutInParent (positions the owner within its parent)
	virtual void setLayoutCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getLayoutCallback() -> const Function<void(CallbackSystem *, Node *)> & {
		return _handleLayout;
	}

	// feeds handleLayoutChildren (lay out the owner's children after reorder)
	virtual void setLayoutChildrenCallback(Function<void(CallbackSystem *)> &&);
	virtual auto getLayoutChildrenCallback() -> const Function<void(CallbackSystem *)> & {
		return _handleLayoutChildren;
	}

	// content measurement protocol (see System::handleMeasure): return true and
	// fill the Size2 to answer, return false to fall through to other systems
	virtual void setMeasureCallback(
			Function<bool(CallbackSystem *, const MeasureConstraints &, Size2 &)> &&);
	virtual auto getMeasureCallback()
			-> const Function<bool(CallbackSystem *, const MeasureConstraints &, Size2 &)> & {
		return _handleMeasure;
	}

	// a layout engine committed the size to the owner (see System::handleLayoutApplied)
	virtual void setLayoutAppliedCallback(Function<void(CallbackSystem *, const Size2 &)> &&);
	virtual auto getLayoutAppliedCallback()
			-> const Function<void(CallbackSystem *, const Size2 &)> & {
		return _handleLayoutApplied;
	}

	// descendant content-size event (see System::handleChildContentSizeDirty); to actually receive
	// it the system must also carry a FrameTag + SystemFlags::AddToFrameStack
	virtual void setChildContentSizeDirtyCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getChildContentSizeDirtyCallback()
			-> const Function<void(CallbackSystem *, Node *)> & {
		return _handleChildContentSizeDirty;
	}

	// descendant measure event (see System::handleChildMeasure); needs FrameTag + AddToFrameStack
	virtual void setChildMeasureCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getChildMeasureCallback() -> const Function<void(CallbackSystem *, Node *)> & {
		return _handleChildMeasure;
	}

	// descendant layout-children event (see System::handleChildLayoutChildren); needs FrameTag +
	// AddToFrameStack
	virtual void setChildLayoutChildrenCallback(Function<void(CallbackSystem *, Node *)> &&);
	virtual auto getChildLayoutChildrenCallback()
			-> const Function<void(CallbackSystem *, Node *)> & {
		return _handleChildLayoutChildren;
	}

	// descendant components-dirty event (see System::handleChildComponentsDirty); needs FrameTag +
	// AddToFrameStack
	virtual void setChildComponentsDirtyCallback(
			Function<void(CallbackSystem *, Node *, const ComponentMask &)> &&);
	virtual auto getChildComponentsDirtyCallback()
			-> const Function<void(CallbackSystem *, Node *, const ComponentMask &)> & {
		return _handleChildComponents;
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
	Function<void(CallbackSystem *, const ComponentMask &mask)> _handleComponentsDirty;
	Function<void(CallbackSystem *, const Mat4 &)> _handleTransformDirty;
	Function<void(CallbackSystem *)> _handleReorderChildDirty;
	Function<void(CallbackSystem *, Node *)> _handleLayout;
	Function<bool(CallbackSystem *, const MeasureConstraints &, Size2 &)> _handleMeasure;
	Function<void(CallbackSystem *, const Size2 &)> _handleLayoutApplied;
	Function<void(CallbackSystem *)> _handleLayoutChildren;
	Function<void(CallbackSystem *, Node *)> _handleChildContentSizeDirty;
	Function<void(CallbackSystem *, Node *)> _handleChildMeasure;
	Function<void(CallbackSystem *, Node *)> _handleChildLayoutChildren;
	Function<void(CallbackSystem *, Node *, const ComponentMask &)> _handleChildComponents;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLSYSTEM_H_ */
