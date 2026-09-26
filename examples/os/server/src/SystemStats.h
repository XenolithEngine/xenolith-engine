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


#ifndef EXAMPLES_OS_SERVER_SRC_SYSTEMSTATS_H_
#define EXAMPLES_OS_SERVER_SRC_SYSTEMSTATS_H_

#include "WmProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

class DisplayPipe;
class DisplayPlane;

} // namespace stappler::xenolith::compositor

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

// The server's own numbers for the status bar, sampled once a second: each call measures the
// time since the previous one.
class SystemStats {
public:
	const protocol::Stats &update(const compositor::DisplayPipe *,
			const compositor::DisplayPlane *focused, uint32_t apps);

	const protocol::Stats &get() const { return _stats; }

protected:
	protocol::Stats _stats;

	uint64_t _lastTime = 0;
	uint64_t _lastCpu = 0;
	uint64_t _lastHostFrames = 0;
	const compositor::DisplayPlane *_lastPlane = nullptr;
	uint64_t _lastPublished = 0;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SERVER_SRC_SYSTEMSTATS_H_ */
