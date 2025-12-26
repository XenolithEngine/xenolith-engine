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

#ifndef XENOLITH_APPLICATION_PLATFORM_XLDISPLAYCONFIGMANAGER_H_
#define XENOLITH_APPLICATION_PLATFORM_XLDISPLAYCONFIGMANAGER_H_

#include "XlCoreMonitorInfo.h"
#include "XLContextInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct NativeId {
	union {
		uintptr_t xid = 0; // mode id
		void *ptr;
	};

	NativeId(uintptr_t id) : xid(id) { }
	NativeId(void *ptr) : ptr(ptr) { }

	operator uintptr_t() const { return xid; }
	operator void *() const { return ptr; }

	bool operator==(const NativeId &other) const {
		return memcmp(&xid, &other.xid, sizeof(uintptr_t)) == 0;
	}
	bool operator!=(const NativeId &other) const {
		return memcmp(&xid, &other.xid, sizeof(uintptr_t)) != 0;
	}
	bool operator==(const uintptr_t &other) const { return xid == other; }
	bool operator!=(const uintptr_t &other) const { return xid != other; }
	bool operator==(void *other) const { return ptr == other; }
	bool operator!=(void *other) const { return ptr != other; }
};

struct DisplayMode {
	static DisplayMode None;

	NativeId xid = uintptr_t(0);
	core::ModeInfo mode;
	String id;
	String name;

	Vector<float> scales;

	bool preferred = false;
	bool current = false;

	bool operator==(const DisplayMode &m) const noexcept = default;
	bool operator==(const core::ModeInfo &m) const noexcept { return mode == m; }
};

struct PhysicalDisplay {
	NativeId xid = uintptr_t(0);
	uint32_t index = 0;
	MonitorId id;
	Extent2 mm;
	Vector<DisplayMode> modes;

	const DisplayMode *getMode(const core::ModeInfo &m) const;
	const DisplayMode &getCurrent() const;

	bool operator==(const PhysicalDisplay &m) const noexcept = default;
	bool operator!=(const PhysicalDisplay &m) const noexcept = default;
};

struct LogicalDisplay {
	NativeId xid = uintptr_t(0);
	IRect rect;
	float scale = 1.0f;
	uint32_t transform = 0;
	bool primary = false;
	Vector<MonitorId> monitors;

	bool hasMonitor(const MonitorId &id) const;

	bool operator==(const LogicalDisplay &m) const noexcept = default;
};

struct DisplayConfig : public Ref {
	uint32_t serial = 0;
	IRect desktopRect;
	Vector<PhysicalDisplay> monitors;
	Vector<LogicalDisplay> logical;

	// OS-native config
	Rc<Ref> native;
	Time time = Time::now();

	const PhysicalDisplay *getMonitor(const MonitorId &id) const;
	const LogicalDisplay *getLogical(const MonitorId &id) const;
	const LogicalDisplay *getLogical(const NativeId &id) const;

	bool isEqual(const DisplayConfig *) const;

	Extent2 getSize() const;
	Extent2 getSizeMM() const;
};

class DisplayConfigManager : public Ref {
public:
	virtual ~DisplayConfigManager() = default;

	virtual bool init(Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate();

	void exportScreenInfo(NotNull<core::ScreenInfo>) const;

	// Set mode for the monitor, and reset modes for all other monitors to default
	// Only single monitor mode can be set with this function
	virtual void setModeExclusive(xenolith::MonitorId, xenolith::ModeInfo,
			Function<void(Status)> &&, Ref *);

	virtual void setMode(xenolith::MonitorId, xenolith::ModeInfo, Function<void(Status)> &&, Ref *);

	// Reset monitor modes to captured defaults (modes before first setMonitorMode call)
	virtual void restoreMode(Function<void(Status)> &&, Ref *);

	bool hasSavedMode() const { return _savedConfig; }

	const DisplayConfig *getCurrentConfig() const { return _currentConfig; }

protected:
	// Как применяется изменение размера для конфигурации
	// См. реализацию `adjustDisplay`
	enum ScalingMode {
		// При пост-скейлинге конфигурация расчитывается для целочисленных параметров
		// размерности, то есть, параметры режима умножаются на (std::ceil(scale) / scale).
		// Такие конфигураторы сперва увеличивают изображение на целое значение (2, 3, ... раза)
		// и снижают разрешение после отрисовки (Wayland)
		PostScaling,

		// При прямом скейлинге значение scale используется для опредления нового размера непосредственно
		// Такие конфигураторы сразу рисуют буферы с нужным размером, либо получают уже увеличенные буферы (XRandR)
		DirectScaling,
	};

	// extract only current modes
	Rc<DisplayConfig> extractCurrentConfig(NotNull<const DisplayConfig>) const;
	void adjustDisplay(NotNull<DisplayConfig>) const;

	virtual void handleConfigChanged(NotNull<DisplayConfig>);

	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&);
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&);

	Function<void(NotNull<DisplayConfigManager>)> _onConfigChanged;
	Vector<Function<void()>> _waitForConfigNotification;
	Rc<DisplayConfig> _currentConfig;
	Rc<DisplayConfig> _savedConfig;
	ScalingMode _scalingMode = PostScaling;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_PLATFORM_XLDISPLAYCONFIGMANAGER_H_
