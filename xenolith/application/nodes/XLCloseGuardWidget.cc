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

#include "XLCloseGuardWidget.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool CloseGuardWidget::init() {
	if (!Node::init()) {
		return false;
	}

	auto l = addSystem(Rc<InputListener>::create());
	l->setWindowStateCallback([this](WindowState state, WindowState changes) {
		if (state != _currentState) {
			updateWindowState(state);
		}
		return true;
	});

	return true;
}

void CloseGuardWidget::handleContentSizeDirty() { Node::handleContentSizeDirty(); }

void CloseGuardWidget::handleLayout(Node *parent) { Node::handleLayout(parent); }

void CloseGuardWidget::handleExit() {
	if (_director && _rejectWhenExit) {
		_director->getWindow()->disableState(WindowState::CloseRequest);
	}
	Node::handleExit();
}

void CloseGuardWidget::updateWindowState(WindowState state) {
	if (!hasFlag(state, WindowState::CloseRequest)) {
		_rejectWhenExit = false;
		close();
	}
}

void CloseGuardWidget::commit() {
	_rejectWhenExit = false;
	_director->getWindow()->enableState(WindowState::CloseRequest);
	close();
}
void CloseGuardWidget::reject() {
	_rejectWhenExit = false;
	_director->getWindow()->disableState(WindowState::CloseRequest);
	close();
}

void CloseGuardWidget::close() { removeFromParent(); }

} // namespace stappler::xenolith
