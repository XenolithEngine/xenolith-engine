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

#ifndef XENOLITH_APPLICATION_NODES_XLSCENE_H_
#define XENOLITH_APPLICATION_NODES_XLSCENE_H_

#include "XLNode.h"
#include "XLCoreResource.h"
#include "XLCoreQueue.h"
#include "XLCoreAttachment.h"
#include "XLCoreMaterial.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameRequestProxy.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;
class SceneContent;

class SP_PUBLIC Scene : public Node {
public:
	using Queue = core::Queue;
	using FrameRequest = core::FrameRequest;
	using FrameQueue = core::FrameQueue;
	using FrameHandle = core::FrameHandle;

	virtual ~Scene();

	virtual bool init(Queue::Builder &&, const core::FrameConstraints &);

	// Adopt an already-built (usually already-compiled) queue instead of building one.
	//
	// The scene does NOT own it: whoever owns the queue also owns the registration of its internal
	// resource in the ResourceCache. That matters - ResourceCache entries are keyed by name with no
	// refcount, so if two scenes sharing a queue each registered and unregistered it, the first one
	// to finish would pull the resource out from under the second.
	virtual bool init(Rc<Queue> &&, const core::FrameConstraints &);

	virtual void renderRequest(const Rc<core::FrameRequestProxy> &, sprt::PoolRef *pool);
	virtual void render(FrameInfo &info);

	// The frame currently being visited, or null outside render().
	FrameInfo *getFrameInfo() const { return _frameInfo; }

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleContentSizeDirty() override;

	const Rc<Queue> &getQueue() const { return _queue; }
	Director *getDirector() const { return _director; }

	virtual void setContent(SceneContent *);
	virtual SceneContent *getContent() const { return _content; }

	virtual void handlePresented(Director *);
	virtual void handleFinished(Director *);

	virtual void setFrameConstraints(const core::FrameConstraints &);
	const core::FrameConstraints &getFrameConstraints() const { return _constraints; }

	virtual Size2 getContentSize() const override;

	virtual void setClipContent(bool);
	virtual bool isClipContent() const;

protected:
	using Node::init;
	using Node::addChild; // запрет добавлять ноды напрямую на сцену

	virtual Rc<Queue> makeQueue(Queue::Builder &&);

	virtual void updateContentNode(SceneContent *);

	// Uncomment to track retain/release cycles
	//#if SP_REF_DEBUG
	//	virtual bool isRetainTrackerEnabled() const override { return true; }
	//#endif

	Director *_director = nullptr;
	SceneContent *_content = nullptr;

	// non-owning; valid only for the duration of visitDraw inside render()
	FrameInfo *_frameInfo = nullptr;

	Rc<Queue> _queue;

	// False when the queue was adopted (see init(Rc<Queue>&&)): the scene then neither registers
	// nor unregisters the queue's internal resource.
	bool _ownsQueue = true;

	core::FrameConstraints _constraints;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLSCENE_H_ */
