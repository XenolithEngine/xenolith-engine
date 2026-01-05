/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_PRIVATE_SPRTFILESYSTEM_H_
#define CORE_RUNTIME_PRIVATE_SPRTFILESYSTEM_H_

#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/runtime/filesystem/lookup.h>

namespace sprt::filesystem::detail {

struct LookupData : public memory::AllocPool {
	static LookupData *get();

	bool _initialized = false;
	bool _appPathCommon = false;

	memory::pool_t *_pool = nullptr;
	array<LookupInfo, toInt(LocationCategory::Max)> _resourceLocations;

	// Init application paths as executable-relative fallbacks
	void initAppPaths(StringView execDir);
};

SPRT_LOCAL StringView _readEnvExt(memory::pool_t *pool, StringView key);

SPRT_LOCAL void _initSystemPaths(LookupData &);
SPRT_LOCAL void _termSystemPaths(LookupData &);

} // namespace sprt::filesystem::detail

#endif // CORE_RUNTIME_PRIVATE_SPRTFILESYSTEM_H_
