// Xenolith wasm main-thread broker + worker hub.
//
// The engine runs in a dedicated worker over a shared (SharedArrayBuffer) linear memory;
// threads run as additional workers over that same memory. Crucially, ALL worker creation
// happens here on the main thread: a worker that calls thread_spawn immediately blocks in
// Atomics.wait, and a worker cannot bring up a child worker while its own event loop is
// frozen. So spawn requests are posted to the main thread, whose event loop is always free,
// and it creates the thread worker. This also lets nested spawns (a thread creating a
// thread) route back here uniformly.
//
// run(wasmUrl, opts) -> Promise<exitCode>  (also .stop())
//   opts.onStdout / onStderr / onExit, opts.bundle (mount->url), opts.argv0,
//   opts.args (extra argv entries after argv0, e.g. a test-suite name)
//   opts.canvas: an on-page <canvas> whose control is transferred to the engine worker as an
//   OffscreenCanvas (the engine's WebGPU surface). The DOM node still receives pointer/keys;
//   those are packed and posted to the engine worker (sprt.input_poll).
//   opts.captureInput: default true when canvas is set.

import { writeDisplay, writeInputEvent, writeProcessCompletion } from "./sprt-imports.mjs";

const NAME = {
	Begin: 1, Move: 2, End: 3, Cancel: 4, MouseMove: 5, Scroll: 6,
	KeyPressed: 7, KeyRepeated: 8, KeyReleased: 9, KeyCanceled: 10,
	WindowState: 11,
};
const BTN = { None: 0, Left: 1, Middle: 2, Right: 3, ScrollUp: 4, ScrollDown: 5, ScrollLeft: 6, ScrollRight: 7 };
// WindowState bits from sprt/runtime/window/window_info.h
const WSTATE = { Focused: 1 << 12, Pointer: 1 << 14, Enabled: 1 << 17 };

function modsFrom(e) {
	let m = 0;
	if (e.shiftKey) { m |= 1 << 0; }
	if (e.ctrlKey) { m |= 1 << 2; }
	if (e.altKey) { m |= 1 << 3; }
	if (e.metaKey) { m |= 1 << 6; }
	return m;
}

function pointerButton(e) {
	if (e.button === 1) { return BTN.Middle; }
	if (e.button === 2) { return BTN.Right; }
	return BTN.Left;
}

function keycodeFrom(e) {
	const c = e.code || "";
	if (c.startsWith("Key") && c.length === 4) { return c.charCodeAt(3); }
	if (c.startsWith("Digit") && c.length === 6) { return 48 + (c.charCodeAt(5) - 48); }
	const map = {
		Enter: 10, NumpadEnter: 6, Space: 32, Backspace: 8, Tab: 9, Escape: 27,
		ArrowRight: 11, ArrowLeft: 12, ArrowDown: 13, ArrowUp: 14,
		PageUp: 15, PageDown: 16, Home: 17, End: 18,
		ShiftLeft: 19, ControlLeft: 20, AltLeft: 21, MetaLeft: 22,
		ShiftRight: 23, ControlRight: 24, AltRight: 25, MetaRight: 26,
		Insert: 28, CapsLock: 29, ScrollLock: 30, NumLock: 31, Delete: 127,
		Minus: 45, Equal: 61, BracketLeft: 91, BracketRight: 93, Backslash: 92,
		Semicolon: 59, Quote: 43, Comma: 44, Period: 46, Slash: 47, Backquote: 96,
		NumpadDecimal: 1, NumpadDivide: 2, NumpadMultiply: 3, NumpadSubtract: 4, NumpadAdd: 5,
		Numpad0: 33, Numpad1: 34, Numpad2: 35, Numpad3: 36, Numpad4: 37,
		Numpad5: 38, Numpad6: 39, Numpad7: 40, Numpad8: 41, Numpad9: 42,
	};
	if (c.startsWith("F") && c.length <= 3) {
		const n = parseInt(c.slice(1), 10);
		if (n >= 1 && n <= 25) { return 96 + n; }
	}
	return map[c] || 0;
}

function attachCanvasInput(canvas, inputSab, density, getSize) {
	const dpr0 = density || 1;
	canvas.tabIndex = 0;
	canvas.style.outline = "none";
	canvas.style.touchAction = "none";

	let keysOn = false;
	let focused = true;
	let pointerIn = true;
	let lastPtr = { x: 0, y: 0, density: dpr0 };

	const loc = (e) => {
		const { w, h, dpr } = getSize();
		const r = canvas.getBoundingClientRect();
		const sx = r.width > 0 ? w / r.width : dpr;
		const sy = r.height > 0 ? h / r.height : dpr;
		const x = (e.clientX - r.left) * sx;
		const yRaw = (e.clientY - r.top) * sy;
		return { x, y: h - yRaw - 1, density: dpr };
	};

	const send = (ev) => writeInputEvent(inputSab, ev);
	const pushState = () => {
		let s = WSTATE.Enabled;
		if (focused) { s |= WSTATE.Focused; }
		if (pointerIn) { s |= WSTATE.Pointer; }
		send({ id: 0, name: NAME.WindowState, keycode: s >>> 0 });
	};
	pushState();

	const onDown = (e) => {
		keysOn = true;
		focused = true;
		pointerIn = true;
		canvas.focus();
		pushState();
		e.preventDefault();
		const p = loc(e);
		lastPtr = p;
		send({
			id: e.pointerId >>> 0, name: NAME.Begin, button: pointerButton(e),
			modifiers: modsFrom(e), x: p.x, y: p.y, density: p.density,
		});
		try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
	};
	const onMove = (e) => {
		const p = loc(e);
		lastPtr = p;
		const name = (e.buttons & 1) ? NAME.Move : NAME.MouseMove;
		send({
			id: e.pointerId >>> 0, name, button: name === NAME.Move ? BTN.Left : BTN.None,
			modifiers: modsFrom(e), x: p.x, y: p.y, density: p.density,
		});
	};
	const onUp = (e) => {
		const p = loc(e);
		lastPtr = p;
		send({
			id: e.pointerId >>> 0, name: NAME.End, button: pointerButton(e),
			modifiers: modsFrom(e), x: p.x, y: p.y, density: p.density,
		});
		try { canvas.releasePointerCapture(e.pointerId); } catch (_) {}
	};
	const onWheel = (e) => {
		e.preventDefault();
		const p = loc(e);
		lastPtr = p;
		let button = BTN.None;
		const valueX = -e.deltaX;
		const valueY = -e.deltaY;
		if (Math.abs(valueY) >= Math.abs(valueX)) {
			button = valueY > 0 ? BTN.ScrollUp : BTN.ScrollDown;
		} else {
			button = valueX > 0 ? BTN.ScrollRight : BTN.ScrollLeft;
		}
		send({
			id: 0xffffffff, name: NAME.Scroll, button,
			modifiers: modsFrom(e), x: p.x, y: p.y, valueX, valueY, density: p.density,
		});
	};
	const onKey = (e, name) => {
		if (!keysOn && document.activeElement !== canvas) {
			return;
		}
		if (e.repeat && name === NAME.KeyPressed) {
			name = NAME.KeyRepeated;
		}
		const keychar = e.key && e.key.length === 1 ? e.key.codePointAt(0) : 0;
		if (e.key === "Tab" || e.key === "Backspace" || e.key === "Enter" || e.key.length === 1) {
			e.preventDefault();
		}
		send({
			id: 0xffffffff, name, button: BTN.None, modifiers: modsFrom(e),
			x: lastPtr.x, y: lastPtr.y, density: lastPtr.density || dpr0,
			keycode: keycodeFrom(e), keysym: e.keyCode || 0, keychar, compose: 0,
		});
	};
	const onKeyDown = (e) => onKey(e, NAME.KeyPressed);
	const onKeyUp = (e) => onKey(e, NAME.KeyReleased);
	const onContext = (e) => e.preventDefault();
	const onEnter = () => {
		pointerIn = true;
		pushState();
	};
	const onLeave = () => {
		pointerIn = false;
		pushState();
	};
	const onFocus = () => {
		keysOn = true;
		focused = true;
		pushState();
	};
	const onBlur = () => {
		focused = false;
		pushState();
	};
	const onDocDown = (e) => {
		if (e.target !== canvas && !canvas.contains(e.target)) {
			keysOn = false;
		}
	};

	canvas.addEventListener("pointerdown", onDown);
	canvas.addEventListener("pointermove", onMove);
	canvas.addEventListener("pointerup", onUp);
	canvas.addEventListener("pointercancel", onUp);
	canvas.addEventListener("pointerenter", onEnter);
	canvas.addEventListener("pointerleave", onLeave);
	canvas.addEventListener("wheel", onWheel, { passive: false });
	canvas.addEventListener("focus", onFocus);
	canvas.addEventListener("blur", onBlur);
	canvas.addEventListener("contextmenu", onContext);
	window.addEventListener("keydown", onKeyDown, true);
	window.addEventListener("keyup", onKeyUp, true);
	document.addEventListener("pointerdown", onDocDown, true);
	try { canvas.focus(); } catch (_) {}

	return () => {
		canvas.removeEventListener("pointerdown", onDown);
		canvas.removeEventListener("pointermove", onMove);
		canvas.removeEventListener("pointerup", onUp);
		canvas.removeEventListener("pointercancel", onUp);
		canvas.removeEventListener("pointerenter", onEnter);
		canvas.removeEventListener("pointerleave", onLeave);
		canvas.removeEventListener("wheel", onWheel);
		canvas.removeEventListener("focus", onFocus);
		canvas.removeEventListener("blur", onBlur);
		canvas.removeEventListener("contextmenu", onContext);
		window.removeEventListener("keydown", onKeyDown, true);
		window.removeEventListener("keyup", onKeyUp, true);
		document.removeEventListener("pointerdown", onDocDown, true);
	};
}

function workerUrl(file) {
	const u = new URL(file, import.meta.url);
	u.searchParams.set("v", "xlmake-proc");
	return u;
}

function completeProcess(shared, rec) {
	if (!shared || !shared.processCtrl) {
		return;
	}
	writeProcessCompletion(shared.processCtrl, shared.processOut, shared.memory, shared.wakePtr, rec);
}

export function run(wasmUrl, { onStdout, onStderr, onExit, bundle, argv0, args, canvas, density, captureInput, onProcess, onInit, onFilePut, sdkGzUrl, mkGzUrl, productBundle, memoryInitial, memoryMaximum, sdkSab, wantProcess } = {}) {
	const workers = [];
	let detachInput = null;
	let stopped = false;
	const stop = () => {
		if (stopped) {
			return;
		}
		stopped = true;
		detachInput?.();
		detachInput = null;
		for (const w of workers) {
			try { w.terminate(); } catch (_) {}
		}
		workers.length = 0;
	};

	let displaySab = null;
	const size = {
		w: canvas ? (canvas.width | 0) : 0,
		h: canvas ? (canvas.height | 0) : 0,
		dpr: density || 1,
	};
	const setDisplay = (w, h, dens) => {
		size.w = w | 0;
		size.h = h | 0;
		size.dpr = dens || size.dpr;
		writeDisplay(displaySab, size.w, size.h, Math.round(size.dpr * 1000));
	};

	const done = new Promise((resolve, reject) => {
		let shared = null;
		const dispW = size.w, dispH = size.h;
		const dispDensity = Math.round(size.dpr * 1000);
		let offscreen = canvas ? canvas.transferControlToOffscreen() : null;

		const wire = (w) => {
			workers.push(w);
			w.onmessage = (e) => handle(e.data);
			w.onerror = (ev) => (onStderr || onStdout)?.("[worker error] " + (ev.message || ev.filename || ev) + "\n");
		};

		const handle = (m) => {
			switch (m.type) {
			case "init-threads":
				shared = m;
				displaySab = m.displaySab || null;
				writeDisplay(displaySab, size.w, size.h, Math.round(size.dpr * 1000));
				if (m.opfsSab) {
					const o = new Worker(workerUrl("./opfs-worker.mjs"), { type: "module" });
					o.onerror = (ev) => (onStderr || onStdout)?.("[opfs worker] " + (ev.message || ev.filename || ev) + "\n");
					o.postMessage({ opfsSab: m.opfsSab, mem: m.memory });
					workers.push(o);
				}
				if (m.gpuCtrl && offscreen) {
					const g = new Worker(workerUrl("./gpu-broker.mjs"), { type: "module" });
					g.onmessage = (e) => handle(e.data);
					g.onerror = (ev) => (onStderr || onStdout)?.("[gpu broker] " + (ev.message || ev.filename || ev) + "\n");
					g.postMessage({ memory: m.memory, canvas: offscreen, ctrl: m.gpuCtrl, scratchPtr: m.scratchPtr }, [offscreen]);
					offscreen = null;
					workers.push(g);
				}
				if (canvas && captureInput !== false && m.inputSab && !detachInput) {
					detachInput = attachCanvasInput(canvas, m.inputSab, size.dpr, () => size);
				}
				onInit?.(m);
				break;
			case "gpu-ready": onStdout?.("[gpu] ready (" + m.info + ")\n"); break;
			case "process-spawn": {
				shared.wakePtr = m.wakePtr;
				const finish = (code, stdout) => completeProcess(shared, { id: m.id, code, stdout });
				if (onProcess) {
					Promise.resolve(onProcess(m.cmd, m)).then((r) => {
						// Contract: null means the handler writes the completion slot
						// itself, asynchronously (process-host does this).
						if (r == null || r === undefined) {
							return;
						}
						// {type:"done"} or an {exitCode} worker message also means the
						// handler already completed the slot.
						if (r.type === "done" || ("exitCode" in r && !("code" in r))) {
							return;
						}
						if (typeof r.code !== "number") {
							// Garbage shape would silently complete with code 0.
							finish(127, "xlmake: onProcess returned a result with no exit code\n");
							return;
						}
						completeProcess(shared, {
							id: m.id,
							code: (r && r.code) | 0,
							stdout: (r && r.stdout) || "",
							bytes: r && r.bytes,
						});
					}).catch((err) => {
						finish(127, String(err));
					});
				} else {
					finish(127, "xlmake: no process worker (clang.wasm host not wired)\n");
				}
				break;
			}
			case "file-put": onFilePut?.(m.path, m.bytes); break;
			case "spawn": {
				const w = new Worker(workerUrl("./wasm-thread.mjs"), { type: "module" });
				wire(w);
				w.postMessage({
					module: shared.module, memory: shared.memory, bundle: shared.bundle, tidBuf: shared.tidBuf,
					opfsSab: shared.opfsSab, gpuCtrl: shared.gpuCtrl, dispW: shared.dispW, dispH: shared.dispH, dispDensity: shared.dispDensity,
					inputSab: shared.inputSab, displaySab: shared.displaySab,
					tid: m.tid, threadPtr: m.threadPtr, stackTop: m.stackTop, stackSize: m.stackSize, tlsBase: m.tlsBase,
				});
				break;
			}
			case "stdout": onStdout?.(m.text); break;
			case "stderr": (onStderr || onStdout)?.(m.text); break;
			case "exit":
				onExit?.(m.code);
				stop();
				resolve(m.code);
				break;
			case "error":
				stop();
				reject(new Error(m.message));
				break;
			}
		};

		const engine = new Worker(workerUrl("./worker.mjs"), { type: "module" });
		wire(engine);
		engine.onerror = (err) => { stop(); reject(err); };
		engine.postMessage({
			wasmUrl, bundleManifest: bundle, argv0, args, hasCanvas: !!offscreen, dispW, dispH, dispDensity,
			sdkGzUrl, mkGzUrl, productBundle, memoryInitial, memoryMaximum, sdkSab,
			wantProcess: !!(onProcess || wantProcess),
		});
	});
	done.stop = stop;
	done.setDisplay = setDisplay;
	return done;
}
