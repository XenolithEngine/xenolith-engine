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

// Process-completion SAB (must match worker.mjs / loader.mjs):
// Int32[0] write_idx (published), [1] read_idx, [2] claim_idx;
// then N slots of {id, code, stdoutOff, stdoutLen, fileOff, fileLen}.
// claim_idx is reserved atomically so two clang workers can complete at once
// without overwriting the same slot (that race hung xlmake -j2).
// 4×40MiB = 160MiB. 2 slots raced with -j2 and ate .o bytes before take_file
// finished (silent link error 1). iOS uses -j1 so the extra slots sit idle.
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

// Safari's Error.stack is just `handle@file:line` — it does not include .message.
// Always print name+message first or the playground terminal looks empty.
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
	const file = err.filename || err.fileName || "";
	const line = err.lineno || err.lineNumber;
	const col = err.colno || err.columnNumber;
	const loc = file ? file + ":" + (line || 0) + ":" + (col || 0) : "";
	const nested = err.error && err.error !== err ? formatError(err.error) : "";
	const head = (name && msg) ? (name + ": " + msg) : (msg || name || String(err));
	if (stack && (stack.startsWith(head) || (msg && stack.indexOf(msg) >= 0))) {
		return nested && stack.indexOf(nested) < 0 ? stack + "\n" + nested : stack;
	}
	const parts = [head];
	if (loc && head.indexOf(loc) < 0) {
		parts.push(loc);
	}
	if (stack && stack !== head) {
		parts.push(stack);
	}
	if (nested && nested !== head) {
		parts.push(nested);
	}
	return parts.join("\n");
}

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

	let claimed;
	for (;;) {
		claimed = Atomics.load(i32, PROC_CLAIM);
		const rd = Atomics.load(i32, PROC_RD);
		if (claimed - rd >= PROC_SLOTS) {
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

	// Publish in claim order so the reader never sees a hole.
	while (Atomics.load(i32, PROC_WR) !== claimed) {
		// previous writer is still filling its slot
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

export function readProcessSlot(processCtrl, processOut, index) {
	const i32 = new Int32Array(processCtrl);
	const slot = PROC_HDR + (index % PROC_SLOTS) * PROC_STRIDE;
	const out = new Uint8Array(processOut);
	const pathOff = Atomics.load(i32, slot + 6);
	const pathLen = Atomics.load(i32, slot + 7);
	const fileOff = Atomics.load(i32, slot + 4);
	const fileLen = Atomics.load(i32, slot + 5);
	const path = pathLen > 0 ? new TextDecoder().decode(out.slice(pathOff, pathOff + pathLen)) : "";
	const bytes = fileLen > 0 ? out.slice(fileOff, fileOff + fileLen) : new Uint8Array();
	return {
		id: Atomics.load(i32, slot),
		code: Atomics.load(i32, slot + 1),
		path,
		bytes,
	};
}

function bundleLookup(bundle, key) {
	if (!key) {
		return null;
	}
	if (bundle[key]) {
		return bundle[key];
	}
	if (key[0] === "/" && bundle[key.slice(1)]) {
		return bundle[key.slice(1)];
	}
	if (key[0] !== "/" && bundle["/" + key]) {
		return bundle["/" + key];
	}
	return null;
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
	for (const raw of Object.keys(bundle)) {
		let p = raw.replace(/\\/g, "/");
		if (!p.startsWith("/")) {
			p = "/" + p;
		}
		if (p.length > 1 && p.endsWith("/")) {
			p = p.slice(0, -1);
		}
		const parts = p.split("/").filter(Boolean);
		let acc = "";
		for (let i = 0; i < parts.length; i++) {
			const parent = acc || "/";
			add(parent, parts[i], i < parts.length - 1);
			acc += "/" + parts[i];
		}
	}
	return dirs;
}

export function makeImports({ memory, bundle = {}, argv = ["app"], log, spawn, onExit, opfsSab, dispW = 0, dispH = 0, dispDensity = 0, processCtrl = null, processOut = null, postProcess = null, onFilePut = null }) {
	const opfsCtrl = opfsSab ? new Int32Array(opfsSab) : null;
	const u8 = () => new Uint8Array(memory.buffer);
	const dv = () => new DataView(memory.buffer);
	const dec = new TextDecoder();
	const enc = new TextEncoder();
	const dirIndex = makeDirIndex(bundle);
	// TextDecoder rejects views over a SharedArrayBuffer, so slice() out a plain copy.
	const readStr = (p, l) => dec.decode(u8().slice(p, p + l));
	const bkey = (p, l) => { const b = u8(); let s = ""; for (let i = 0; i < l; i++) s += String.fromCharCode(b[p + i]); return s; };
	const timeOrigin = (typeof performance !== "undefined" && performance.timeOrigin) || 0;
	let nextProcId = 0;
	let procCur = null;
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
			bundle_size(p, l) { const f = bundleLookup(bundle, bkey(p, l)); return f ? f.length : -1; },
			// Viewport backing size (device px), packed as (width << 16 | height); 0 if unknown.
			display_size() { return ((dispW & 0xFFFF) << 16) | (dispH & 0xFFFF); },
			// devicePixelRatio x1000 (so content lays out in CSS px, renders at device px); 0 if unknown.
			display_density() { return dispDensity & 0xFFFFFF; },
			bundle_read(p, l, buf, cap) { const f = bundleLookup(bundle, bkey(p, l)); if (!f) return -1; const n = Math.min(cap, f.length); u8().set(f.subarray(0, n), buf); return n; },
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
				if (!postProcess || !processCtrl) return -1;
				const cmd = readStr(cmdPtr, cmdLen);
				const id = ++nextProcId;
				postProcess({ type: "process-spawn", id, cmd, wakePtr });
				return id;
			},
			process_poll(idOut, codeOut) {
				if (!processCtrl) return 0;
				const i32 = new Int32Array(processCtrl);
				const rd = Atomics.load(i32, PROC_RD);
				const wr = Atomics.load(i32, PROC_WR);
				if (rd === wr) return 0;
				const slot = PROC_HDR + (rd % PROC_SLOTS) * PROC_STRIDE;
				const id = Atomics.load(i32, slot);
				dv().setInt32(idOut, id, true);
				dv().setInt32(codeOut, Atomics.load(i32, slot + 1), true);
				const stdoutOff = Atomics.load(i32, slot + 2);
				const stdoutLen = Atomics.load(i32, slot + 3);
				const fileOff = Atomics.load(i32, slot + 4);
				const fileLen = Atomics.load(i32, slot + 5);
				// Copy out of the ring slot BEFORE freeing it. With 2 slots and -j2 the next
				// clang completion would otherwise overwrite .o bytes while take_file still reads.
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
				if (!procCur || procCur.id !== id) return 0;
				const n = Math.min(cap, procCur.stdout.length - procCur.stdoutPos);
				if (n <= 0) return 0;
				u8().set(procCur.stdout.subarray(procCur.stdoutPos, procCur.stdoutPos + n), dst);
				procCur.stdoutPos += n;
				return n;
			},
			process_take_file(id, dst, cap) {
				if (!procCur || procCur.id !== id) return 0;
				const n = Math.min(cap, procCur.file.length - procCur.filePos);
				if (n <= 0) return 0;
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
