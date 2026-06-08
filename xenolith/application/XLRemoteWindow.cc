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

#include "XLRemoteWindow.h"
#include "XLRemoteSerialize.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

RemoteWindow::~RemoteWindow() { }

bool RemoteWindow::init(const Value &val) {
	_id = val.getInteger(0);
	_windowId = val.getString(1);
	_state = static_cast<core::WindowState>(val.getInteger(2));
	_capabilities = static_cast<sprt::window::WindowCapabilities>(val.getInteger(3));
	_appFrameConstraints = remote::deserializeFrameConstraints(val.getValue(4));
	_appSwapchainConfig = remote::deserializeSwapchainConfig(val.getValue(5));

	if (val.hasValue(6)) {
		_info = remote::deserializeWindowInfo(val.getValue(6));
	}

	return true;
}

void RemoteWindow::compileRenderQueue(const Rc<core::Queue> &, Function<void(bool)> &&) { }
void RemoteWindow::compileResource(Rc<core::Resource> &&, Function<void(bool)> &&, bool preload) { }
void RemoteWindow::compileMaterials(Rc<core::MaterialInputData> &&,
		const Vector<Rc<core::DependencyEvent>> &) { }
void RemoteWindow::compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&) { }

void RemoteWindow::attachRenderQueue(const Rc<core::Queue> &) { }

void RemoteWindow::setReadyForNextFrame() { }
void RemoteWindow::setPreferredFrameInterval(uint64_t intervalUs) { }
core::FrameTimingInfo RemoteWindow::getFrameTiming() const { return core::FrameTimingInfo(); }

void RemoteWindow::acquireScreenInfo(Function<void(NotNull<core::ScreenInfo>)> &&, Ref *) { }
void RemoteWindow::acquireTextInput(core::TextInputRequest &&) { }
void RemoteWindow::releaseTextInput() { }
void RemoteWindow::close(bool graceful) { }

void RemoteWindow::handleBackButton() { }

const sprt::window::WindowInfo *RemoteWindow::getInfo() const { return _info; }

bool RemoteWindow::enableState(core::WindowState) { return false; }
bool RemoteWindow::disableState(core::WindowState) { return false; }

bool RemoteWindow::setFullscreen(core::FullscreenInfo &&, Function<void(Status)> &&, Ref *) {
	return false;
}

bool RemoteWindow::setPreferredFrameRate(float, Function<void(Status)> &&) { return false; }

void RemoteWindow::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) { }

bool RemoteWindow::openWindowMenu(Vec2 pos) { return false; }

void RemoteWindow::handleInputEvents(Vector<core::InputEventData> &&events) { }

void RemoteWindow::updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&) { }

} // namespace stappler::xenolith
