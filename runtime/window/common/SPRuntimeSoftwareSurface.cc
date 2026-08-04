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

#include <sprt/runtime/window/software_surface.h>

namespace sprt::window {

uint32_t SoftwareSwapchain::acquire(Status &status) {
	if (_invalid) {
		status = Status::ErrorCancelled;
		return Max<uint32_t>;
	}

	// Round-robin from the slot after the one handed out last, so a transport that releases
	// promptly still cycles through the whole ring instead of hammering one buffer.
	auto count = uint32_t(_buffers.size());
	for (uint32_t i = 0; i < count; ++i) {
		auto index = (_nextIndex + i) % count;
		if (_busy[index]) {
			continue;
		}

		_nextIndex = (index + 1) % count;
		status = Status::Ok;
		return index;
	}

	// Every slot is still held by the window system. This is not an error: the presentation
	// engine arms its acquisition timer and comes back. It must never turn into a wait here -
	// the thread that would block is the same one that dispatches the release event.
	status = Status::Timeout;
	return Max<uint32_t>;
}

void SoftwareSwapchain::setBufferBusy(uint32_t index) {
	if (index < _busy.size()) {
		_busy[index] = true;
	}
}

void SoftwareSwapchain::setBufferFree(uint32_t index) {
	if (index >= _busy.size() || !_busy[index]) {
		return;
	}

	_busy[index] = false;

	if (_releaseCallback) {
		_releaseCallback(index);
	}
}

bool SoftwareSwapchain::isBufferBusy(uint32_t index) const {
	return index < _busy.size() && _busy[index];
}

} // namespace sprt::window
