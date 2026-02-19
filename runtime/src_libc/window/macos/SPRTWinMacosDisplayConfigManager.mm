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

#define __SPRT_BUILD 1

#import <CoreGraphics/CGDisplayConfiguration.h>
#import <IOKit/graphics/IOGraphicsTypes.h>
#import <IOKit/IOKitLib.h>
#import <AppKit/AppKit.h>

#include <sprt/runtime/base16.h>

#include "private/window/macos/SPRTWinMacosDisplayConfigManager.h"
#include "private/window/macos/SPRTWinMacosContextController.h"
#include "private/window/macos/SPRTWinMacosWindow.h"

namespace sprt::window {

struct MacosEdidData {
	EdidInfo edid;
	uint32_t vendorId = 0;
	uint32_t serialNumber = 0;
	uint32_t productId = 0;
};

static String getMonitorIdString(uint32_t idx) {
	idx = byteorder::bswap32(idx);

	char buf[10] = {0};
	base16::encode((const uint8_t *)&idx, 4, buf, 10);

	auto tmp = StringView(buf);
	tmp.skipChars<StringView::Chars<'0'>>();
	return tmp.str<String>();
}

static MonitorId getMonitorId(CGDirectDisplayID display, NSScreen *screen,
		SpanView<MacosEdidData> edids) {
	auto vendor = CGDisplayVendorNumber(display);
	auto model = CGDisplayModelNumber(display);
	auto serial = CGDisplaySerialNumber(display);

	String name;
	if (auto ptr = screen.localizedName.UTF8String) {
		name = ptr;
	}

	for (auto &it : edids) {
		if (it.vendorId == vendor && it.serialNumber == serial) {
			return MonitorId{name, it.edid};
		}
	}

	if (vendor == 1'552) {
		if (StringView(name).starts_with("Built-in ")) {
			name = "Built-in display";
		}
		return MonitorId{name,
			EdidInfo{
				getMonitorIdString(vendor),
				getMonitorIdString(model),
				getMonitorIdString(serial),
				"Apple Computer Inc",
			}};
	} else {
		return MonitorId{name,
			EdidInfo{
				getMonitorIdString(vendor),
				getMonitorIdString(model),
				getMonitorIdString(serial),
				"",
			}};
	}
}

static bool isDisplayModeAvailable(CGDisplayModeRef mode) {
	uint32_t flags = CGDisplayModeGetIOFlags(mode);

	if (!(flags & kDisplayModeValidFlag) || !(flags & kDisplayModeSafeFlag)) {
		return false;
	}
	if (flags & kDisplayModeInterlacedFlag) {
		return false;
	}
	if (flags & kDisplayModeStretchedFlag) {
		return false;
	}
	return true;
}

bool MacosDisplayConfigManager::init(NotNull<MacosContextController> c,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sprt::move(cb))) {
		return false;
	}

	_controller = c;

	CGDisplayRegisterReconfigurationCallback(&handleMacConfigUpdated, this);

	updateDisplayConfig();

	return true;
}

void MacosDisplayConfigManager::invalidate() {
	CGDisplayRemoveReconfigurationCallback(&handleMacConfigUpdated, this);
	_controller = nullptr;
}

void MacosDisplayConfigManager::restoreMode(Function<void(Status)> &&cb, Ref *) {
	CGRestorePermanentDisplayConfiguration();
	if (cb) {
		cb(Status::Ok);
	}
	updateDisplayConfig(nullptr);
}

void MacosDisplayConfigManager::handleMacConfigUpdated(CGDirectDisplayID display,
		CGDisplayChangeSummaryFlags flags, void *__nullable userInfo) {
	//reinterpret_cast<MacosDisplayConfigManager *>(userInfo)->updateDisplayConfig(nullptr);
}

void MacosDisplayConfigManager::updateDisplayConfig(Function<void(DisplayConfig *)> &&cb) {
	// update edid mappings

	Vector<MacosEdidData> edids;

	io_iterator_t iter;
	io_service_t serv;

	// note that this implemetation for Apple Silicon only
	kern_return_t err = IOServiceGetMatchingServices(kIOMainPortDefault,
			IOServiceMatching("AppleCLCD2"), &iter);
	if (err == 0) {
		while ((serv = IOIteratorNext(iter)) != MACH_PORT_NULL) {
			CFDictionaryRef dict = (CFDictionaryRef)IORegistryEntrySearchCFProperty(serv,
					kIOServicePlane, CFSTR("DisplayAttributes"), kCFAllocatorDefault,
					kIORegistryIterateRecursively);

			auto nsdict = (__bridge_transfer NSDictionary *)dict;

			auto hasEdid = (NSNumber *)nsdict[@"HasHDMILegacyEDID"];
			if (hasEdid && [hasEdid boolValue]) {
				NSLog(@"hasEdid: %@", hasEdid);
				auto productAttributes = (NSDictionary *)nsdict[@"ProductAttributes"];

				MacosEdidData data;
				for (NSString *name in productAttributes) {
					if ([name isEqualToString:@"ManufacturerID"]) {
						data.edid.vendorId = ((NSString *)productAttributes[name]).UTF8String;
						data.edid.vendor = EdidInfo::getVendorName(data.edid.vendorId);
					} else if ([name isEqualToString:@"ProductName"]) {
						data.edid.model = ((NSString *)productAttributes[name]).UTF8String;
					} else if ([name isEqualToString:@"LegacyManufacturerID"]) {
						data.vendorId = ((NSNumber *)productAttributes[name]).unsignedIntegerValue;
					} else if ([name isEqualToString:@"SerialNumber"]) {
						data.serialNumber =
								((NSNumber *)productAttributes[name]).unsignedIntegerValue;
						data.edid.serial = getMonitorIdString(data.serialNumber);
					} else if ([name isEqualToString:@"ProductID"]) {
						data.productId = ((NSNumber *)productAttributes[name]).unsignedIntegerValue;
					}
				}
				edids.emplace_back(data);
			}
		}
		IOObjectRelease(iter);
	}

	Rc<DisplayConfig> info = Rc<DisplayConfig>::create();

	uint32_t displayCount = 0;
	CGGetOnlineDisplayList(0, NULL, &displayCount);

	Vector<CGDirectDisplayID> displays;
	displays.resize(displayCount);
	CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

	for (uint32_t i = 0; i < displayCount; i++) {
		auto displayId = displays[i];
		if (!CGDisplayIsActive(displayId)) {
			continue;
		}

		const uint32_t unitNumber = CGDisplayUnitNumber(displayId);

		NSScreen *screen = nil;

		for (screen in [NSScreen screens]) {
			NSNumber *screenNumber = [screen deviceDescription][@"NSScreenNumber"];
			if (CGDisplayUnitNumber([screenNumber unsignedIntValue]) == unitNumber) {
				break;
			}
		}

		const CGSize size = CGDisplayScreenSize(displayId);

		auto &mon = info->monitors.emplace_back(PhysicalDisplay{
			displayId,
			unitNumber,
			getMonitorId(displayId, screen, edids),
			Extent2{static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height)},
		});

		const NSRect points = [screen frame];
		const NSRect pixels = [screen convertRectToBacking:points];

		auto scaleX = static_cast<float>(pixels.size.width / points.size.width);
		auto scaleY = static_cast<float>(pixels.size.height / points.size.height);

		auto currentMode = CGDisplayCopyDisplayMode(displayId);

		ModeInfo currentModeInfo{
			static_cast<uint32_t>(CGDisplayModeGetPixelWidth(currentMode)),
			static_cast<uint32_t>(CGDisplayModeGetPixelHeight(currentMode)),
			static_cast<uint32_t>(CGDisplayModeGetRefreshRate(currentMode) * 1'000),
			sprt::max(float(CGDisplayModeGetPixelWidth(currentMode))
							/ float(CGDisplayModeGetWidth(currentMode)),
					float(CGDisplayModeGetPixelHeight(currentMode))
							/ float(CGDisplayModeGetHeight(currentMode))),
		};

		CFStringRef keys[1] = {kCGDisplayShowDuplicateLowResolutionModes};
		CFBooleanRef values[1] = {kCFBooleanTrue};

		CFDictionaryRef options =
				CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values,
						1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		CFArrayRef modes = CGDisplayCopyAllDisplayModes(displayId, options);

		CFRelease(options);

		const CFIndex count = CFArrayGetCount(modes);

		bool currentFound = false;

		for (CFIndex i = 0; i < count; i++) {
			CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);

			if (!isDisplayModeAvailable(mode)) {
				continue;
			}

			auto pw = CGDisplayModeGetPixelWidth(mode);
			auto ph = CGDisplayModeGetPixelHeight(mode);
			auto w = CGDisplayModeGetWidth(mode);
			auto h = CGDisplayModeGetHeight(mode);
			auto rate = CGDisplayModeGetRefreshRate(mode) * 1'000;

			auto &m = mon.modes.emplace_back(DisplayMode{
				CGDisplayModeGetIODisplayModeID(mode),
				ModeInfo{
					static_cast<uint32_t>(pw),
					static_cast<uint32_t>(ph),
					static_cast<uint32_t>(CGDisplayModeGetRefreshRate(mode) * 1'000),
					sprt::max(float(pw) / float(w), float(ph) / float(h)),
				},
				"",
				StreamTraits<char>::toString(pw, "x", ph, "@", rate),
			});

			m.current = m.mode == currentModeInfo;
			if (m.current) {
				currentFound = true;
			}
			m.preferred = (CGDisplayModeGetIOFlags(mode) & kDisplayModeDefaultFlag) != 0;
		}

		if (!currentFound) {
			mon.modes.emplace_back(DisplayMode{
				CGDisplayModeGetIODisplayModeID(currentMode),
				currentModeInfo,
				"",
				"",

				Vector<float>(),
				(CGDisplayModeGetIOFlags(currentMode) & kDisplayModeDefaultFlag) != 0,
				true,
			});
		}

		CFRelease(modes);
		CFRelease(currentMode);

		sprt::reverse(mon.modes.begin(), mon.modes.end());

		if (CGDisplayPrimaryDisplay(displayId) == displayId) {
			const NSRect frameRect = [screen visibleFrame];
			info->logical.emplace_back(LogicalDisplay{displayId,
				IRect{static_cast<int32_t>(frameRect.origin.x),
					static_cast<int32_t>(frameRect.origin.y),
					static_cast<uint32_t>(frameRect.size.width),
					static_cast<uint32_t>(frameRect.size.height)},
				sprt::max(scaleX, scaleY), 0, screen == [NSScreen mainScreen],
				Vector<MonitorId> { mon.id }});
		}
	}

	for (auto &it : info->monitors) {
		auto displayId = it.xid.xid;
		auto primary = CGDisplayPrimaryDisplay(displayId);
		if (primary != displayId) {
			for (auto &log : info->logical) {
				if (it.xid.xid == primary) {
					log.monitors.emplace_back(it.id);
					break;
				}
			}
		}
	}

	if (cb) {
		cb(info);
	}
	handleConfigChanged(info);
}

void MacosDisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&cb) {
	updateDisplayConfig(sprt::move(cb));
}

void MacosDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> config,
		Function<void(Status)> &&cb) {
	CGDisplayConfigRef configPtr;
	CGBeginDisplayConfiguration(&configPtr);
	for (auto &it : config->monitors) {
		auto m = _currentConfig->getMonitor(it.id);
		if (m) {
			auto newMode = it.getCurrent();
			if (newMode != m->getCurrent()) {
				CFStringRef keys[1] = {kCGDisplayShowDuplicateLowResolutionModes};
				CFBooleanRef values[1] = {kCFBooleanTrue};

				CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault,
						(const void **)keys, (const void **)values, 1,
						&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

				CFArrayRef modes = CGDisplayCopyAllDisplayModes(it.xid.xid, options);
				const CFIndex count = CFArrayGetCount(modes);

				for (CFIndex i = 0; i < count; i++) {
					CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
					if (CGDisplayModeGetIODisplayModeID(mode) == newMode.xid.xid) {
						CGConfigureDisplayWithDisplayMode(configPtr, it.xid.xid, mode, nullptr);
						break;
					}
				}

				CFRelease(modes);
			}
		}
	}
	CGCompleteDisplayConfiguration(configPtr, kCGConfigureForAppOnly);
	cb(Status::Ok);
	updateDisplayConfig(nullptr);
}

} // namespace sprt::window
