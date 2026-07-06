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

#include "XLWgpuPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	if (info->api != core::InstanceApi::WebGPU) {
		return nullptr;
	}

	if (::getenv("XL_WGPU_LOG")) {
		wgpuSetLogLevel(WGPULogLevel_Debug);
		wgpuSetLogCallback([](WGPULogLevel level, WGPUStringView message, void *) {
			log::source().debug("wgpu-native", StringView(message.data, message.length));
		}, nullptr);
	}

	WGPUInstanceExtras extras = {};
	extras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
	extras.backends = WGPUInstanceBackend_All;

	if (hasFlag(info->flags, core::InstanceFlags::Validation)) {
		extras.flags |= WGPUInstanceFlag_Validation | WGPUInstanceFlag_Debug;
	}

	WGPUInstanceDescriptor desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
	desc.nextInChain = &extras.chain;

	auto instance = wgpuCreateInstance(&desc);
	if (!instance) {
		log::source().error("webgpu", "Fail to create WGPUInstance");
		return nullptr;
	}

	// wgpu-native is linked directly, no DSO handle to hold
	return Rc<Instance>::alloc(instance, info->flags, sprt::Dso());
}

} // namespace stappler::xenolith::webgpu::platform
