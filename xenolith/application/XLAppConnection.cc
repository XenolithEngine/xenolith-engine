/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLAppConnection.h"
#include "XLAppWindow.h"
#include "director/XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Out-of-line virtual destructor anchors the vtable (stage-1 vtable lesson) and lets the Rc<>
// members destruct against complete types here.
__SPRT_PUSH_ALLOW_CXXABI_ALLOC

Connection::~Connection() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool Connection::init(NotNull<AppThread> app) {
	_app = app;
	return true;
}

bool Connection::attach(NotNull<AppWindow> window, core::RenderClientChannel *remote) {
	if (!remote) {
		return false;
	}
	_window = window;
	_client = remote;
	// Drive the existing window from the remote client; its local Director stays alive as fallback.
	window->setRenderClient(remote);
	window->setReadyForNextFrame();
	return true;
}

void Connection::detach() {
	if (_window) {
		// Revert the window to its local fallback Director (the default scene resumes).
		_window->setRenderClient(_window->getDirector());
		_window->setReadyForNextFrame();
		_window = nullptr;
	}
	_client = nullptr;
}

} // namespace stappler::xenolith
