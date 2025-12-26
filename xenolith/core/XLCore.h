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

#ifndef XENOLITH_CORE_XLCORE_H_
#define XENOLITH_CORE_XLCORE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPFilesystem.h"
#include "SPString.h"
#include "SPThreadTask.h"
#include "SPSpanView.h"
#include "SPLog.h"
#include "SPHashTable.h"
#include "SPPlatform.h"

#include "SPVec1.h"
#include "SPVec2.h"
#include "SPVec3.h"
#include "SPVec4.h"
#include "SPGeometry.h"
#include "SPQuaternion.h"
#include "SPMat4.h"
#include "SPPadding.h"
#include "SPColor.h"
#include "SPColorHCT.h"
#include "SPFontStyle.h"
#include "SPEvent.h"

#include "XLCoreConfig.h"

#define XL_ASSERT(cond, msg)  do { if (!(cond)) { stappler::log::text(stappler::log::LogType::Fatal, "Assert", msg); } assert((cond)); } while (0)

#ifndef XLASSERT
#if DEBUG
#define XLASSERT(cond, msg) XL_ASSERT(cond, msg)
#else
#define XLASSERT(cond, msg)
#endif
#endif

// check if 64-bit pointer is available for Vulkan
#ifndef XL_USE_64_BIT_PTR_DEFINES
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) \
		|| defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) \
		|| defined(__powerpc64__)
#define XL_USE_64_BIT_PTR_DEFINES 1
#else
#define XL_USE_64_BIT_PTR_DEFINES 0
#endif
#endif

#if __CDT_PARSER__
// IDE-specific definition

// enable all modules

#define MODULE_XENOLITH_CORE 1
#define MODULE_XENOLITH_APPLICATION 1
#define MODULE_XENOLITH_FONT 1
#define MODULE_XENOLITH_BACKEND_VK 1
#define MODULE_XENOLITH_RENDERER_BASIC2D 1
#define MODULE_XENOLITH_RESOURCES_NETWORK 1
#define MODULE_XENOLITH_RESOURCES_ASSETS 1
#define MODULE_XENOLITH_RESOURCES_STORAGE 1

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Import std memory model as default
using namespace mem_std;

using geom::Vec1;
using geom::Vec2;
using geom::Vec3;
using geom::Vec4;
using geom::Mat4;
using geom::Size2;
using geom::Size3;
using geom::Extent2;
using geom::Extent3;
using geom::Rect;
using geom::URect;
using geom::UVec2;
using geom::UVec3;
using geom::UVec4;
using geom::IRect;
using geom::IVec2;
using geom::IVec3;
using geom::IVec4;
using geom::Quaternion;
using geom::Color;
using geom::Color3B;
using geom::Color4B;
using geom::Color4F;
using geom::ColorHCT;
using geom::ColorMask;
using geom::Padding;

namespace Anchor = geom::Anchor;

inline constexpr uint32_t XL_MAKE_API_VERSION(uint32_t variant, uint32_t major, uint32_t minor,
		uint32_t patch) {
	return SP_MAKE_API_VERSION(variant, major, minor, patch);
}

// based on VK_MAKE_API_VERSION
inline uint32_t XL_MAKE_API_VERSION(StringView version) { return SP_MAKE_API_VERSION(version); }

inline String getVersionDescription(uint32_t version) {
	return sp::getVersionDescription<Interface>(version);
}

SP_PUBLIC const char *getEngineName();

SP_PUBLIC const char *getVersionString();

SP_PUBLIC uint32_t getVersionIndex();

SP_PUBLIC uint32_t getVersionVariant();

// API version number
SP_PUBLIC uint32_t getVersionApi();

// Build revision version number
SP_PUBLIC uint32_t getVersionRev();

SP_PUBLIC uint32_t getVersionBuild();

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::profiling {

struct SP_PUBLIC ProfileData {
	uint64_t timestamp;
	StringView tag;
	StringView variant;
	uint64_t limit;
};

SP_PUBLIC ProfileData begin(StringView tag, StringView variant, uint64_t limit);

SP_PUBLIC void end(ProfileData &);
SP_PUBLIC void store(ProfileData &);

#define XL_PROFILE_DEBUG 0

#if XL_PROFILE_DEBUG
#define XL_PROFILE_BEGIN(name, tag, variant, limit) \
	auto __xl_profile__ ## name = stappler::xenolith::profiling::begin(tag, variant, limit);

#define XL_PROFILE_END(name) \
	stappler::xenolith::profiling::end(__xl_profile__ ## name);
#else
#define XL_PROFILE_BEGIN(name, tag, variant, limit)
#define XL_PROFILE_END(name)
#endif

} // namespace stappler::xenolith::profiling

#include "XLCoreEnum.h"
#include "XLCorePipelineInfo.h"

#endif /* XENOLITH_CORE_XLCORE_H_ */
