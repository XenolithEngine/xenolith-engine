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

// A Tau user module: the Vulkan loader is a library of the module's preload, which finds its
// driver by the manifests in icd.d (xenolith-tau RT-A-VulkanLoader-1).

#include "XLVkPlatform.h"

#if SPRT_TAU_USER

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	if (info->api != core::InstanceApi::Vulkan || !info->backend) {
		return nullptr;
	}

	// A module has at most 16 threads besides its first: llvmpipe's rasterizer, one a CPU by
	// default, shares them with the engine's pools.
	if (!::getenv("LP_NUM_THREADS")) {
		auto cpus = sprt::thread::hardware_concurrency();
		::setenv("LP_NUM_THREADS", toString(cpus > 2 ? cpus / 2 : 1).data(), 0);
	}

	auto handle = sprt::Dso("libvulkan.so.1");
	if (!handle) {
		log::source().error("Vk", "Fail to open libvulkan.so.1: not in the module's preload");
		return nullptr;
	}

	auto getInstanceProcAddr = handle.sym<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	if (!getInstanceProcAddr) {
		return nullptr;
	}

	FunctionTable table(getInstanceProcAddr);
	if (!table) {
		return nullptr;
	}

	return table.createInstance(info, info->backend.get_cast<InstanceBackendInfo>(), move(handle));
}

} // namespace stappler::xenolith::vk::platform

#endif
