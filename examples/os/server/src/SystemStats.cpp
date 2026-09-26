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


#include "SystemStats.h"
#include "XLCompositorPipe.h"

#include <fcntl.h>
#include <unistd.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

// A /proc file reports no size, so it is read until EOF rather than through the filesystem API
static StringView SystemStats_readProc(const char *path, char *buf, size_t size) {
	auto fd = ::open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return StringView();
	}
	size_t len = 0;
	while (len < size) {
		auto n = ::read(fd, buf + len, size - len);
		if (n <= 0) {
			break;
		}
		len += size_t(n);
	}
	::close(fd);
	return StringView(buf, len);
}

const protocol::Stats &SystemStats::update(const compositor::DisplayPipe *pipe,
		const compositor::DisplayPlane *focused, uint32_t apps) {
	auto now = sp::platform::clock(ClockType::Monotonic);
	auto cpu = sp::platform::clock(ClockType::Process);
	auto hostFrames = pipe->getHostFrames();
	auto published = focused ? focused->getPublished() : 0;

	if (_lastTime && now > _lastTime) {
		auto dt = float(now - _lastTime) / 1'000'000.0f;
		_stats.cpu = 100.0f * float(cpu - _lastCpu) / float(now - _lastTime);
		_stats.fps = float(hostFrames - _lastHostFrames) / dt;
		_stats.appFps = (focused && focused == _lastPlane && published >= _lastPublished)
				? float(published - _lastPublished) / dt
				: 0.0f;
	}

	// statm: size resident shared ... in pages
	char buf[256];
	auto statm = SystemStats_readProc("/proc/self/statm", buf, sizeof(buf));
	statm.readUntil<StringView::WhiteSpace>();
	statm.skipChars<StringView::WhiteSpace>();
	auto resident = statm.readInteger(10);
	if (resident.valid()) {
		_stats.rss = uint64_t(resident.get()) * uint64_t(::sysconf(_SC_PAGESIZE));
	}

	_stats.apps = apps;
	_lastTime = now;
	_lastCpu = cpu;
	_lastHostFrames = hostFrames;
	_lastPlane = focused;
	_lastPublished = published;
	return _stats;
}

} // namespace stappler::xenolith::wm
