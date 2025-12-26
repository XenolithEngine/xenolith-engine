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

#ifndef XENOLITH_APPLICATION_NODES_XLDYNAMICSTATESYSTEM_H_
#define XENOLITH_APPLICATION_NODES_XLDYNAMICSTATESYSTEM_H_

#include "XLSystem.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

enum class DynamicStateApplyMode : uint32_t {
	DoNotApply = 0,
	ApplyForNodesBelow = 1 << 0,
	ApplyForSelf = 1 << 1,
	ApplyForNodesAbove = 1 << 2,
	ApplyForAll = ApplyForNodesBelow | ApplyForSelf | ApplyForNodesAbove,
};

SP_DEFINE_ENUM_AS_MASK(DynamicStateApplyMode)

class SP_PUBLIC DynamicStateSystem : public System, protected FrameStateOwnerInterface {
public:
	virtual ~DynamicStateSystem() = default;

	virtual bool init() override;
	virtual bool init(DynamicStateApplyMode value);

	virtual void handleVisitBegin(FrameInfo &) override;
	virtual void handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>,
			NodeVisitFlags flags) override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags flags) override;
	virtual void handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>,
			NodeVisitFlags flags) override;
	virtual void handleVisitEnd(FrameInfo &) override;

	virtual DynamicStateApplyMode getStateApplyMode() const { return _applyMode; }
	virtual void setStateApplyMode(DynamicStateApplyMode value);

	virtual bool isIgnoreParentState() const { return _ignoreParentState; }
	virtual void setIgnoreParentState(bool);

	virtual StateId getCurrentStateId() const { return _currentStateId; }

	virtual void enableScissor(Padding outline = Padding());
	virtual void disableScissor();
	virtual bool isScissorEnabled() const { return _scissorEnabled; }

	virtual void setScissorOutlone(Padding value) { _scissorOutline = value; }
	virtual Padding getScissorOutline() const { return _scissorOutline; }

protected:
	using System::init;

	virtual DrawStateValues updateDynamicState(const DrawStateValues &) const;

	virtual void pushState(FrameInfo &);
	virtual void popState(FrameInfo &);

	virtual StateId rebuildState(FrameContextHandle &) override;

	DynamicStateApplyMode _applyMode = DynamicStateApplyMode::DoNotApply;

	bool _ignoreParentState = false;
	bool _scissorEnabled = false;
	Padding _scissorOutline;
	StateId _currentStateId = maxOf<StateId>();

	bool _isStateActive = false;
	bool _isStatePushed = false;
	bool _isStateValuesActual = false;
	DrawStateValues _stateValues;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLDYNAMICSTATESYSTEM_H_ */
