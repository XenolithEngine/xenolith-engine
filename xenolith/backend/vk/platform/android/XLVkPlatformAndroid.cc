/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLVkPlatform.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	auto handle = Dso("libvulkan.so.1");
	if (!handle) {
		handle = Dso("libvulkan.so");
		if (!handle) {
			log::source().error("Vk", "Fail to open libvulkan.so");
			return nullptr;
		}
	}

	auto getInstanceProcAddr = handle.sym<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	if (!getInstanceProcAddr) {
		return nullptr;
	}

	FunctionTable table(getInstanceProcAddr);

	if (!table) {
		return nullptr;
	}

	if (auto instance = table.createInstance(info, info->backend.get_cast<InstanceBackendInfo>(),
				move(handle))) {
		return instance;
	}

	return nullptr;
}

} // namespace stappler::xenolith::vk::platform

#endif
