/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_BACKEND_VK_XLVKPLATFORM_H_
#define XENOLITH_BACKEND_VK_XLVKPLATFORM_H_

#include "XLVkInstance.h"
#include "SPDso.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

class SP_PUBLIC FunctionTable final : public vk::LoaderTable {
public:
	using LoaderTable::LoaderTable;

	Rc<Instance> createInstance(NotNull<core::InstanceInfo>, NotNull<InstanceBackendInfo>,
			Dso &&vulkanModule) const;

	explicit operator bool() const {
		return vkGetInstanceProcAddr != nullptr && vkCreateInstance != nullptr
				&& vkEnumerateInstanceExtensionProperties != nullptr
				&& vkEnumerateInstanceLayerProperties != nullptr;
	}

private:
	InstanceInfo loadInfo() const;
	bool prepareData(InstanceData &, const InstanceInfo &) const;
	bool validateData(InstanceData &, const InstanceInfo &, bool &validationEnabled) const;

	Rc<Instance> doCreateInstance(InstanceData &, const InstanceInfo &, Dso &&vulkanModule,
			bool validationEnabled) const;

	mutable uint32_t _instanceVersion = 0;
	mutable Vector<VkLayerProperties> _instanceAvailableLayers;
	mutable Vector<VkExtensionProperties> _instanceAvailableExtensions;
};

SP_PUBLIC Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&);

} // namespace stappler::xenolith::vk::platform

#endif /* XENOLITH_BACKEND_VK_XLVKPLATFORM_H_ */
