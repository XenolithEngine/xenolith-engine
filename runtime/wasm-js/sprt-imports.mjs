// Shared builder for the `sprt` host import table, used by both the engine worker and each
// spawned thread worker so every module instance sees the same ABI over the one shared
// memory. Callers supply the sinks that differ per context (console, thread spawn, bundle).

// ---- No Unicode is imported any more -----------------------------------------------------
// `unicode_char`, `unicode_transform` and `unicode_compare` all used to be here. Everything
// they answered now lives in the runtime, so that every target gives the same result and a
// host that knows nothing about Unicode is not a diminished one: IDNA via the UTS-46 engine
// (runtime/src/idn), case mapping - lower, upper and title, the last with UAX #29 word
// breaking - and both string orderings via the compiled-in tables (runtime/src/unicode).
//
// The one thing the runtime deliberately does not do is collate, which is why the last of the
// three went: `localeCompare` is language-dependent ordering, and pretending the runtime has
// it on one target out of seven was worse than not having it. A host that still supplies
// these imports is not wrong, it is just ignored.

// OPFS control-block indices + ops (must match opfs-worker.mjs and wasm/libc_opfs.cc).
const OPFS_LOCK = 0, OPFS_REQSEQ = 1, OPFS_RESPSEQ = 2, OPFS_OP = 3, OPFS_RESULT = 4,
	OPFS_A0 = 5, OPFS_A1 = 6, OPFS_A2 = 7, OPFS_A3 = 8;
const ENOSYS = 38;

// Host↔engine shared blocks. The engine App thread blocks in Atomics.wait, so its
// onmessage never runs — pointer/keys and live display size must live in a SAB the
// main thread writes and input_poll / display_size read.
export const INPUT_CAP = 64;
export const INPUT_EVENT_BYTES = 52;
export const INPUT_HDR_BYTES = 8;
export const INPUT_SAB_BYTES = INPUT_HDR_BYTES + INPUT_CAP * INPUT_EVENT_BYTES;
export const DISPLAY_SAB_BYTES = 16;

// xlmake spawnProcess completion SAB. origin/stage dropped these with process_spawn;
// xlmake.wasm (built against feature/wasm-host) still imports them. Layout matches
// xenolith-host-wasm/js/proc-sab.mjs: claim_idx so -j2 clang workers don't share a slot.
export const PROC_SLOTS = 4;
export const PROC_WR = 0;
export const PROC_RD = 1;
export const PROC_CLAIM = 2;
export const PROC_HDR = 3;
export const PROC_STRIDE = 8;
export const PROC_SLOT_BYTES = 40 * 1024 * 1024;
export const PROC_STDOUT_MAX = 256 * 1024;
export const PROC_PATH_MAX = 1024;
export const PROC_CTRL_BYTES = (PROC_HDR + PROC_SLOTS * PROC_STRIDE) * 4;
export const PROC_OUT_BYTES = PROC_SLOTS * PROC_SLOT_BYTES;
// How long a completion writer may spin on a full ring / slow previous writer
// before giving up and reporting a failed spawn back to the guest.
export const PROC_WAIT_MS = 10_000;

function memoryBuffer(memory) {
	if (!memory) {
		return null;
	}
	if (memory.buffer instanceof SharedArrayBuffer || memory.buffer instanceof ArrayBuffer) {
		return memory.buffer;
	}
	if (memory instanceof SharedArrayBuffer || memory instanceof ArrayBuffer) {
		return memory;
	}
	return null;
}

export function writeProcessCompletion(processCtrl, processOut, memory, wakePtr, { id, code, stdout, bytes, path }) {
	const i32 = new Int32Array(processCtrl);
	const text = stdout instanceof Uint8Array ? stdout : new TextEncoder().encode(stdout || "");
	const file = bytes instanceof Uint8Array ? bytes : new Uint8Array(0);
	const pathBytes = new TextEncoder().encode(path || "");

	// Bounded spins: a guest that stalls (trap, dead worker) must surface as a
	// failed job, not freeze the main thread forever. Returns false on timeout.
	const deadline = performance.now() + PROC_WAIT_MS;
	let claimed;
	for (;;) {
		claimed = Atomics.load(i32, PROC_CLAIM);
		const rd = Atomics.load(i32, PROC_RD);
		if (claimed - rd >= PROC_SLOTS) {
			if (performance.now() > deadline) {
				console.error("sprt-imports: completion ring full for " + PROC_WAIT_MS + "ms, dropping completion for proc " + id);
				return false;
			}
			continue;
		}
		if (Atomics.compareExchange(i32, PROC_CLAIM, claimed, claimed + 1) === claimed) {
			break;
		}
	}

	const slotIdx = claimed % PROC_SLOTS;
	const out = new Uint8Array(processOut);
	const base = slotIdx * PROC_SLOT_BYTES;
	const pathOff = base + PROC_SLOT_BYTES - PROC_PATH_MAX;
	const fileMax = PROC_SLOT_BYTES - PROC_STDOUT_MAX - PROC_PATH_MAX;
	const sl = Math.min(text.length, PROC_STDOUT_MAX);
	const fl = Math.min(file.length, fileMax);
	const pl = Math.min(pathBytes.length, PROC_PATH_MAX);
	out.set(text.subarray(0, sl), base);
	out.set(file.subarray(0, fl), base + PROC_STDOUT_MAX);
	out.set(pathBytes.subarray(0, pl), pathOff);
	const slot = PROC_HDR + slotIdx * PROC_STRIDE;
	Atomics.store(i32, slot, id);
	Atomics.store(i32, slot + 1, code);
	Atomics.store(i32, slot + 2, base);
	Atomics.store(i32, slot + 3, sl);
	Atomics.store(i32, slot + 4, base + PROC_STDOUT_MAX);
	Atomics.store(i32, slot + 5, fl);
	Atomics.store(i32, slot + 6, pathOff);
	Atomics.store(i32, slot + 7, pl);

	// Publication is in claim order, so this waits for the writer that claimed the
	// slot before ours. With a single writer (the loader completes on the main
	// thread) the condition already holds and this never spins.
	while (Atomics.load(i32, PROC_WR) !== claimed) {
		if (performance.now() > deadline) {
			// Never leave the ring wedged: rolling our claim back would strand
			// PROC_WR below it forever and every later completion would wait on a
			// sequence number that can no longer arrive. Publish through instead -
			// our own slot is filled, and an unfilled slot in between reads back as
			// id 0, which the guest has no job for and drops.
			console.error("sprt-imports: writer " + (claimed - 1) + " stalled for "
				+ PROC_WAIT_MS + "ms, publishing proc " + id + " over it");
			break;
		}
	}
	Atomics.store(i32, PROC_WR, claimed + 1);

	const buf = memoryBuffer(memory);
	if (buf && wakePtr) {
		const mem = new Int32Array(buf);
		Atomics.add(mem, wakePtr >> 2, 1);
		Atomics.notify(mem, wakePtr >> 2);
	}
	return true;
}

function makeDirIndex(bundle) {
	const dirs = new Map();
	const add = (parent, name, isDir) => {
		let m = dirs.get(parent);
		if (!m) {
			m = new Map();
			dirs.set(parent, m);
		}
		const prev = m.get(name);
		if (prev === undefined || (isDir && !prev)) {
			m.set(name, isDir);
		}
	};
	for (const raw of Object.keys(bundle || {})) {
		let p = String(raw).replace(/\\/g, "/");
		if (!p.startsWith("/")) {
			p = "/" + p;
		}
		if (p.length > 1 && p.endsWith("/")) {
			p = p.slice(0, -1);
		}
		const parts = p.split("/").filter(Boolean);
		let acc = "";
		for (let i = 0; i < parts.length; i++) {
			add(acc || "/", parts[i], i < parts.length - 1);
			acc += "/" + parts[i];
		}
	}
	return dirs;
}

export function writeDisplay(sab, w, h, densMilli) {
	if (!sab) {
		return;
	}
	const i32 = new Int32Array(sab);
	Atomics.store(i32, 0, w | 0);
	Atomics.store(i32, 1, h | 0);
	Atomics.store(i32, 2, densMilli | 0);
}

export function writeInputEvent(sab, e) {
	if (!sab) {
		return false;
	}
	const i32 = new Int32Array(sab);
	let wr = Atomics.load(i32, 0);
	const rd = Atomics.load(i32, 1);
	const view = new DataView(sab);
	// Coalesce MouseMove: pointermove floods the ring and drops keys.
	if (e.name === 5 && wr > rd) {
		const last = INPUT_HDR_BYTES + (((wr - 1) % INPUT_CAP) * INPUT_EVENT_BYTES);
		if (view.getUint32(last + 4, true) === 5) {
			wr = wr - 1;
		}
	}
	if (wr - rd >= INPUT_CAP) {
		return false;
	}
	const o = INPUT_HDR_BYTES + ((wr % INPUT_CAP) * INPUT_EVENT_BYTES);
	view.setUint32(o + 0, (e.id || 0) >>> 0, true);
	view.setUint32(o + 4, (e.name || 0) >>> 0, true);
	view.setUint32(o + 8, (e.button || 0) >>> 0, true);
	view.setUint32(o + 12, (e.modifiers || 0) >>> 0, true);
	view.setFloat32(o + 16, e.x || 0, true);
	view.setFloat32(o + 20, e.y || 0, true);
	view.setFloat32(o + 24, e.valueX || 0, true);
	view.setFloat32(o + 28, e.valueY || 0, true);
	view.setFloat32(o + 32, e.density || 1, true);
	view.setUint32(o + 36, (e.keycode || 0) >>> 0, true);
	view.setUint32(o + 40, (e.compose || 0) >>> 0, true);
	view.setUint32(o + 44, (e.keysym || 0) >>> 0, true);
	view.setUint32(o + 48, (e.keychar || 0) >>> 0, true);
	Atomics.store(i32, 0, wr + 1);
	return true;
}

export function formatError(err) {
	if (err == null) {
		return "unknown error";
	}
	if (typeof err === "string") {
		return err;
	}
	const name = err.name || "";
	const msg = err.message != null && String(err.message) !== "" ? String(err.message) : "";
	const stack = err.stack ? String(err.stack) : "";
	const head = (name && msg) ? (name + ": " + msg) : (msg || name || String(err));
	if (stack && (stack.startsWith(head) || (msg && stack.indexOf(msg) >= 0))) {
		return stack;
	}
	return stack && stack !== head ? head + "\n" + stack : head;
}

export function makeImports({ memory, bundle = {}, argv = ["app"], log, spawn, onExit, opfsSab, dispW = 0, dispH = 0, dispDensity = 0, inputQueue = null, inputSab = null, displaySab = null, processCtrl = null, processOut = null, postProcess = null, onFilePut = null }) {
	const opfsCtrl = opfsSab ? new Int32Array(opfsSab) : null;
	const u8 = () => new Uint8Array(memory.buffer);
	const dv = () => new DataView(memory.buffer);
	const dec = new TextDecoder();
	const enc = new TextEncoder();
	const dirIndex = makeDirIndex(bundle);
	let nextProcId = 0;
	let procCur = null;
	// TextDecoder rejects views over a SharedArrayBuffer, so slice() out a plain copy.
	const readStr = (p, l) => dec.decode(u8().slice(p, p + l));
	const bkey = (p, l) => { const b = u8(); let s = ""; for (let i = 0; i < l; i++) s += String.fromCharCode(b[p + i]); return s; };
	const bundleFile = (k) => {
		if (bundle[k]) { return bundle[k]; }
		if (k.charCodeAt(0) !== 47 && bundle["/" + k]) { return bundle["/" + k]; }
		if (k.startsWith("/app/")) {
			const rest = k.slice(5);
			if (bundle[rest]) { return bundle[rest]; }
			if (bundle["/" + rest]) { return bundle["/" + rest]; }
		} else {
			const withApp = "/app/" + k.replace(/^\//, "");
			if (bundle[withApp]) { return bundle[withApp]; }
		}
		return null;
	};
	// The bundle map holds files only, so directories are implicit in the keys. Derive
	// the parent set once and let bundle_size answer -2 for it: that is how the guest
	// VFS learns a bundled directory exists (stat/opendir on it used to be ENOENT, which
	// makes any compiler drop the include path it lives on). A host that predates the
	// sentinel keeps returning -1 and keeps the old file-only behaviour.
	let bundleDirs = null;
	const bundleDirSet = () => {
		if (!bundleDirs) {
			bundleDirs = new Set(["/"]);
			for (const k of Object.keys(bundle)) {
				for (let i = k.lastIndexOf("/"); i > 0; i = k.lastIndexOf("/", i - 1)) {
					bundleDirs.add(k.slice(0, i));
				}
			}
		}
		return bundleDirs;
	};
	const timeOrigin = (typeof performance !== "undefined" && performance.timeOrigin) || 0;
	const env = [];
	const packed = (list) => {
		const n = list.reduce((a, s) => a + enc.encode(s).length + 1, 0);
		return ((list.length & 0xffff) << 16) | (n & 0xffff);
	};
	const copy = (list) => (table, buf) => {
		let p = buf;
		for (let i = 0; i < list.length; i++) {
			dv().setUint32(table + i * 4, p, true);
			const b = enc.encode(list[i]); u8().set(b, p); p += b.length; u8()[p++] = 0;
		}
		return 0;
	};

	return {
		env: { memory },
		sprt: {
			clock_now(id) { return (id === 1 ? performance.now() + timeOrigin : Date.now()) * 1e6; },
			clock_res() { return 1e3; },
			fd_write(h, buf, len) { log?.(h === 2 ? "stderr" : "stdout", readStr(buf, len)); return len; },
			fd_read() { return 0; },
			// Fill [buf, buf+len) with cryptographically-strong random bytes; returns 0 on
			// success (WASI random_get contract). crypto.getRandomValues rejects views over a
			// SharedArrayBuffer and caps at 65536 bytes/call, so fill a plain scratch and copy.
			random_get(buf, len) {
				const dstU8 = u8();
				for (let off = 0; off < len; off += 65536) {
					const n = Math.min(65536, len - off);
					const tmp = new Uint8Array(n);
					crypto.getRandomValues(tmp);
					dstU8.set(tmp, buf + off);
				}
				return 0;
			},
			args_sizes() { return packed(argv); },
			args_copy(t, b) { return copy(argv)(t, b); },
			environ_sizes() { return packed(env); },
			environ_copy(t, b) { return copy(env)(t, b); },
			bundle_size(p, l) { const f = bundleFile(bkey(p, l)); if (f) { return f.length; } const k = bkey(p, l); return bundleDirSet().has(k) || bundleDirSet().has("/app" + (k.startsWith("/") ? k : "/" + k)) ? -2 : -1; },
			// Viewport backing size (device px), packed as (width << 16 | height); 0 if unknown.
			display_size() {
				if (displaySab) {
					const i32 = new Int32Array(displaySab);
					const w = Atomics.load(i32, 0) & 0xFFFF, h = Atomics.load(i32, 1) & 0xFFFF;
					return (w << 16) | h;
				}
				return ((dispW & 0xFFFF) << 16) | (dispH & 0xFFFF);
			},
			// devicePixelRatio x1000 (so content lays out in CSS px, renders at device px); 0 if unknown.
			display_density() {
				if (displaySab) {
					return Atomics.load(new Int32Array(displaySab), 2) & 0xFFFFFF;
				}
				return dispDensity & 0xFFFFFF;
			},
			// Drain pointer/key events. Packed WasmHostInputEvent is 13×u32/f32 (52 bytes).
			input_poll(ptr, maxCount) {
				if (!maxCount) {
					return 0;
				}
				const view = dv();
				if (inputSab) {
					const i32 = new Int32Array(inputSab);
					const src = new DataView(inputSab);
					let rd = Atomics.load(i32, 1);
					const wr = Atomics.load(i32, 0);
					let n = 0;
					while (rd !== wr && n < maxCount) {
						const srcOff = INPUT_HDR_BYTES + ((rd % INPUT_CAP) * INPUT_EVENT_BYTES);
						const dstOff = ptr + n * INPUT_EVENT_BYTES;
						for (let b = 0; b < INPUT_EVENT_BYTES; b++) {
							view.setUint8(dstOff + b, src.getUint8(srcOff + b));
						}
						rd++;
						n++;
					}
					Atomics.store(i32, 1, rd);
					return n;
				}
				if (!inputQueue) {
					return 0;
				}
				const n = Math.min(maxCount, inputQueue.length);
				for (let i = 0; i < n; i++) {
					const e = inputQueue[i];
					const o = ptr + i * 52;
					view.setUint32(o + 0, e.id >>> 0, true);
					view.setUint32(o + 4, e.name >>> 0, true);
					view.setUint32(o + 8, e.button >>> 0, true);
					view.setUint32(o + 12, e.modifiers >>> 0, true);
					view.setFloat32(o + 16, e.x, true);
					view.setFloat32(o + 20, e.y, true);
					view.setFloat32(o + 24, e.valueX || 0, true);
					view.setFloat32(o + 28, e.valueY || 0, true);
					view.setFloat32(o + 32, e.density || 1, true);
					view.setUint32(o + 36, (e.keycode || 0) >>> 0, true);
					view.setUint32(o + 40, (e.compose || 0) >>> 0, true);
					view.setUint32(o + 44, (e.keysym || 0) >>> 0, true);
					view.setUint32(o + 48, (e.keychar || 0) >>> 0, true);
				}
				inputQueue.splice(0, n);
				return n;
			},
			bundle_read(p, l, buf, cap) { const f = bundleFile(bkey(p, l)); if (!f) return -1; const n = Math.min(cap, f.length); u8().set(f.subarray(0, n), buf); return n; },
			file_put(p, l, buf, n) {
				if (!onFilePut || n <= 0) {
					return;
				}
				onFilePut(readStr(p, l), u8().slice(buf, buf + n));
			},
			bundle_dir(p, l, buf, cap) {
				let path = readStr(p, l) || "/";
				if (path.length > 1 && path.endsWith("/")) {
					path = path.slice(0, -1);
				}
				const kids = dirIndex.get(path || "/");
				if (!kids || kids.size === 0) {
					return -1;
				}
				const parts = [];
				let n = 0;
				for (const [name, isDir] of kids) {
					const s = enc.encode(isDir ? name + "/" : name);
					parts.push(s);
					n += s.length + 1;
				}
				if (n <= cap) {
					const dst = u8();
					let o = buf;
					for (const s of parts) {
						dst.set(s, o);
						o += s.length;
						dst[o++] = 0;
					}
				}
				return n;
			},
			// Thread spawn: create a worker that runs __xl_thread_entry over the shared memory.
			// The creator pre-allocated the stack and TLS block; the worker only wires them.
			// Returns the new native tid (>= 2), or -1 if spawning is unavailable here.
			thread_spawn(threadPtr, stackTop, stackSize, tlsBase) { return spawn ? spawn(threadPtr, stackTop, stackSize, tlsBase) : -1; },
			// Subprocess: not posix_spawn. JS host runs clang.wasm in a Web Worker.
			// Returns a host id (>=1) or -1 if the host has no process worker.
			process_spawn(cmdPtr, cmdLen, wakePtr) {
				if (!postProcess || !processCtrl) {
					return -1;
				}
				const cmd = readStr(cmdPtr, cmdLen);
				const id = ++nextProcId;
				postProcess({ type: "process-spawn", id, cmd, wakePtr });
				return id;
			},
			process_poll(idOut, codeOut) {
				if (!processCtrl) {
					return 0;
				}
				const i32 = new Int32Array(processCtrl);
				const rd = Atomics.load(i32, PROC_RD);
				const wr = Atomics.load(i32, PROC_WR);
				if (rd === wr) {
					return 0;
				}
				const slot = PROC_HDR + (rd % PROC_SLOTS) * PROC_STRIDE;
				const id = Atomics.load(i32, slot);
				dv().setInt32(idOut, id, true);
				dv().setInt32(codeOut, Atomics.load(i32, slot + 1), true);
				const stdoutOff = Atomics.load(i32, slot + 2);
				const stdoutLen = Atomics.load(i32, slot + 3);
				const fileOff = Atomics.load(i32, slot + 4);
				const fileLen = Atomics.load(i32, slot + 5);
				const ring = processOut ? new Uint8Array(processOut) : new Uint8Array();
				procCur = {
					id,
					stdout: stdoutLen > 0 ? ring.slice(stdoutOff, stdoutOff + stdoutLen) : new Uint8Array(),
					stdoutPos: 0,
					file: fileLen > 0 ? ring.slice(fileOff, fileOff + fileLen) : new Uint8Array(),
					filePos: 0,
				};
				Atomics.store(i32, PROC_RD, rd + 1);
				return 1;
			},
			process_take_output(id, dst, cap) {
				if (!procCur || procCur.id !== id) {
					return 0;
				}
				const n = Math.min(cap, procCur.stdout.length - procCur.stdoutPos);
				if (n <= 0) {
					return 0;
				}
				u8().set(procCur.stdout.subarray(procCur.stdoutPos, procCur.stdoutPos + n), dst);
				procCur.stdoutPos += n;
				return n;
			},
			process_take_file(id, dst, cap) {
				if (!procCur || procCur.id !== id) {
					return 0;
				}
				const n = Math.min(cap, procCur.file.length - procCur.filePos);
				if (n <= 0) {
					return 0;
				}
				u8().set(procCur.file.subarray(procCur.filePos, procCur.filePos + n), dst);
				procCur.filePos += n;
				return n;
			},
			thread_exit() { throw { __thread_exit: true }; },
			proc_exit(code) { onExit?.(code); throw { __exit: code }; },
			// Host UI locale (BCP-47) as UTF-8; returns byte length written (0 if unknown).
			// navigator.language is often language-only ("ru"); the engine's LocaleManager wants a
			// language-region tag. Intl.Locale.maximize() fills in the likely region via CLDR
			// (ru -> ru-Cyrl-RU), from which we take language + region ("ru-RU").
			os_locale(dst, cap) {
				let loc = (typeof navigator !== "undefined" && navigator.language) || "";
				try {
					const m = new Intl.Locale(loc).maximize();
					if (m.language && m.region) { loc = m.language + "-" + m.region; }
				} catch (_) { /* keep navigator.language as-is */ }
				if (!loc) return 0;
				const b = enc.encode(loc);
				const n = Math.min(cap, b.length);
				u8().set(b.subarray(0, n), dst);
				return n;
			},
			// Persistent (/opfs) filesystem op — brokered to the OPFS worker over the shared
			// control block. Args are pointers/lengths into this same shared memory. This
			// (engine or thread) worker may block in Atomics.wait; the OPFS worker cannot,
			// so it drains with Atomics.waitAsync. Returns the op result (>=0) or -errno.
			opfs_call(op, a0, a1, a2, a3) {
				if (!opfsCtrl) return -ENOSYS;
				// Serialise concurrent callers (one outstanding request at a time).
				while (Atomics.compareExchange(opfsCtrl, OPFS_LOCK, 0, 1) !== 0) { /* spin */ }
				Atomics.store(opfsCtrl, OPFS_OP, op);
				Atomics.store(opfsCtrl, OPFS_A0, a0);
				Atomics.store(opfsCtrl, OPFS_A1, a1);
				Atomics.store(opfsCtrl, OPFS_A2, a2);
				Atomics.store(opfsCtrl, OPFS_A3, a3);
				const my = Atomics.add(opfsCtrl, OPFS_REQSEQ, 1) + 1;
				Atomics.notify(opfsCtrl, OPFS_REQSEQ);
				let cur = Atomics.load(opfsCtrl, OPFS_RESPSEQ);
				while (cur < my) { Atomics.wait(opfsCtrl, OPFS_RESPSEQ, cur); cur = Atomics.load(opfsCtrl, OPFS_RESPSEQ); }
				const res = Atomics.load(opfsCtrl, OPFS_RESULT);
				Atomics.store(opfsCtrl, OPFS_LOCK, 0);
				return res;
			},
		},
	};
}
