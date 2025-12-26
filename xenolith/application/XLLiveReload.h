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

#ifndef XENOLITH_APPLICATION_XLLIVERELOAD_H_
#define XENOLITH_APPLICATION_XLLIVERELOAD_H_

#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct SP_PUBLIC LiveReloadLibrary : public Ref {
	String path;
	Time mtime;
	Dso library;

	// we need toi release library only after all references are dead
	event::Looper *releaseLooper = nullptr;

	virtual ~LiveReloadLibrary();

	bool init(StringView, Time, uint32_t version, event::Looper *);

	uint32_t getVersion() const { return library.getVersion(); }
};

struct SP_PUBLIC LiveReloadComponent {
	static const ComponentId Id;

	Rc<LiveReloadLibrary> library;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLLIVERELOAD_H_
