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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMLIBRARY_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMLIBRARY_H_

#include <sprt/runtime/init.h>

#include "../SPRTWinLinux.h"

#if SPRT_LINUX

#include <sprt/runtime/ref.h>

#if SPRT_HAS_LIBDRM
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

namespace sprt::window {

// libdrm bindings, loaded from libdrm.so.2 at runtime.
//
// libdrm is not linked: it is dlopen'd like libxcb/libwayland-client, so a build
// without a DRM-capable system still runs. Everything we need lives in the single
// libdrm.so, so there is one Dso and one mandatory function block; symbols that
// only exist in newer releases go into a separate optional block so an older
// libdrm still loads.
//
// This is the ONE class whose shape depends on SPRT_HAS_LIBDRM: the proto table
// needs the libdrm declarations to exist. It is confined to linux/drm, which is a
// single translation unit (SPRTWinLinuxDrm.cpp), and it is never embedded by
// value anywhere - DrmDevice holds it behind an Rc and forward-declares it. So no
// class visible outside this directory changes layout with the macro.
class SPRT_API DrmLibrary : public Ref {
public:
	virtual ~DrmLibrary();

	DrmLibrary() { }

	// False where libdrm is unavailable at build time or cannot be dlopen'd
	bool init();

	// Connector type name, with a local fallback table for older libdrm
	// (drmModeGetConnectorTypeName appeared in libdrm 2.4.112).
	StringView getConnectorTypeName(uint32_t connectorType) const;

#if SPRT_HAS_LIBDRM
	bool open(Dso &handle);

	decltype(&_null_fn) _drm_first_fn = &_null_fn;
	SPRT_DEFINE_PROTO(drmGetDevices2)
	SPRT_DEFINE_PROTO(drmFreeDevices)
	SPRT_DEFINE_PROTO(drmGetVersion)
	SPRT_DEFINE_PROTO(drmFreeVersion)
	SPRT_DEFINE_PROTO(drmModeGetResources)
	SPRT_DEFINE_PROTO(drmModeFreeResources)
	SPRT_DEFINE_PROTO(drmModeGetConnector)
	SPRT_DEFINE_PROTO(drmModeFreeConnector)
	SPRT_DEFINE_PROTO(drmModeGetEncoder)
	SPRT_DEFINE_PROTO(drmModeFreeEncoder)
	SPRT_DEFINE_PROTO(drmModeGetCrtc)
	SPRT_DEFINE_PROTO(drmModeFreeCrtc)
	SPRT_DEFINE_PROTO(drmModeGetProperty)
	SPRT_DEFINE_PROTO(drmModeFreeProperty)
	SPRT_DEFINE_PROTO(drmModeGetPropertyBlob)
	SPRT_DEFINE_PROTO(drmModeFreePropertyBlob)
	decltype(&_null_fn) _drm_last_fn = &_null_fn;

	// Optional: absent on libdrm < 2.4.112, use getConnectorTypeName() instead
	SPRT_DEFINE_PROTO(drmModeGetConnectorTypeName)
#endif

protected:
	Dso _handle;
};

} // namespace sprt::window

#endif // SPRT_LINUX

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMLIBRARY_H_ */
