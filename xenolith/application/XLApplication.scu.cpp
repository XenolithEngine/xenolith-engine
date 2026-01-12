/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLCommon.h" // IWYU pragma: keep

#include "XLEvent.cc"
#include "XLWindowInfo.cc"
#include "XLContextInfo.cc"
#include "XLContext.cc"
#include "XLAppThread.cc"
#include "XLAppWindow.cc"
#include "XLLiveReload.cc"
#include "XLLooperAdapter.cc"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static SharedSymbol s_appSymbols[] = {
	SharedSymbol(Context::SymbolContextRunName,
			static_cast<Context::SymbolRunCmdSignature>(Context::run)),
	SharedSymbol(Context::SymbolContextRunName,
			static_cast<Context::SymbolRunNativeSignature>(Context::run)),
};

SP_USED static SharedModule s_appCommonModule(buildconfig::MODULE_XENOLITH_APPLICATION_NAME,
		s_appSymbols, sizeof(s_appSymbols) / sizeof(SharedSymbol));

} // namespace stappler::xenolith
