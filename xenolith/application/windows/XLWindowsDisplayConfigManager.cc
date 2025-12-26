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

#include "XLWindowsDisplayConfigManager.h"
#include "SPPlatformUnistd.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct WinDisplay {
	WideString adapterName;
	WideString adapterString;
	WideString adapterId;
	WideString adapterKey;
	WideString displayName;
	WideString displayString;
	WideString displayId;
	WideString displayKey;
	int widthMM = 0;
	int heightMM = 0;
	bool isPrimary = true;
	core::ModeInfo current;
};

struct WinPhysicalDisplayHandle {
	HANDLE handle = nullptr;
	core::MonitorId id;
	WideString key;

	~WinPhysicalDisplayHandle() {
		if (handle != nullptr) {
			DestroyPhysicalMonitor(handle);
			handle = nullptr;
		}
	}
};

struct WinDisplayHandle {
	HMONITOR handle = nullptr;
	WideString adapter;
	WideString name;
	WideString id;
	Vector<WinPhysicalDisplayHandle> physical;
	Vector<DisplayMode> modes;
	IRect rect;
	Extent2 mm;
	Vec2 scale = Vec2(1.0f, 1.0f);
	core::ModeInfo current;
	core::ModeInfo preferred;
	uint32_t transform = 0;
	bool isPrimary = false;
};

struct WinDisplayConfig : Ref {
	IRect screenRect;
	Vector<WinDisplayHandle> handles;
};

const GUID GUID_INTERFACE_MONITOR = {0xe6f0'7b5f, 0xee97, 0x4a90,
	{0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7}};

bool WindowsDisplayConfigManager::init(NotNull<WindowsContextController> c,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sp::move(cb))) {
		return false;
	}

	_scalingMode = ScalingMode::DirectScaling;
	_controller = c;

	updateDisplayConfig();

	return true;
}

void WindowsDisplayConfigManager::update() { updateDisplayConfig(); }

static BOOL CALLBACK WindowsDisplayConfigManager_handleMonitors(HMONITOR hMonitor, HDC dc,
		RECT *rect, LPARAM data) {
	auto cfg = reinterpret_cast<WinDisplayConfig *>(data);

	WinDisplayHandle &handle = cfg->handles.emplace_back();
	handle.handle = hMonitor;
	if (rect) {
		handle.rect = IRect{
			rect->left,
			rect->top,
			uint32_t(rect->right - rect->left),
			uint32_t(rect->bottom - rect->top),
		};
	}

	MONITORINFOEXW mi;
	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);

	if (GetMonitorInfoW(hMonitor, (MONITORINFO *)&mi)) {
		handle.name = (char16_t *)mi.szDevice;

		if (hasFlag(mi.dwFlags, DWORD(MONITORINFOF_PRIMARY))) {
			handle.isPrimary = true;
		}

		DWORD cPhysicalMonitors = 0;
		if (GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &cPhysicalMonitors)) {
			Vector<PHYSICAL_MONITOR> handles;
			handles.resize(cPhysicalMonitors);
			if (GetPhysicalMonitorsFromHMONITOR(hMonitor, cPhysicalMonitors, handles.data())) {
				for (auto &it : handles) {
					handle.physical.emplace_back(WinPhysicalDisplayHandle{it.hPhysicalMonitor,
						core::MonitorId{string::toUtf8<Interface>(
								(char16_t *)it.szPhysicalMonitorDescription)}});
				}
			}
		}
	}

	UINT xdpi, ydpi;
	if (GetDpiForMonitor(handle.handle, MDT_EFFECTIVE_DPI, &xdpi, &ydpi) == S_OK) {
		handle.scale =
				Vec2(xdpi / (float)USER_DEFAULT_SCREEN_DPI, ydpi / (float)USER_DEFAULT_SCREEN_DPI);
	}

	return TRUE;
}

static void WindowsDisplayConfigManager_enumDisplayDevices(WinDisplayConfig *config) {
	DWORD adapterIndex, displayIndex;
	DISPLAY_DEVICEW adapter, display;

	for (adapterIndex = 0;; adapterIndex++) {
		ZeroMemory(&adapter, sizeof(adapter));
		adapter.cb = sizeof(adapter);

		if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, EDD_GET_DEVICE_INTERFACE_NAME)) {
			break;
		}

		if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
			continue;
		}

		WinDisplayHandle *handle = nullptr;
		for (auto &it : config->handles) {
			if (it.name == WideStringView((char16_t *)adapter.DeviceName)) {
				handle = &it;
				break;
			}
		}

		if (!handle) {
			continue;
		}

		handle->adapter = (char16_t *)adapter.DeviceName;
		handle->id = string::tolower<Interface>((char16_t *)adapter.DeviceID);

		auto dc = CreateDCW(L"DISPLAY", adapter.DeviceName, NULL, NULL);
		handle->mm = Extent2(GetDeviceCaps(dc, HORZSIZE), GetDeviceCaps(dc, VERTSIZE));
		DeleteDC(dc);

		DEVMODEW dm;
		ZeroMemory(&dm, sizeof(dm));
		dm.dmSize = sizeof(dm);

		if (EnumDisplaySettingsW(adapter.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
			handle->current.width = dm.dmPelsWidth;
			handle->current.height = dm.dmPelsHeight;
			handle->current.rate = dm.dmDisplayFrequency * 1'000;
			if (hasFlag(dm.dmFields, DWORD(DM_DISPLAYORIENTATION))) {
				handle->transform = dm.dmDisplayOrientation;
			}
		}

		if (EnumDisplaySettingsW(adapter.DeviceName, ENUM_REGISTRY_SETTINGS, &dm)) {
			handle->preferred.width = dm.dmPelsWidth;
			handle->preferred.height = dm.dmPelsHeight;
			handle->preferred.rate = dm.dmDisplayFrequency * 1'000;
		}

		DWORD modeNum = 0;
		while (EnumDisplaySettingsW(adapter.DeviceName, modeNum++, &dm)) {
			if (hasFlag(dm.dmFields, DWORD(DM_PELSWIDTH))
					|| hasFlag(dm.dmFields, DWORD(DM_PELSHEIGHT))
					|| hasFlag(dm.dmFields, DWORD(DM_DISPLAYFREQUENCY))) {
				auto mode = core::ModeInfo{
					dm.dmPelsWidth,
					dm.dmPelsHeight,
					dm.dmDisplayFrequency * 1'000,
					1.0f,
				};

				bool found = false;
				for (auto &it : handle->modes) {
					if (it.mode == mode) {
						found = true;
						break;
					}
				}

				if (!found) {
					handle->modes.emplace_back(DisplayMode{
						uintptr_t(modeNum),
						core::ModeInfo{
							dm.dmPelsWidth,
							dm.dmPelsHeight,
							dm.dmDisplayFrequency * 1'000,
							1.0f,
						},
						"",
						toString(dm.dmPelsWidth, "x", dm.dmPelsHeight, "@",
								dm.dmDisplayFrequency * 1'000),
					});
				}
			}
		}

		std::sort(handle->modes.begin(), handle->modes.end(),
				[](const DisplayMode &l, const DisplayMode &r) { return l.mode > r.mode; });

		for (displayIndex = 0;; displayIndex++) {
			ZeroMemory(&display, sizeof(display));
			display.cb = sizeof(display);

			if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display,
						EDD_GET_DEVICE_INTERFACE_NAME)) {
				break;
			}

			if (!(display.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
				continue;
			}

			WinPhysicalDisplayHandle *physical = nullptr;
			for (auto &it : handle->physical) {
				if (it.id.name == string::toUtf8<Interface>((char16_t *)display.DeviceString)) {
					physical = &it;
					break;
				}
			}

			if (!physical) {
				continue;
			}

			physical->key = string::tolower<Interface>((char16_t *)display.DeviceID);
		}
	}
}

static void WindowsDisplayConfigManager_getDeviceInfoRegKey(HDEVINFO hDevInfo,
		PSP_DEVINFO_DATA devInfoData, WinDisplayHandle &adapter,
		WinPhysicalDisplayHandle &display) {

	static constexpr uint32_t ValueDataSize = 512;
	static constexpr uint32_t ValueNameSize = 512;

	uint8_t valueData[ValueDataSize];
	WCHAR valueName[ValueNameSize];

	HKEY hDevRegKey =
			SetupDiOpenDevRegKey(hDevInfo, devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

	if (!hDevRegKey || (hDevRegKey == INVALID_HANDLE_VALUE)) {
		return;
	}

	for (LONG i = 0, retValue = ERROR_SUCCESS; retValue != ERROR_NO_MORE_ITEMS; ++i) {
		DWORD dwType;
		DWORD valueNameLength = ValueNameSize;
		DWORD valueDataLength = ValueDataSize;
		ZeroMemory(valueName, 256 * sizeof(WCHAR));
		retValue = RegEnumValueW(hDevRegKey, i, valueName, &valueNameLength, NULL, &dwType,
				valueData, &valueDataLength);
		if (retValue == ERROR_SUCCESS && WideStringView((char16_t *)valueName) == u"EDID") {
			if (valueDataLength >= 256) {
				display.id.edid = core::EdidInfo::parse(BytesView(valueData, valueDataLength));
			}
		}
	}

	RegCloseKey(hDevRegKey);
}

static void WindowsDisplayConfigManager_enumMonitorDeviceInterface(WinDisplayConfig *config) {
	auto hDevInfo = SetupDiGetClassDevsEx(&GUID_INTERFACE_MONITOR, //class GUID
			NULL, //enumerator
			NULL, //HWND
			DIGCF_PRESENT | DIGCF_DEVICEINTERFACE,
			NULL, // device info, create a new one.
			NULL, // machine name, local machine
			NULL); // reserved

	SP_DEVICE_INTERFACE_DATA ifData = {.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA)};

	SP_DEVINFO_DATA devInfoData;
	ZeroMemory(&devInfoData, sizeof(devInfoData));
	devInfoData.cbSize = sizeof(devInfoData);

	uint32_t i = 0;
	while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_INTERFACE_MONITOR, i, &ifData)) {
		DWORD requiredSize = 0;

		SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, nullptr, 0, &requiredSize, nullptr);

		auto data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)HeapAlloc(GetProcessHeap(),
				HEAP_ZERO_MEMORY, requiredSize);
		data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

		if (SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, data, requiredSize, nullptr,
					&devInfoData)) {

			auto path = string::tolower<Interface>((char16_t *)data->DevicePath);

			for (auto &adapter : config->handles) {
				for (auto &display : adapter.physical) {
					if (display.key == path) {
						WindowsDisplayConfigManager_getDeviceInfoRegKey(hDevInfo, &devInfoData,
								adapter, display);
					}
				}
			}
		}

		HeapFree(GetProcessHeap(), 0, data);
		++i;
	}
}

void WindowsDisplayConfigManager::updateDisplayConfig(
		Function<void(DisplayConfig *_Nullable)> &&cb) {
	auto winInfo = Rc<WinDisplayConfig>::create();

	winInfo->screenRect = IRect{
		GetSystemMetricsForDpi(SM_XVIRTUALSCREEN, USER_DEFAULT_SCREEN_DPI),
		GetSystemMetricsForDpi(SM_YVIRTUALSCREEN, USER_DEFAULT_SCREEN_DPI),
		uint32_t(GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, USER_DEFAULT_SCREEN_DPI)),
		uint32_t(GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, USER_DEFAULT_SCREEN_DPI)),
	};

	EnumDisplayMonitors(nullptr, nullptr, WindowsDisplayConfigManager_handleMonitors,
			(LPARAM)winInfo.get());

	WindowsDisplayConfigManager_enumDisplayDevices(winInfo);
	WindowsDisplayConfigManager_enumMonitorDeviceInterface(winInfo);

	Rc<DisplayConfig> info = Rc<DisplayConfig>::create();

	uint32_t index = 0;
	for (auto &it : winInfo->handles) {
		auto &l = info->logical.emplace_back(LogicalDisplay{
			(void *)it.handle,
			it.rect,
			std::min(it.scale.x, it.scale.y),
			it.transform,
			it.isPrimary,
		});

		for (auto &m : it.physical) {
			l.monitors.emplace_back(m.id);

			auto &p = info->monitors.emplace_back(PhysicalDisplay{
				uintptr_t(0),
				index++,
				m.id,
				it.mm,
			});

			for (auto &mode : it.modes) {
				auto &m = p.modes.emplace_back(mode);
				if (m.mode == it.current) {
					m.current = true;
				}
				if (m.mode == it.preferred) {
					m.preferred = true;
				}
			}
		}
	}

	info->native = winInfo;

	if (cb) {
		cb(info);
	}
	handleConfigChanged(info);
}

void WindowsDisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&cb) {
	updateDisplayConfig(sp::move(cb));
}

void WindowsDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> config,
		Function<void(Status)> &&cb) {
	auto current = extractCurrentConfig(getCurrentConfig());
	auto native = config->native.get_cast<WinDisplayConfig>();

	struct ModeData {
		wchar_t *name = nullptr;
		DEVMODEW dm;
		bool set = false;

		core::ModeInfo getMode() {
			return core::ModeInfo{dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency * 1'000};
		};

		ModeData(wchar_t *n) : name(n) {
			ZeroMemory(&dm, sizeof(dm));
			dm.dmSize = sizeof(dm);
		}
	};

	Vector<ModeData> modesToSet;

	for (auto &it : native->handles) {
		auto l = config->getLogical(it.handle);
		if (!l || l->monitors.size() == 0) {
			continue;
		}

		auto mon = config->getMonitor(l->monitors.front());
		if (!mon) {
			continue;
		}

		auto mode = mon->getCurrent().mode;

		auto &dm = modesToSet.emplace_back(ModeData((wchar_t *)it.adapter.data()));

		if (EnumDisplaySettingsW(dm.name, ENUM_CURRENT_SETTINGS, &dm.dm)) {
			if (mode == dm.getMode()) {
				continue; // no need to switch mode
			}
		}

		bool found = true;
		DWORD modeNum = 0;
		while (EnumDisplaySettingsW((wchar_t *)it.adapter.data(), modeNum++, &dm.dm)) {
			if (hasFlag(dm.dm.dmFields, DWORD(DM_PELSWIDTH))
					|| hasFlag(dm.dm.dmFields, DWORD(DM_PELSHEIGHT))
					|| hasFlag(dm.dm.dmFields, DWORD(DM_DISPLAYFREQUENCY))) {
				dm.dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
				if (mode == dm.getMode()) {
					if (ChangeDisplaySettingsExW(dm.name, &dm.dm, nullptr, CDS_TEST, nullptr)
							== DISP_CHANGE_SUCCESSFUL) {
						dm.set = true;
						found = true;
						break;
					}
				}
			}
		}
		if (!found) {
			if (cb) {
				cb(Status::ErrorInvalidArguemnt);
			}
			return;
		}
	}

	for (auto &dm : modesToSet) {
		if (dm.set) {
			if (ChangeDisplaySettingsExW(dm.name, &dm.dm, nullptr, CDS_FULLSCREEN, nullptr)
					!= DISP_CHANGE_SUCCESSFUL) {
				if (cb) {
					cb(Status::ErrorInvalidArguemnt);
				}
				return;
			}
		}
	}

	if (cb) {
		cb(Status::Ok);
	}
}

} // namespace stappler::xenolith::platform
