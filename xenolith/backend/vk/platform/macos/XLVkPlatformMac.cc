/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPFilesystem.h"
#include "XLVkPlatform.h"

#if MACOS

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

Rc<core::Instance> createInstance(Rc<core::InstanceInfo> &&info) {
	if (info->api != core::InstanceApi::Vulkan || !info->backend) {
		return nullptr;
	}

	bool isBundled = false;

	auto execPath = sprt::platform::getExecPath();

	String loaderPath;

	auto root = filepath::root(filepath::root(execPath));
	auto infoPlistPath = filepath::merge<Interface>(root, "Info.plist");
	if (filesystem::exists(FileInfo(infoPlistPath))) {
		auto bundledPath = filepath::merge<Interface>(root, "Frameworks", "libvulkan.1.dylib");
		if (!filesystem::exists(FileInfo{bundledPath})) {
			bundledPath = filepath::merge<Interface>(root, "Frameworks", "libvulkan.dylib");

			if (!filesystem::exists(FileInfo{bundledPath})) {
				log::source().error("Vulkan", "Vulkan loader is not found on paths: ", bundledPath);
				return nullptr;
			}
		}

		loaderPath = bundledPath;
		isBundled = true;
	} else {
		auto flatPath = filepath::merge<Interface>(filepath::root(execPath), "vulkan/lib",
				"libvulkan.1.dylib");
		if (!filesystem::exists(FileInfo{flatPath})) {
			flatPath = filepath::merge<Interface>(filepath::root(execPath), "libvulkan.dylib");
		}

		if (!filesystem::exists(FileInfo{flatPath})) {
			log::source().error("Vulkan", "Vulkan loader is not found on path: ", flatPath);
			return nullptr;
		}

		loaderPath = flatPath;
		isBundled = false;
	}

	if (isBundled) {
		// Restrict the loader to the ICD we ship. Left to its own devices it *adds* the system-wide
		// manifests (/usr/local/share/vulkan/icd.d) to the bundled one, so a machine with the
		// Vulkan SDK installed ends up with two libMoltenVK images in the process. They export the
		// same Objective-C classes (MVKBlockObserver & co), and the ObjC runtime keeps only one
		// implementation per class name — so command-buffer completion handlers run against objects
		// laid out by the *other* library. That corrupts Metal's resource bookkeeping and surfaces
		// as VK_ERROR_DEVICE_LOST / kIOGPUCommandBufferCallbackErrorInvalidResource, mostly once
		// several windows are presenting at once.
		auto icdPath = filepath::merge<Interface>(root, "Resources", "vulkan", "icd.d",
				"MoltenVK_icd.json");
		if (filesystem::exists(FileInfo{icdPath})) {
			// VK_DRIVER_FILES is the current name, VK_ICD_FILENAMES the pre-1.3.207 one; older
			// loaders ignore the former, newer ones accept either, so set both.
			::setenv("VK_DRIVER_FILES", icdPath.data(), 1);
			::setenv("VK_ICD_FILENAMES", icdPath.data(), 1);
		} else {
			log::source().warn("Vulkan",
					"Bundled ICD manifest is not found, the loader may pick up a system-wide "
					"MoltenVK in addition to the bundled one: ",
					icdPath);
		}
	} else {
		// Point to where is layers located when we not in bundle
		::setenv("VK_LAYER_PATH",
				filepath::merge<Interface>(filepath::root(execPath), "vulkan", "explicit_layer.d")
						.data(),
				1);
	}

	sprt::Dso handle(loaderPath);
	if (!handle) {
		log::source().error("Vulkan", "Fail to dlopen loader: ", loaderPath);
		return nullptr;
	}

	auto getInstanceProcAddr = handle.sym<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	if (!getInstanceProcAddr) {
		log::source().error("Vulkan",
				"Fail to find entrypoint 'vkGetInstanceProcAddr' in loader: ", loaderPath);
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
