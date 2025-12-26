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

#include "XLLiveReload.h"
#include "SPEventLooper.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

LiveReloadLibrary::~LiveReloadLibrary() {
	if (releaseLooper) {
		auto tmp = new (std::nothrow) Dso(sp::move(library));
		releaseLooper->performOnThread([looper = releaseLooper, tmp, path = sp::move(path)] {
			looper->schedule(TimeInterval::milliseconds(100),
					[tmp, path = sp::move(path)](event::Handle *, bool) {
				tmp->close();
				delete tmp;
				filesystem::remove(FileInfo(path));
			});
		}, nullptr, false);
	}
}

bool LiveReloadLibrary::init(StringView p, Time time, uint32_t version, event::Looper *looper) {
	library = Dso(p, version);
	if (library) {
		path = p.str<Interface>();
		mtime = time;
		releaseLooper = looper;
		return true;
	} else {
		slog().debug("Context", "Fail to open Live reloadlibrary: ", p, ": ", library.getError());
	}
	return false;
}

const ComponentId LiveReloadComponent::Id;

} // namespace stappler::xenolith
