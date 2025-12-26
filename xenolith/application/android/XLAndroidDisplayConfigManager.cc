/**
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

#include "XLAndroidDisplayConfigManager.h"
#include "XLAndroidContextController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static void AndroidDisplayConfigManager_handleDisplayChanged(JNIEnv *_Nonnull env,
		jobject _Nonnull thiz, jlong nativePointer) {
	auto native = reinterpret_cast<AndroidDisplayConfigManager *>(nativePointer);
	native->updateDisplayConfig();
}

static JNINativeMethod s_displayConfigMethods[] = {
	{"handleDisplayChanged", "(J)V",
		reinterpret_cast<void *>(&AndroidDisplayConfigManager_handleDisplayChanged)},
};

static void registerDisplayConfigMethods(const jni::RefClass &cl) {
	static std::atomic<int> s_counter = 0;

	if (s_counter.fetch_add(1) == 0) {
		cl.registerNatives(s_displayConfigMethods);
	}
}

AndroidDisplayConfigManager::~AndroidDisplayConfigManager() {
	if (thiz) {
		auto env = jni::Env::getEnv();
		proxy.finalize(thiz.ref(env));
		thiz = nullptr;
	}
}

bool AndroidDisplayConfigManager::init(NotNull<AndroidContextController> c,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sp::move(cb))) {
		return false;
	}

	auto jApp = c->getSelf();
	auto jClass = proxy.getClass().ref(jApp.getEnv());

	registerDisplayConfigMethods(jClass);

	thiz = proxy.create(jClass, jApp, reinterpret_cast<jlong>(this));

	_scalingMode = ScalingMode::DirectScaling;
	_controller = c;

	updateDisplayConfig();

	return true;
}

void AndroidDisplayConfigManager::invalidate() {
	if (thiz) {
		auto env = jni::Env::getEnv();
		proxy.finalize(thiz.ref(env));
		thiz = nullptr;
	}
}

void AndroidDisplayConfigManager::updateDisplayConfig(
		Function<void(DisplayConfig *_Nullable)> &&cb) {
	Rc<DisplayConfig> info = Rc<DisplayConfig>::create();

	auto env = jni::Env::getEnv();
	auto app = jni::Env::getApp();

	auto displays = app->DisplayManager.getDisplays(app->DisplayManager.service.ref(env));

	uint32_t idx = 0;
	for (auto displayIt : displays) {
		auto id = app->Display.getDisplayId(displayIt);
		auto name = app->Display.getName(displayIt);

		Extent2 mmSize;

		auto mode = app->Display.getMode(displayIt);
		auto currentModeId = app->DisplayMode.getModeId(mode);
		auto currentWidth = app->DisplayMode.getPhysicalWidth(mode);
		auto currentHeight = app->DisplayMode.getPhysicalHeight(mode);

		jni::Local metrics = nullptr;
		if (app->Display.getRealMetrics) {
			metrics = app->DisplayMetrics.constructor(app->DisplayMetrics.getClass().ref(env));
			app->Display.getRealMetrics(displayIt, metrics);

			auto xdpi = app->DisplayMetrics.xdpi(metrics);
			auto ydpi = app->DisplayMetrics.ydpi(metrics);

			mmSize = Extent2((currentWidth / xdpi) * 25.4f, (currentHeight / ydpi) * 25.4f);
		}

		auto &monIt = info->monitors.emplace_back(PhysicalDisplay{
			id,
			idx,
			MonitorId{StringView(name.getString()).str<Interface >()},
			mmSize,
		});

		if (app->DeviceProductInfo && app->Display.getDeviceProductInfo) {
			auto productInfo = app->Display.getDeviceProductInfo(displayIt);
			if (productInfo) {
				monIt.id.edid.vendorId = StringView(
						app->DeviceProductInfo.getManufacturerPnpId(productInfo).getString())
												 .str<Interface>();
				monIt.id.edid.vendor = core::EdidInfo::getVendorName(monIt.id.edid.vendorId);
				monIt.id.edid.model =
						StringView(app->DeviceProductInfo.getProductId(productInfo).getString())
								.str<Interface>();
				monIt.id.name = StringView(app->DeviceProductInfo.getName(productInfo).getString())
										.str<Interface>();
			}
		}

		uint32_t midx = 0;
		for (auto mIt : app->Display.getSupportedModes(displayIt)) {
			auto id = app->DisplayMode.getModeId(mIt);
			auto w = app->DisplayMode.getPhysicalWidth(mIt);
			auto h = app->DisplayMode.getPhysicalHeight(mIt);
			auto rate = app->DisplayMode.getRefreshRate(mIt);

			monIt.modes.emplace_back(DisplayMode{
				id,
				core::ModeInfo{
					static_cast<uint32_t>(w),
					static_cast<uint32_t>(h),
					static_cast<uint32_t>(rate * 1'000),
					1.0f,
				},
				"",
				toString(w, "x", h, "@", static_cast<uint32_t>(rate * 1'000)),
				Vector<float>(),
				midx == 0,
				id == currentModeId,
			});

			++midx;
		}

		info->logical.emplace_back(LogicalDisplay{
			id,
			IRect{
				0,
				0,
				static_cast<uint32_t>(currentWidth),
				static_cast<uint32_t>(currentHeight),
			},
			metrics ? app->DisplayMetrics.density(metrics) : 1.0f,
			static_cast<uint32_t>(app->Display.getRotation(displayIt)),
			id == 0,
			Vector<MonitorId>{monIt.id},
		});

		++idx;
	}

	if (app->DisplayTopology && app->DisplayManager.getDisplayTopology) {
		auto topology =
				app->DisplayManager.getDisplayTopology(app->DisplayManager.service.ref(env));

		auto array = app->DisplayTopology.getAbsoluteBounds(topology);

		auto size = app->SparseArray.size(array);
		for (jint i = 0; i < size; ++i) {
			auto key = app->SparseArray.keyAt(array, jint(i));
			auto value = app->SparseArray.valueAt(array, jint(i));

			auto bottom = app->RectF.bottom(value);
			auto left = app->RectF.left(value);
			auto right = app->RectF.right(value);
			auto top = app->RectF.top(value);

			for (auto &it : info->logical) {
				if (it.xid.xid == key) {
					it.rect.x = left;
					it.rect.y = top;
					it.rect.width = right - left;
					it.rect.height = bottom - top;
					break;
				}
			}

			slog().info("AndroidDisplayConfigManager", "Topology: ", key, " ", top, " ", right, " ",
					bottom, " ", left);
		}
	}

	if (cb) {
		cb(info);
	}
	handleConfigChanged(info);
}

void AndroidDisplayConfigManager::prepareDisplayConfigUpdate(
		Function<void(DisplayConfig *_Nullable)> &&cb) {
	updateDisplayConfig(sp::move(cb));
}

void AndroidDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> config,
		Function<void(Status)> &&cb) {
	if (cb) {
		cb(Status::Ok);
	}
}

} // namespace stappler::xenolith::platform
