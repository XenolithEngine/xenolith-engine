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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLINTERACTIVENODE_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLINTERACTIVENODE_H_

#include "SPDocStyle.h"
#include "XLUiConfig.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using InteractiveState = stappler::document::InteractiveFlags;

struct InteractiveComponent {
	static ComponentId Id;
	// <=0 - inactive, >0 - active;
	// implementet as counter so multiple sources can mark node as active
	// (e.g. mouse down on node + active IME)
	int activeCounter = 0;
	int focusCounter = 0;
	int hoverCounter = 0;
	InteractiveState state = InteractiveState::Enabled;

	bool handleHover(int value) {
		hoverCounter = hoverCounter + value;
		if (hoverCounter > 0 && !sprt::hasFlag(state, InteractiveState::Hover)) {
			return updateState(state | InteractiveState::Hover);
		} else if (hoverCounter <= 0 && sprt::hasFlag(state, InteractiveState::Hover)) {
			return updateState(state & ~InteractiveState::Hover);
		}
		return false;
	}

	bool handleActive(int value) {
		activeCounter = activeCounter + value;
		if (activeCounter > 0 && !sprt::hasFlag(state, InteractiveState::Active)) {
			return updateState(state | InteractiveState::Active);
		} else if (activeCounter <= 0 && sprt::hasFlag(state, InteractiveState::Active)) {
			return updateState(state & ~InteractiveState::Active);
		}
		return false;
	}

	bool handleFocus(int value) {
		focusCounter = focusCounter + value;
		if (focusCounter > 0 && !sprt::hasFlag(state, InteractiveState::Focus)) {
			return updateState(state | InteractiveState::Focus);
		} else if (focusCounter <= 0 && sprt::hasFlag(state, InteractiveState::Focus)) {
			return updateState(state & ~InteractiveState::Focus);
		}
		return false;
	}

	bool updateState(InteractiveState newState) {
		if (newState != state) {
			state = newState;
			return true;
		}
		return false;
	}
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLINTERACTIVENODE_H_
