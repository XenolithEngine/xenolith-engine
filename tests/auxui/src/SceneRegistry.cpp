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

#include "XLCommon.h"

#include "SceneRegistry.h"

#include <sprt/cxx/mutex>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {
// Process-wide registry. One mutex: the registry is touched from the app thread
// at registration time and from the auxiliary scene's app-thread setup at take
// time, but the same window is never registered and taken concurrently.
sprt::mutex s_mutex;
Map<String, SceneRegistry::Builder> s_builders;
} // namespace

void SceneRegistry::set(StringView id, Builder &&b) {
	sprt::unique_lock lock(s_mutex);
	s_builders[id.str<mem_std::Interface>()] = sprt::move(b);
}

SceneRegistry::Builder SceneRegistry::take(StringView id) {
	sprt::unique_lock lock(s_mutex);
	auto it = s_builders.find(id);
	if (it == s_builders.end()) {
		return nullptr;
	}
	auto out = sprt::move(it->second);
	s_builders.erase(it);
	return out;
}

void SceneRegistry::erase(StringView id) {
	sprt::unique_lock lock(s_mutex);
	s_builders.erase(id.str<mem_std::Interface>());
}

} // namespace stappler::xenolith::app
