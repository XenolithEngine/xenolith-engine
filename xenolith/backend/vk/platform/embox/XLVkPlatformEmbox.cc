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

#include "XLVkPlatform.h"

#if SPRT_EMBOX

// Embox links the application into the kernel image, and the Vulkan driver
// with it -- lavapipe or Venus (EMBOX_VULKAN, see vk.mk): there is no loader
// and no libvulkan.so to open. The driver's own entry points stand in for the
// loader's; both answer global commands for a NULL instance, as
// vkGetInstanceProcAddr does.
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance, const char *);
extern "C" VkResult vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *);

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	if (info->api != core::InstanceApi::Vulkan || !info->backend) {
		return nullptr;
	}

	// What a loader does first; version 7 is the one Mesa's drivers implement.
	uint32_t icdVersion = 7;
	if (vk_icdNegotiateLoaderICDInterfaceVersion(&icdVersion) != VK_SUCCESS) {
		log::source().error("Vk", "the driver refused the ICD interface version");
		return nullptr;
	}

	FunctionTable table(reinterpret_cast<PFN_vkGetInstanceProcAddr>(vk_icdGetInstanceProcAddr));

	if (!table) {
		log::source().error("Vk", "the driver does not give the global commands");
		return nullptr;
	}

	if (auto instance = table.createInstance(info, info->backend.get_cast<InstanceBackendInfo>(),
				sprt::Dso())) {
		return instance;
	}

	return nullptr;
}

} // namespace stappler::xenolith::vk::platform

#endif
