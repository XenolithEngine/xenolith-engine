/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSCENE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSCENE_H_

#include "XLCoreRenderSession.h"
#include "XLScene.h"
#include "XLInput.h"
#include "XL2d.h" // QueueType, re-exported below

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class VectorSprite;

class SP_PUBLIC Scene2d : public Scene {
public:
	// Declared in XL2d.h so the backend pass makers can tag their queues with it; re-exported here
	// because Scene2d::QueueType is how the rest of the engine spells it.
	using QueueType = basic2d::QueueType;

	struct QueueInfo {
		Extent2 extent;
		Color4F backgroundColor = Color4F::WHITE;
		QueueType type = QueueType::Default;

		// Defaulted per QueueType in Scene2d::init and overridable from buildQueueResources.
		// maxOf<uint32_t> means "not set, use the type's default".
		core::QueueDamageFlags damage = core::QueueDamageFlags(maxOf<uint32_t>());
	};

	class FpsDisplay;

	virtual ~Scene2d() = default;

	// create with default render queue
	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &constraints);

	// create with default render queue, resources can be added via callback
	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel>,
			const Callback<void(Queue::Builder &)> &, const core::FrameConstraints &);

	virtual bool init(Queue::Builder &&, const core::FrameConstraints &) override;

	// Adopt an already-built (usually already-compiled) queue instead of building one.
	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel>,
			Rc<core::Queue> &&, const core::FrameConstraints &);

	// Fill `builder` with the standard 2d render graph for the current gAPI, honouring
	// QueueInfo::type and defaulting QueueInfo::damage for it.
	//
	// Static on purpose: this is the part of scene construction that needs no Scene, so a queue
	// can be built - and cached, and compiled - before the scene or the window that will use it
	// exists. QueueCache is the intended caller.
	static bool buildQueue(NotNull<AppThread>, QueueInfo &, core::Queue::Builder &);

	virtual void update(const UpdateTime &time) override;

	virtual void handleContentSizeDirty() override;

	virtual void setFpsVisible(bool);
	virtual bool isFpsVisible() const;

	virtual void setContent(SceneContent *) override;

protected:
	// What kind of queue this scene wants, before anything is built.
	//
	// Separate from buildQueueResources because it is the half that is meaningful on BOTH paths: a
	// local scene turns the answer into a graph, a remote one matches it against the graphs the
	// server already has. Setting `type` from buildQueueResources worked only locally -- a client
	// never builds a queue, so the preference had nowhere to be read from and every remote scene
	// silently got whatever the server's first queue happened to be.
	virtual void describeQueue(QueueInfo &);

	// override this to add initial resources to be compiled woth render queue
	virtual void buildQueueResources(QueueInfo &, core::Queue::Builder &);

	virtual void initialize();
	virtual void addContentNodes(SceneContent *);

	void updateInputEventData(InputEventData &data, const InputEventData &source, Vec2 pos,
			uint32_t id);

	// Which of the server's shared queues this scene renders through, by name (the name is the
	// handle the rest of the client/server exchange uses -- see RemoteWindow::compileRenderQueue).
	// The default matches on what the queue IS: the server's gAPI and the shape `info` asked for.
	// Empty means nothing usable was offered, and scene construction fails.
	virtual StringView selectServerQueue(NotNull<AppThread> app,
			NotNull<core::RenderServerChannel> window, const QueueInfo &info);

	InputEventData _data1 = InputEventData{maxOf<uint32_t>() - 1};
	InputEventData _data2 = InputEventData{maxOf<uint32_t>() - 2};
	InputListener *_listener = nullptr;
	FpsDisplay *_fps = nullptr;
	VectorSprite *_pointerReal = nullptr;
	VectorSprite *_pointerVirtual = nullptr;
	VectorSprite *_pointerCenter = nullptr;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCENE_H_ */
