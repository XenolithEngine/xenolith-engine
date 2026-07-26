/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKINSTANCE_H_
#define XENOLITH_BACKEND_VK_XLVKINSTANCE_H_

#include "XLCoreInstance.h"
#include "XLVkInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class Allocator;

struct SP_PUBLIC InstanceInfo {
	core::InstanceFlags flags = core::InstanceFlags::None;
	uint32_t targetVersion = 0;
	bool hasSurfaceExtension = false;
	SurfaceBackendMask availableBackends;
	SpanView<VkLayerProperties> availableLayers;
	SpanView<VkExtensionProperties> availableExtensions;
};

struct SP_PUBLIC InstanceData {
	uint32_t targetVulkanVersion;
	StringView applicationVersion;
	StringView applicationName;
	Vector<const char *> layersToEnable;
	Vector<const char *> extensionsToEnable;

	SurfaceBackendMask enableBackends;

	Function<SurfaceBackendMask(const Instance *, VkPhysicalDevice device, uint32_t queueIdx)>
			checkPresentationSupport;

	void enableLayer(const char *);
	void enableExtension(const char *);
};

struct SP_PUBLIC InstanceBackendInfo : public core::InstanceBackendInfo {
	virtual ~InstanceBackendInfo() = default;
	Function<bool(InstanceData &, const InstanceInfo &)> setup;

	// nothing to encode
	virtual Value encode() const { return Value(); }
};

struct SP_PUBLIC LoopBackendInfo : core::LoopBackendInfo {
	using DeviceSupportCallback = Function<bool(const DeviceInfo &)>;
	using DeviceExtensionsCallback = Function<Vector<StringView>(const DeviceInfo &)>;
	using DeviceFeaturesCallback = Function<DeviceInfo::Features(const DeviceInfo &)>;

	DeviceSupportCallback deviceSupportCallback;
	DeviceExtensionsCallback deviceExtensionsCallback;
	DeviceFeaturesCallback deviceFeaturesCallback;

	// nothing to encode
	virtual Value encode() const { return Value(); }
};

class SP_PUBLIC Instance : public core::Instance, public InstanceTable {
public:
	using OptVec = sprt::bitset<toInt(OptionalInstanceExtension::Max)>;

	using PresentSupportCallback = Function<SurfaceBackendMask(const Instance *,
			VkPhysicalDevice device, uint32_t familyIdx)>;

	Instance(VkInstance, const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
			uint32_t targetVersion, OptVec &&optionals, sprt::Dso &&vulkanModule,
			PresentSupportCallback &&, SurfaceBackendMask &&, core::InstanceFlags flags);
	virtual ~Instance();

	virtual size_t getDeviceCount() const override;

	virtual bool readDeviceProperties(size_t, sprt::window::gapi::DeviceProperties &) override;

	virtual Rc<core::Loop> makeLoop(NotNull<sprt::dispatch::Looper>,
			Rc<core::LoopInfo> &&) const override;

	Rc<Device> makeDevice(const core::LoopInfo &) const;

	core::SurfaceInfo getSurfaceOptions(VkSurfaceKHR, VkPhysicalDevice,
			core::FullScreenExclusiveMode fullscreenMode, void *fullscreenHandle) const;
	VkExtent2D getSurfaceExtent(VkSurfaceKHR, VkPhysicalDevice) const;

	// Create a VK_KHR_display surface on the primary display of the first device
	// that drives one. Used for direct-to-display (no window system) startup.
	// prefW/prefH hint the desired resolution (0 = pick the largest mode); the mode
	// nearest that size is chosen (QEMU advertises bogus huge modes otherwise).
	// Returns VK_NULL_HANDLE if no display is available.
	VkSurfaceKHR createDisplayPlaneSurface(uint32_t prefW = 0, uint32_t prefH = 0) const;

	VkInstance getInstance() const;

	void printDevicesInfo(const CallbackStream &stream, bool initOnly = false) const;

	uint32_t getVersion() const { return _version; }

	SurfaceBackendMask getSurfaceBackends() const { return _surfaceBackendMask; }

private:
	struct DeviceInfoWrapper {
		mutable sprt::qonce once;
		mutable DeviceInfo info;

		DeviceInfoWrapper(VkPhysicalDevice dev) : info(dev) { }
	};

	void getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &,
			const DeviceInfo::OptVec &, uint32_t) const;
	void getDeviceProperties(const VkPhysicalDevice &device, DeviceInfo::Properties &,
			const DeviceInfo::OptVec &, uint32_t) const;

	void getDeviceInfo(DeviceInfo &, VkPhysicalDevice device) const;

	// Build a VK_KHR_display plane surface on a resolved VkDisplayKHR (shared by
	// the auto-enumerated and acquire-drm-display paths of createDisplayPlaneSurface).
	VkSurfaceKHR makeDisplayPlaneSurface(VkPhysicalDevice, VkDisplayKHR, StringView name,
			uint32_t prefW, uint32_t prefH) const;

	SurfaceBackendMask checkPresentationSupport(VkPhysicalDevice device, uint32_t) const;

	friend class VirtualDevice;
	friend class DrawDevice;
	friend class PresentationDevice;
	friend class TransferDevice;
	friend class Allocator;
	friend class ViewImpl;

	VkInstance _instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	uint32_t _version = 0;
	OptVec _optionals;
	Vector<DeviceInfoWrapper> _devices;
	PresentSupportCallback _checkPresentSupport;
	SurfaceBackendMask _surfaceBackendMask;
};

SP_PUBLIC StringView getSurfaceBackendExtension(SurfaceBackend);
SP_PUBLIC SurfaceBackend getSurfaceBackendForExtension(StringView);

} // namespace stappler::xenolith::vk

#endif // XENOLITH_BACKEND_VK_XLVKINSTANCE_H_
