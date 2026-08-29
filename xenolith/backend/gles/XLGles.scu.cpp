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
#include "XLGles.h"

#include "XLGlesTable.cc"
#include "XLGlesObject.cc"
#include "XLGlesPipeline.cc"
#include "XLGlesDevice.cc"
#include "XLGlesTextureSet.cc"
#include "XLGlesQueuePass.cc"
#include "XLGlesPresentation.cc"
#include "XLGlesHeadlessPresentation.cc"
#include "XLGlesWindowedPresentation.cc"
#include "XLGlesLoop.cc"
#include "XLGlesInstance.cc"

#include "platform/linux/XLGlesPlatformLinux.cc"
#include "platform/android/XLGlesPlatformAndroid.cc"
#include "platform/win32/XLGlesPlatformWin32.cc"
#include "platform/macos/XLGlesPlatformMac.cc"

#include "SPSharedModule.h"
#include "XLGlesPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

static SharedSymbol s_glesSharedSymbols[] = {
	SharedSymbol("platform::createInstance", platform::createInstance),
};

SP_USED static SharedModule s_glesSharedModule(buildconfig::MODULE_XENOLITH_BACKEND_GLES_NAME,
		s_glesSharedSymbols, sizeof(s_glesSharedSymbols) / sizeof(SharedSymbol));

} // namespace stappler::xenolith::gles
