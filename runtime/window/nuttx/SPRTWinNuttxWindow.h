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

#ifndef RUNTIME_WINDOW_NUTTX_SPRTWINNUTTXWINDOW_H_
#define RUNTIME_WINDOW_NUTTX_SPRTWINNUTTXWINDOW_H_

#include <sprt/runtime/window/native_window.h>
#include <sprt/runtime/window/software_surface.h>

#if SPRT_NUTTX

namespace sprt::window {

class NuttxContextController;
class NuttxWindow;

// CPU presentation into the mmap'd framebuffer. virtio-gpu on qemu-armv8a is
// B8G8R8X8, which is the layout the software rasterizer already produces.
// The swapchain slot is a CPU shadow: rasterizing into live HDMI scanout
// (bcm2711 mailbox FB) shows the white scene clear as a flash every frame.
// present() copies the finished frame, then FBIO_UPDATE (ENOTTY on bcm2711).
class NuttxSoftwareSurface final : public SoftwareSurface {
public:
	virtual ~NuttxSoftwareSurface();

	bool init(NotNull<NuttxWindow>);

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;
	virtual Rc<SoftwareSwapchain> makeSwapchain(const SoftwareSwapchainInfo &) override;
	virtual void invalidate() override;

protected:
	NuttxWindow *_owner = nullptr;
};

class NuttxSoftwareSwapchain final : public SoftwareSwapchain {
public:
	virtual ~NuttxSoftwareSwapchain();

	bool init(NotNull<NuttxWindow>, const SoftwareSwapchainInfo &);

	virtual Status present(uint32_t index, SpanView<geom::URect> damage) override;
	virtual void invalidate() override;

protected:
	NuttxWindow *_owner = nullptr;
	Extent2 _extent;
	uint8_t *_shadow = nullptr;
	size_t _shadowSize = 0;
};

class NuttxWindow : public NativeWindow {
public:
	virtual ~NuttxWindow();
	NuttxWindow();

	bool init(NotNull<NuttxContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override { }
	virtual void unmapWindow() override { }
	virtual bool close() override;

	virtual Extent2 getExtent() const override { return _extent; }
	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;
	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;
	virtual Rc<SoftwareSurface> makeSoftwareSurface() override;
	virtual PresentationOptions getPreferredOptions() const override;

	int getFd() const { return _fd; }
	uint8_t *getMapping() const { return _mapping; }
	uint32_t getStride() const { return _stride; }
	size_t getMappingSize() const { return _mappingSize; }

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags = TextInputFlags::RunIfDisabled) override {
		return false;
	}
	virtual void cancelTextInput() override { }

	void teardown();

	int _fd = -1;
	uint8_t *_mapping = nullptr;
	size_t _mappingSize = 0;
	uint32_t _stride = 0;
	Extent2 _extent;
	bool _closed = false;
};

} // namespace sprt::window

#endif // SPRT_NUTTX

#endif // RUNTIME_WINDOW_NUTTX_SPRTWINNUTTXWINDOW_H_
