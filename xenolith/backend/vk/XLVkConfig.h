/**
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_BACKEND_VK_XLVKCONFIG_H_
#define XENOLITH_BACKEND_VK_XLVKCONFIG_H_

#include "SPCore.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::config {

#if ANDROID
static constexpr bool UseExternalFenceSync = false;
#else
static constexpr bool UseExternalFenceSync = true;
#endif

/* Number of frames, that can be performed in suboptimal swapchain modes */
static constexpr uint32_t MaxSuboptimalFrames = 24;

} // namespace stappler::xenolith::config

#endif /* XENOLITH_BACKEND_VK_XLVKCONFIG_H_ */
