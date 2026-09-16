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

#include "XLMtlPlatform.h"
#include "XLMtlInstance.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	if (info->api != core::InstanceApi::Metal) {
		return nullptr;
	}

	// Metal has no instance object: devices are enumerated directly.
	// Validation is controlled by the environment (MTL_DEBUG_LAYER=1) or the
	// Xcode scheme, InstanceFlags::Validation is accepted but has no effect here
	auto instance = Rc<Instance>::alloc(info->flags);
	if (instance->getDeviceCount() == 0) {
		log::source().error("mtl", "No Metal devices available");
		return nullptr;
	}

	return instance;
}

void *createOffscreenLayer() {
	@autoreleasepool {
		CAMetalLayer *layer = [CAMetalLayer layer];
		return retainHandle(layer);
	}
}

void releaseLayerHandle(void *handle) { releaseHandle(handle); }

} // namespace stappler::xenolith::mtl::platform
