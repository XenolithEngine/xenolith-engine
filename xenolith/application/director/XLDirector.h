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

#ifndef XENOLITH_APPLICATION_DIRECTOR_XLDIRECTOR_H_
#define XENOLITH_APPLICATION_DIRECTOR_XLDIRECTOR_H_

#include "XLResourceCache.h"
#include "XLAppThread.h"
#include "XLInput.h" // IWYU pragma: keep
#include "SPMovingAverage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Scene;
class Scheduler;
class InputDispatcher;
class TextInputManager;
class ActionManager;
class DirectorWindow;

class SP_PUBLIC Director : public Ref {
public:
	using FrameRequest = core::FrameRequest;

	virtual ~Director();

	Director();

	bool init(NotNull<AppThread>, const core::FrameConstraints &, NotNull<AppWindow>);

	void setFrameConstraints(const core::FrameConstraints &);

	void runScene(Rc<Scene> &&);

	bool acquireFrame(FrameRequest *);

	void update(uint64_t t);

	// Can be nullptr to disconnect director from window
	void setWindow(AppWindow *);

	void end();

	AppThread *getApplication() const { return _application; }
	core::Loop *getGlLoop() const;
	AppWindow *getWindow() const { return _window; }

	Scheduler *getScheduler() const { return _scheduler; }
	ActionManager *getActionManager() const { return _actionManager; }
	InputDispatcher *getInputDispatcher() const { return _inputDispatcher; }
	TextInputManager *getTextInputManager() const;

	Scene *getScene() const { return _scene; }
	ResourceCache *getResourceCache() const;

	const Mat4 &getGeneralProjection() const { return _generalProjection; }

	const core::FrameConstraints &getFrameConstraints() const { return _constraints; }

	void pushDrawStat(const DrawStat &);

	const UpdateTime &getUpdateTime() const { return _time; }
	const DrawStat &getDrawStat() const { return _drawStat; }

	float getFps() const;
	float getAvgFps() const;
	float getSpf() const; // in milliseconds
	float getFenceFrameTime() const;
	float getTimestampFrameTime() const;

	float getDirectorFrameTime() const { return _avgFrameTimeValue / 1000.0f; }

	void autorelease(Ref *);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	bool hasActiveInteractions();

	Rc<AppThread> _application;
	Rc<AppWindow> _window;
	Rc<core::PresentationEngine> _engine;

	core::FrameConstraints _constraints;

	uint64_t _startTime = 0;
	UpdateTime _time;
	DrawStat _drawStat;

	Rc<Scene> _scene;
	Rc<Scene> _nextScene;

	Mat4 _generalProjection;

	Rc<AllocRef> _allocator;
	Rc<PoolRef> _pool;
	Rc<Scheduler> _scheduler;
	Rc<ActionManager> _actionManager;
	Rc<InputDispatcher> _inputDispatcher;
	Rc<TextInputManager> _textInput;

	Vector<Rc<Ref>> _autorelease;

	math::MovingAverage<20, uint64_t> _avgFrameTime;
	uint64_t _avgFrameTimeValue = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_DIRECTOR_XLDIRECTOR_H_ */
