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

// The whole backend compiles as this single Objective-C++ TU (with ARC):
// individual .cc sources are never compiled directly (the build globs only
// *.cpp / *.mm), so their ObjC code inherits this file's language mode.

// This TU mixes sprt headers with Apple SDK headers (Metal/QuartzCore).
// __SPRT_BUILD switches the runtime's include_libc wrappers to #include_next
// into the real SDK libc (and namespaces the sprt type shims), which is the
// only supported way to see both in one TU - same as the runtime's own .mm
// sources. Must come before any include.
#define __SPRT_BUILD 1

#include "XLCommon.h"
#include "XLMtl.h"

#include "XLMtlInstance.cc"
#include "XLMtlDevice.cc"
#include "XLMtlPipeline.cc"
#include "XLMtlObject.cc"
#include "XLMtlTextureSet.cc"
#include "XLMtlMaterial.cc"
#include "XLMtlQueuePass.cc"
#include "XLMtlPresentation.cc"
#include "XLMtlLoop.cc"
#include "XLMtlPlatform.cc"

#include "SPSharedModule.h"
#include "XLMtlPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

static SharedSymbol s_mtlSharedSymbols[] = {
	SharedSymbol("platform::createInstance", platform::createInstance),
};

SP_USED static SharedModule s_mtlSharedModule(buildconfig::MODULE_XENOLITH_BACKEND_MTL_NAME,
		s_mtlSharedSymbols, sizeof(s_mtlSharedSymbols) / sizeof(SharedSymbol));

} // namespace stappler::xenolith::mtl
