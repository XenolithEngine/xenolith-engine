// Shared builder for the `sprt` host import table, used by both the engine worker and each
// spawned thread worker so every module instance sees the same ABI over the one shared
// memory. Callers supply the sinks that differ per context (console, thread spawn, bundle).

// ---- Unicode helpers backing the sprt::unicode wasm backend -----------------------------
// The wasm runtime ships no case-mapping or collation tables, so those are delegated here,
// where the standard String/Intl APIs provide full Unicode support.
//
// IDNA is NOT among them: the runtime carries its own UTS-46 engine (runtime/src/idn), so
// every target gives the same answer and a host that provides no IDNA is no longer a
// runtime without IDNA. Operations 3 and 4 of unicode_transform are therefore retired -
// the numbering of the surviving ones is part of the ABI and does not shift.

function titleCase(s) {
	let out = "", prevLetter = false;
	for (const ch of s) {
		const isLetter = /\p{L}/u.test(ch);
		out += isLetter && !prevLetter ? ch.toUpperCase() : ch;
		prevLetter = isLetter;
	}
	return out;
}
// op: 0 lower, 1 upper, 2 title. (3 and 4 were IDNA, now done in the runtime.)
// Returns null on error.
function unicodeTransform(op, s) {
	try {
		switch (op) {
		case 0: return s.toLowerCase();
		case 1: return s.toUpperCase();
		case 2: return titleCase(s);
		default: return null;
		}
	} catch {
		return null;
	}
}

// OPFS control-block indices + ops (must match opfs-worker.mjs and wasm/libc_opfs.cc).
const OPFS_LOCK = 0, OPFS_REQSEQ = 1, OPFS_RESPSEQ = 2, OPFS_OP = 3, OPFS_RESULT = 4,
	OPFS_A0 = 5, OPFS_A1 = 6, OPFS_A2 = 7, OPFS_A3 = 8;
const ENOSYS = 38;

export function makeImports({ memory, bundle = {}, argv = ["app"], log, spawn, onExit, opfsSab, dispW = 0, dispH = 0, dispDensity = 0 }) {
	const opfsCtrl = opfsSab ? new Int32Array(opfsSab) : null;
	const u8 = () => new Uint8Array(memory.buffer);
	const dv = () => new DataView(memory.buffer);
	const dec = new TextDecoder();
	const enc = new TextEncoder();
	// TextDecoder rejects views over a SharedArrayBuffer, so slice() out a plain copy.
	const readStr = (p, l) => dec.decode(u8().slice(p, p + l));
	const bkey = (p, l) => { const b = u8(); let s = ""; for (let i = 0; i < l; i++) s += String.fromCharCode(b[p + i]); return s; };
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
			bundle_size(p, l) { const f = bundle[bkey(p, l)]; return f ? f.length : -1; },
			// Viewport backing size (device px), packed as (width << 16 | height); 0 if unknown.
			display_size() { return ((dispW & 0xFFFF) << 16) | (dispH & 0xFFFF); },
			// devicePixelRatio x1000 (so content lays out in CSS px, renders at device px); 0 if unknown.
			display_density() { return dispDensity & 0xFFFFFF; },
			bundle_read(p, l, buf, cap) { const f = bundle[bkey(p, l)]; if (!f) return -1; const n = Math.min(cap, f.length); u8().set(f.subarray(0, n), buf); return n; },
			// Thread spawn: create a worker that runs __xl_thread_entry over the shared memory.
			// The creator pre-allocated the stack and TLS block; the worker only wires them.
			// Returns the new native tid (>= 2), or -1 if spawning is unavailable here.
			thread_spawn(threadPtr, stackTop, stackSize, tlsBase) { return spawn ? spawn(threadPtr, stackTop, stackSize, tlsBase) : -1; },
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
			// `unicode_char` (single-codepoint case mapping) used to be here. The
			// runtime carries the Unicode case tables itself now, so it no longer
			// imports it and an embedder no longer has to provide it. The string
			// transforms below are still delegated.
			// String case/normalize/IDNA (see unicodeTransform). ICU-style preflight: if the
			// UTF-8 result exceeds cap, write nothing and return the required length; else write
			// and return its length. -1 on error.
			unicode_transform(op, src, srcLen, dst, cap) {
				let s;
				try { s = readStr(src, srcLen); } catch { return -1; }
				const r = unicodeTransform(op, s);
				if (r == null) return -1;
				const b = enc.encode(r);
				if (b.length > cap) return b.length;
				u8().set(b, dst);
				return b.length;
			},
			// Locale-aware collation of two UTF-8 strings; sign like strcmp.
			unicode_compare(caseInsensitive, a, aLen, b, bLen) {
				const sa = readStr(a, aLen), sb = readStr(b, bLen);
				const r = sa.localeCompare(sb, undefined, caseInsensitive ? { sensitivity: "accent" } : {});
				return r < 0 ? -1 : r > 0 ? 1 : 0;
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
