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

#include "XLSoftInstance.h"
#include "XLSoftDevice.h"
#include "XLSoftLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

Instance::Instance(core::InstanceFlags flags, sprt::Dso &&dso)
: core::Instance(core::InstanceApi::Software, flags, sp::move(dso)) { }

size_t Instance::getDeviceCount() const { return 1; }

bool Instance::readDeviceProperties(size_t idx, sprt::window::gapi::DeviceProperties &props) {
	if (idx != 0) {
		return false;
	}

	props.deviceName = StringView("CPU (software rasterizer)");
	props.apiVersion = 0;
	props.driverVersion = 0;

	// The pseudo-swapchain is always available: presentation never depends on a driver here.
	props.presentationSupported = true;
	return true;
}

Rc<core::Loop> Instance::makeLoop(NotNull<sprt::dispatch::Looper> looper,
		Rc<core::LoopInfo> &&info) const {
	return Rc<Loop>::create(looper, const_cast<Instance *>(this), sp::move(info));
}

Rc<Device> Instance::makeDevice(const core::LoopInfo &) const {
	// deviceIdx is ignored on purpose - there is only ever one device and selecting "another" CPU
	// is not a thing; an out-of-range index is not an error worth failing a launch over.
	return Rc<Device>::create(this);
}

} // namespace stappler::xenolith::soft
