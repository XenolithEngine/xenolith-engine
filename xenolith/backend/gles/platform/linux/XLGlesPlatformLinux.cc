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

#include "XLGlesPlatform.h"

#if SPRT_LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	if (info->api != core::InstanceApi::GLES) {
		return nullptr;
	}

	// Loaded dynamically, like libvulkan: the module must stay usable on a machine with no GL
	// stack at all (a failed open is just "backend unavailable", not a link error).
	auto handle = sprt::Dso(StringView("libEGL.so.1"));
	if (!handle) {
		log::source().error("GLES", "Fail to open libEGL.so.1: ", handle.getError());
		return nullptr;
	}

	Rc<InstanceBackendInfo> backendInfo;
	if (auto b = info->backend.get_cast<InstanceBackendInfo>()) {
		backendInfo = Rc<InstanceBackendInfo>(b);
	} else {
		backendInfo = Rc<InstanceBackendInfo>::create();
	}

	auto instance = Rc<Instance>::alloc(info->flags, sp::move(backendInfo), sp::move(handle));
	if (!instance->init()) {
		return nullptr;
	}
	return instance;
}

} // namespace stappler::xenolith::gles::platform

#endif
