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
#include "XLSoft.h"

// Kernels first: the triangle setup calls into them, and in an SCU that means the definitions
// have to be seen before the caller.
#include "raster/XLSoftRasterSample.cc"
#include "raster/XLSoftRasterKernels.cc"
#include "raster/XLSoftRasterSetup.cc"

#include "XLSoftInstance.cc"
#include "XLSoftDevice.cc"
#include "XLSoftObject.cc"
#include "XLSoftPipeline.cc"
#include "XLSoftTextureSet.cc"
#include "XLSoftMaterial.cc"
#include "XLSoftQueuePass.cc"
#include "XLSoftPresentation.cc"
#include "XLSoftLoop.cc"
#include "XLSoftPlatform.cc"

#include "SPSharedModule.h"
#include "XLSoftPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

static SharedSymbol s_softSharedSymbols[] = {
	SharedSymbol("platform::createInstance", platform::createInstance),
};

SP_USED static SharedModule s_softSharedModule(buildconfig::MODULE_XENOLITH_BACKEND_SOFT_NAME,
		s_softSharedSymbols, sizeof(s_softSharedSymbols) / sizeof(SharedSymbol));

} // namespace stappler::xenolith::soft
