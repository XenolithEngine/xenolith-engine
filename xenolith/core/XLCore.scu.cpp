/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_CORE_XLCORE_CPP_
#define XENOLITH_CORE_XLCORE_CPP_

#include "XLCommon.h"

#include "XLCoreEnum.cc"
#include "XLCoreInfo.cc"
#include "XLCoreObject.cc"
#include "XLCoreDevice.cc"
#include "XLCoreDeviceQueue.cc"
#include "XLCoreInstance.cc"
#include "XLCoreLoop.cc"
#include "XLCoreDynamicImage.cc"
#include "XLCoreMaterial.cc"
#include "XLCoreMesh.cc"
#include "XLCoreSwapchain.cc"
#include "XLCoreTextureSet.cc"

#include "XLCorePresentationFrame.cc"
#include "XLCorePresentationEngine.cc"
#include "XLCoreTextInput.cc"

#include "SPMetastring.h"

#define XENOLITH_VERSION_VARIANT 0

namespace STAPPLER_VERSIONIZED stappler::xenolith {

const char *getEngineName() { return "Stappler/Xenolith"; }

const char *getVersionString() {
	static auto versionString = metastring::merge(
			metastring::numeric<size_t(XENOLITH_VERSION_VARIANT)>(), metastring::metastring<'.'>(),
			metastring::numeric<size_t(buildconfig::XENOLITH_VERSION_API)>(),
			metastring::metastring<'.'>(),
			metastring::numeric<size_t(buildconfig::XENOLITH_VERSION_REV)>(),
			metastring::metastring<'.'>(),
			metastring::numeric<size_t(buildconfig::XENOLITH_VERSION_BUILD)>(),
			metastring::metastring<char(0)>())
										.to_array();

	return versionString.data();
}

uint32_t getVersionIndex() {
	return SP_MAKE_API_VERSION(XENOLITH_VERSION_VARIANT, buildconfig::XENOLITH_VERSION_API,
			buildconfig::XENOLITH_VERSION_REV, buildconfig::XENOLITH_VERSION_BUILD);
}

uint32_t getVersionVariant() { return XENOLITH_VERSION_VARIANT; }

uint32_t getVersionApi() { return buildconfig::XENOLITH_VERSION_API; }

uint32_t getVersionRev() { return buildconfig::XENOLITH_VERSION_REV; }

uint32_t getVersionBuild() { return buildconfig::XENOLITH_VERSION_BUILD; }

} // namespace stappler::xenolith

#endif /* XENOLITH_CORE_XLCORE_CPP_ */
