/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLWindowSceneInfo.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

WindowSceneInfo::~WindowSceneInfo() {
#if DEBUG
	// Destroying this anywhere but the app thread means the payload was left on a WindowInfo that
	// died on the context thread — the one failure mode of the whole transport, and one with no
	// other symptom until a captured scene-graph node is released off-thread much later.
	if (_owner && _owner != sprt::dispatch::Looper::acquire()) {
		log::source().error("WindowSceneInfo",
				"destroyed on a foreign thread; the payload was not taken off WindowInfo");
	}
	// Nobody answered the opener. Either the window never reached teardown, or a new creation path
	// forgot to hand the payload back — see Context::createWindow.
	if (_onClose && !_closeFired) {
		log::source().warn("WindowSceneInfo", "destroyed without firing its close callback");
	}
#endif
}

bool WindowSceneInfo::init(SceneBuilder &&builder, CloseCallback &&onClose) {
	_builder = sp::move(builder);
	_onClose = sp::move(onClose);
#if DEBUG
	_owner = sprt::dispatch::Looper::acquire();
#endif
	return true;
}

StringView WindowSceneInfo::getId() const {
	if (auto info = _window ? _window->getInfo() : nullptr) {
		return info->id;
	}
	return StringView();
}

Rc<Scene> WindowSceneInfo::makeScene(NotNull<AppThread> app,
		NotNull<core::RenderServerChannel> window, const core::FrameConstraints &constraints) {
	if (!_builder) {
		return nullptr;
	}
	return _builder(app, window, constraints);
}

void WindowSceneInfo::fireClose() {
	if (_closeFired) {
		return;
	}
	_closeFired = true;

	// Move out before invoking: a callback that opens the next window is a normal thing to write,
	// and it must not be able to reenter this one.
	auto cb = sp::move(_onClose);
	_onClose = nullptr;
	if (cb) {
		cb(this);
	}
}

} // namespace stappler::xenolith
