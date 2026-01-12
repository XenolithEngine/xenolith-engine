/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKHEADER_H
#define XENOLITH_BACKEND_VK_XLVKHEADER_H

#include "XLCommon.h"

// we don't need function prototypes, we generate them as tables
#define VK_NO_PROTOTYPES 1

#if MACOS
#define VK_USE_PLATFORM_METAL_EXT 1
#define VK_USE_PLATFORM_MACOS_MVK 1
#define VK_ENABLE_BETA_EXTENSIONS 1
#endif

#if ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR 1
#endif

#if WIN32
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif

#include <vulkan/vulkan.h>

#if LINUX

// When building the engine, we do not have the Wayland and XCB headers,
// here we simulate their presence for the needs of Vulkan

typedef void xcb_connection_t;
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_visualid_t;

#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>

#endif

#endif
