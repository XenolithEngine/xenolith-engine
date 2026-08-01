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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMDEVICE_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMDEVICE_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/display_config.h>

#include "SPRTWinLinuxDrmLibrary.h"

// DrmDevice holds the library behind an Rc, never by value, and every other
// member is a plain type. So this class - and DisplayWindow / LinuxContextController
// which embed it - keeps the same layout whether or not SPRT_HAS_LIBDRM is set;
// only what the functions return changes. (Rc needs the complete type to release
// it, hence the include rather than a forward declaration.)

namespace sprt::window {

// An open DRM primary node with a resolved output target.
//
// This owns the fd for the whole lifetime of the window system: the gAPI acquires
// the display through it (vkAcquireDrmDisplayEXT) and stays DRM master until the
// device is released, so the fd must outlive every surface built from it.
//
// Where libdrm is unavailable (SPRT_HAS_LIBDRM == 0) the class still exists with
// the same layout; openFirst() simply never returns an instance.
class SPRT_API DrmDevice : public Ref {
public:
	// Opens the first DRM primary node that has a connected connector with modes.
	// Returns null when there is none (no KMS output => no direct-display mode).
	static Rc<DrmDevice> openFirst(NotNull<DrmLibrary>);

	virtual ~DrmDevice();

	DrmDevice() { }

	bool init(NotNull<DrmLibrary>, StringView path);

	int getFd() const { return _fd; }
	StringView getPath() const { return _path; }

	uint32_t getConnectorId() const { return _connectorId; }
	uint32_t getCrtcId() const { return _crtcId; }

	// Target mode, resolved once at init() - see resolveTarget()
	ModeInfo getMode() const { return _mode; }

	// Full monitor/mode enumeration for DisplayConfigManager
	bool readConfig(NotNull<DisplayConfig>) const;

	// Dump every connector + its modes. Diagnostic only: HW logs need to show
	// whether we painted the wrong HDMI (e.g. HDMI-A-1 @640x480 vs real
	// HDMI-A-2 @1080p).
	void logConnectors() const;

protected:
	// Find the first connected connector with at least one mode, then pick the
	// mode: kernel's current modeset -> EDID preferred/largest -> fb0.
	//
	// Do NOT just take the largest advertised mode: QEMU virtio-gpu advertises
	// bogus EDID modes (e.g. 5120x2160) that would OOM the swapchain.
	bool resolveTarget();

	Rc<DrmLibrary> _drm;
	String _path;
	int _fd = -1;

	uint32_t _connectorId = 0;
	uint32_t _crtcId = 0;
	ModeInfo _mode;
};

} // namespace sprt::window

#endif // SPRT_LINUX

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_DRM_SPRTWINLINUXDRMDEVICE_H_ */
