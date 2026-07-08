/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/
#ifndef SPRT_RUNTIME_WINDOW_WASM_WINDOW_H
#define SPRT_RUNTIME_WINDOW_WASM_WINDOW_H

#include <sprt/runtime/window/native_window.h>

#if SPRT_WASM

namespace sprt::window {

class WasmContextController;

// Browser window: a single OffscreenCanvas the JS binding owns. The WebGPU surface is
// created from it via wgpuInstanceCreateSurface (SurfaceBackend::Canvas).
class WasmWindow : public NativeWindow {
public:
	virtual ~WasmWindow();
	WasmWindow();

	bool init(NotNull<WasmContextController>, Rc<WindowInfo> &&, WindowCapabilities);

	virtual void mapWindow() override { }
	virtual void unmapWindow() override { }
	virtual bool close() override { return true; }

	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;
	// WebGPU has no surface-extent query, so the backend returns a maxOf() sentinel and relies
	// on the window to patch in the real size (see Surface::getSurfaceOptions).
	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&info) const override;
	virtual Extent2 getExtent() const override { return _extent; }
	virtual void cancelTextInput() override { }
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags = TextInputFlags::RunIfDisabled) override { return false; }

protected:
	Extent2 _extent;
	float _density = 1.0f;
};

} // namespace sprt::window

#endif // SPRT_WASM
#endif
