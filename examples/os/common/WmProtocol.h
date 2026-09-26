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


#ifndef EXAMPLES_OS_COMMON_WMPROTOCOL_H_
#define EXAMPLES_OS_COMMON_WMPROTOCOL_H_

#include "XLCommon.h"

/* The window manager's messages: AppMessage values between the server and its shell and shade.
Every message is a dict with the type in `t`; the builders and readers below are the only place
the keys are spelled, so the three programs cannot drift apart. */

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm::protocol {

// The server's environment
static constexpr const char *EnvAddress = "XL_WM_ADDRESS"; // shm:... to listen on
static constexpr const char *EnvCatalog = "XL_WM_CATALOG"; // a JSON file replacing the catalog
static constexpr const char *EnvApps = "XL_WM_APPS"; // `;`-separated commands: the stack mode
static constexpr const char *EnvFps = "XL_WM_FPS"; // the vblank rate of the planes
static constexpr const char *EnvBackground = "XL_WM_BG"; // RRGGBB behind the planes

// The status bar at the top of the screen, in points; the shade draws it, the planes stay below.
static constexpr float StatusBarHeight = 32.0f;

static constexpr StringView TypeKey = "t";

static constexpr StringView Catalog = "catalog"; // shell -> server, request
static constexpr StringView Launch = "launch"; // shell or shade -> server: { id }
static constexpr StringView ListApps = "list-apps"; // shade -> server, request
static constexpr StringView Apps = "apps"; // server -> shade: the running applications changed
static constexpr StringView Status = "status"; // server -> shade, every second
static constexpr StringView Home = "home"; // shade -> server
static constexpr StringView Switch = "switch"; // shade -> server: { label }
static constexpr StringView Close = "close"; // shade -> server: { label }
static constexpr StringView Shade = "shade"; // shade -> server: { open }
static constexpr StringView InputRegion = "input-region"; // shade -> server: { rects }

// An application the shell can launch.
struct CatalogEntry {
	String id;
	String title;
	String icon; // a basic2d::IconName name
	bool available = true; // built and found
};

// A launched application, as the shade lists it.
struct RunningApp {
	String label;
	String id;
	String title;
	String icon;
	bool active = false;
};

struct Stats {
	uint32_t apps = 0; // running applications
	float cpu = 0.0f; // the server's CPU, percent of one core
	uint64_t rss = 0; // the server's resident memory, bytes
	float fps = 0.0f; // host frames per second
	float appFps = 0.0f; // frames per second of the application in focus
};

inline StringView getType(const Value &v) { return v.getString(TypeKey); }

inline Value makeMessage(StringView type) {
	Value ret;
	ret.setString(type, TypeKey);
	return ret;
}

inline Value makeLaunch(StringView id) {
	auto ret = makeMessage(Launch);
	ret.setString(id, "id");
	return ret;
}

// switch, close
inline Value makeLabelled(StringView type, StringView label) {
	auto ret = makeMessage(type);
	ret.setString(label, "label");
	return ret;
}

inline Value makeShade(bool open) {
	auto ret = makeMessage(Shade);
	ret.setBool(open, "open");
	return ret;
}

// Rects in the shade window's pixels, y down.
inline Value makeInputRegion(SpanView<IRect> rects) {
	auto ret = makeMessage(InputRegion);
	auto &list = ret.emplace("rects");
	list.setArray(Value::ArrayType());
	for (auto &it : rects) {
		list.addValue(Value({Value(int64_t(it.x)), Value(int64_t(it.y)), Value(int64_t(it.width)),
			Value(int64_t(it.height))}));
	}
	return ret;
}

inline Vector<IRect> readInputRegion(const Value &v) {
	Vector<IRect> ret;
	for (auto &it : v.getArray("rects")) {
		ret.emplace_back(IRect(int32_t(it.getInteger(0)), int32_t(it.getInteger(1)),
				uint32_t(it.getInteger(2)), uint32_t(it.getInteger(3))));
	}
	return ret;
}

inline Value makeCatalog(SpanView<CatalogEntry> entries) {
	auto ret = makeMessage(Catalog);
	auto &list = ret.emplace("apps");
	list.setArray(Value::ArrayType());
	for (auto &it : entries) {
		Value entry;
		entry.setString(it.id, "id");
		entry.setString(it.title, "title");
		entry.setString(it.icon, "icon");
		entry.setBool(it.available, "available");
		list.addValue(sp::move(entry));
	}
	return ret;
}

inline Vector<CatalogEntry> readCatalog(const Value &v) {
	Vector<CatalogEntry> ret;
	for (auto &it : v.getArray("apps")) {
		ret.emplace_back(CatalogEntry{it.getString("id"), it.getString("title"),
			it.getString("icon"), it.getBool("available")});
	}
	return ret;
}

// The reply to list-apps, and the apps notification: the same list.
inline Value makeApps(StringView type, SpanView<RunningApp> apps) {
	auto ret = makeMessage(type);
	auto &list = ret.emplace("apps");
	list.setArray(Value::ArrayType());
	for (auto &it : apps) {
		Value entry;
		entry.setString(it.label, "label");
		entry.setString(it.id, "id");
		entry.setString(it.title, "title");
		entry.setString(it.icon, "icon");
		entry.setBool(it.active, "active");
		list.addValue(sp::move(entry));
	}
	return ret;
}

inline Vector<RunningApp> readApps(const Value &v) {
	Vector<RunningApp> ret;
	for (auto &it : v.getArray("apps")) {
		ret.emplace_back(RunningApp{it.getString("label"), it.getString("id"),
			it.getString("title"), it.getString("icon"), it.getBool("active")});
	}
	return ret;
}

inline Value makeStatus(const Stats &stats) {
	auto ret = makeMessage(Status);
	ret.setInteger(stats.apps, "apps");
	ret.setDouble(stats.cpu, "cpu");
	ret.setInteger(int64_t(stats.rss), "rss");
	ret.setDouble(stats.fps, "fps");
	ret.setDouble(stats.appFps, "appFps");
	return ret;
}

inline Stats readStatus(const Value &v) {
	return Stats{uint32_t(v.getInteger("apps")), float(v.getDouble("cpu")),
		uint64_t(v.getInteger("rss")), float(v.getDouble("fps")), float(v.getDouble("appFps"))};
}

} // namespace stappler::xenolith::wm::protocol

#endif /* EXAMPLES_OS_COMMON_WMPROTOCOL_H_ */
