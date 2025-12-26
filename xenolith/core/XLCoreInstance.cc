/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "XLCoreInstance.h"
#include "XLCoreLoop.h"
#include "SPSharedModule.h"

#ifdef MODULE_XENOLITH_BACKEND_VK
#include "XLVkPlatform.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

Value InstanceInfo::encode() const {
	Value ret;
	ret.setString(getInstanceApiName(api), "backend");
	if (auto i = backend->encode()) {
		ret.setValue(move(i), "info");
	}
	Value f;
	if (hasFlag(flags, InstanceFlags::Validation)) {
		f.addString("Validation");
	}
	if (hasFlag(flags, InstanceFlags::RenderDoc)) {
		f.addString("RenderDoc");
	}
	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value LoopInfo::encode() const {
	Value ret;
	ret.setInteger(deviceIdx, "deviceIdx");
	ret.setString(getImageFormatName(defaultFormat), "defaultFormat");
	if (auto b = backend->encode()) {
		ret.setValue(move(b), "backend");
	}
	return ret;
}

Rc<Instance> Instance::create(Rc<InstanceInfo> &&info) {
#ifdef MODULE_XENOLITH_BACKEND_VK
	if (info->api == InstanceApi::Vulkan) {
		auto createInstance =
				SharedModule::acquireTypedSymbol<decltype(&vk::platform::createInstance)>(
						buildconfig::MODULE_XENOLITH_BACKEND_VK_NAME, "platform::createInstance");
		if (createInstance) {
			return createInstance(move(info));
		}
	}
#endif
	return nullptr;
}

Instance::~Instance() {
	_dsoModule.close();

	log::source().debug("core::Instance", "~Instance");
}

Instance::Instance(InstanceApi api, InstanceFlags flags, Dso &&dso)
: _api(api), _flags(flags), _dsoModule(sp::move(dso)) { }

Rc<Loop> Instance::makeLoop(NotNull<event::Looper>, Rc<LoopInfo> &&) const { return nullptr; }

StringView getInstanceApiName(InstanceApi backend) {
	switch (backend) {
	case InstanceApi::None: return "None"; break;
	case InstanceApi::Vulkan: return "Vulkan"; break;
	}
	return StringView();
}

} // namespace stappler::xenolith::core
