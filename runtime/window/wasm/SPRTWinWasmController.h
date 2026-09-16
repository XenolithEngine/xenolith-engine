/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/
#ifndef SPRT_RUNTIME_WINDOW_WASM_CONTROLLER_H
#define SPRT_RUNTIME_WINDOW_WASM_CONTROLLER_H

#include <sprt/runtime/window/controller.h>
#include <sprt/runtime/dispatch/handle.h>

#if SPRT_WASM

namespace sprt::window {

// Browser context controller: drives the engine loop on the worker's dispatch::Looper and
// hosts a single OffscreenCanvas window. No native windowing — the canvas + WebGPU come
// from the JS host; pointer/key events arrive via sprt.input_poll (WasmWindow).
class WasmContextController : public ContextController {
public:
	static Rc<WasmContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<dispatch::Looper>);

	virtual ~WasmContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<dispatch::Looper>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override { return false; }
	virtual WindowCapabilities getCapabilities() const override;
	virtual void openUrl(StringView) override;

protected:
	virtual bool loadWindow(Rc<WindowInfo> &&) override;

	// There is no window-system fd to poll. Linux/XCB/Wayland call notifyPendingWindows
	// from their display socket; wasm has to drive that itself so WasmWindow::pollHostInput
	// sees live display_size (resize) and input_poll (pointer/keys).
	static void onHostPoll(WasmContextController *, dispatch::TimerHandle *, uint32_t, Status);

	Rc<dispatch::TimerHandle> _pollTimer;
};

} // namespace sprt::window

#endif // SPRT_WASM
#endif
