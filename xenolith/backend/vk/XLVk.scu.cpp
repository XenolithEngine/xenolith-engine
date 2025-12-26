/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLCommon.h"
#include "XLVk.h"

// Enable to log device queues acquisition
#ifndef XL_VKDEVICE_DEBUG
#define XL_VKDEVICE_DEBUG 0
#endif

// Enable to log key API calls and timings
#ifndef XL_VKAPI_DEBUG
#define XL_VKAPI_DEBUG 0
#endif

#if XL_VKDEVICE_DEBUG
#define XL_VKDEVICE_LOG(...) log::source().debug("Vk::Device", __VA_ARGS__)
#else
#define XL_VKDEVICE_LOG(...)
#endif

#if XL_VKAPI_DEBUG
#define XL_VKAPI_LOG(...) log::source().debug("vk::Api", __VA_ARGS__)
#else
#define XL_VKAPI_LOG(...)
#endif

#include "XLVkDevice.cc"
#include "XLVkLoop.cc"
#include "XLVkInstance.cc"
#include "XLVkTable.cc"

#include "XLVkAttachment.cc"
#include "XLVkQueuePass.cc"
#include "XLVkRenderQueueCompiler.cc"
#include "XLVkTransferQueue.cc"
#include "XLVkMaterialCompiler.cc"
#include "XLVkMeshCompiler.cc"

#include "XLVkPlatform.cc"

#include "platform/android/XLVkPlatformAndroid.cc"
#include "platform/linux/XLVkPlatformLinux.cc"
#include "platform/win32/XLVkPlatformWin32.cc"
#include "platform/macos/XLVkPlatformMac.cc"

#include "SPSharedModule.h"
#include "XLVkPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

static SharedSymbol s_vkSharedSymbols[] = {
	SharedSymbol("platform::createInstance", platform::createInstance),
};

SP_USED static SharedModule s_vkGuiSharedModule(buildconfig::MODULE_XENOLITH_BACKEND_VK_NAME,
		s_vkSharedSymbols, sizeof(s_vkSharedSymbols) / sizeof(SharedSymbol));

} // namespace stappler::xenolith::vk
