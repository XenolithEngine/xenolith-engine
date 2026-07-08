// Shared builder for the `sprt` host import table, used by both the engine worker and each
// spawned thread worker so every module instance sees the same ABI over the one shared
// memory. Callers supply the sinks that differ per context (console, thread spawn, bundle).

// ---- Unicode helpers backing the sprt::unicode wasm backend -----------------------------
// The wasm runtime ships no Unicode tables; case/normalization/IDNA/collation are delegated
// here, where the standard String/Intl APIs provide full Unicode support. Punycode (RFC 3492)
// is implemented inline so IDNA does not depend on the (spoof-check-dependent) URL behaviour.

const PUNY = { base: 36, tmin: 1, tmax: 26, skew: 38, damp: 700, initialBias: 72, initialN: 128 };
const MAXINT = 0x7fffffff;

function punyAdapt(delta, numPoints, firstTime) {
	delta = firstTime ? Math.floor(delta / PUNY.damp) : delta >> 1;
	delta += Math.floor(delta / numPoints);
	let k = 0;
	for (; delta > ((PUNY.base - PUNY.tmin) * PUNY.tmax) >> 1; k += PUNY.base) {
		delta = Math.floor(delta / (PUNY.base - PUNY.tmin));
	}
	return k + Math.floor(((PUNY.base - PUNY.tmin + 1) * delta) / (delta + PUNY.skew));
}
// 0..25 -> 'a'..'z', 26..35 -> '0'..'9'
function punyDigitToBasic(d) { return d + 22 + 75 * (d < 26 ? 1 : 0); }
function punyBasicToDigit(cp) {
	if (cp - 48 < 10) return cp - 22;
	if (cp - 65 < 26) return cp - 65;
	if (cp - 97 < 26) return cp - 97;
	return PUNY.base;
}
function punyEncode(input) {
	const cps = Array.from(input, (c) => c.codePointAt(0));
	const output = [];
	let n = PUNY.initialN, delta = 0, bias = PUNY.initialBias;
	for (const cp of cps) if (cp < 0x80) output.push(String.fromCharCode(cp));
	let basicLength = output.length, handled = basicLength;
	if (basicLength) output.push("-");
	while (handled < cps.length) {
		let m = MAXINT;
		for (const cp of cps) if (cp >= n && cp < m) m = cp;
		if (m - n > Math.floor((MAXINT - delta) / (handled + 1))) throw new Error("overflow");
		delta += (m - n) * (handled + 1);
		n = m;
		for (const cp of cps) {
			if (cp < n && ++delta > MAXINT) throw new Error("overflow");
			if (cp === n) {
				let q = delta;
				for (let k = PUNY.base; ; k += PUNY.base) {
					const t = k <= bias ? PUNY.tmin : k >= bias + PUNY.tmax ? PUNY.tmax : k - bias;
					if (q < t) break;
					output.push(String.fromCharCode(punyDigitToBasic(t + ((q - t) % (PUNY.base - t)))));
					q = Math.floor((q - t) / (PUNY.base - t));
				}
				output.push(String.fromCharCode(punyDigitToBasic(q)));
				bias = punyAdapt(delta, handled + 1, handled === basicLength);
				delta = 0;
				++handled;
			}
		}
		++delta;
		++n;
	}
	return output.join("");
}
function punyDecode(input) {
	const output = [];
	let n = PUNY.initialN, i = 0, bias = PUNY.initialBias;
	let basic = input.lastIndexOf("-");
	if (basic < 0) basic = 0;
	for (let j = 0; j < basic; ++j) {
		const cp = input.charCodeAt(j);
		if (cp >= 0x80) throw new Error("not-basic");
		output.push(cp);
	}
	for (let idx = basic > 0 ? basic + 1 : 0; idx < input.length;) {
		const oldi = i;
		for (let w = 1, k = PUNY.base; ; k += PUNY.base) {
			if (idx >= input.length) throw new Error("invalid");
			const digit = punyBasicToDigit(input.charCodeAt(idx++));
			if (digit >= PUNY.base) throw new Error("invalid");
			if (digit > Math.floor((MAXINT - i) / w)) throw new Error("overflow");
			i += digit * w;
			const t = k <= bias ? PUNY.tmin : k >= bias + PUNY.tmax ? PUNY.tmax : k - bias;
			if (digit < t) break;
			if (w > Math.floor(MAXINT / (PUNY.base - t))) throw new Error("overflow");
			w *= PUNY.base - t;
		}
		const out = output.length + 1;
		bias = punyAdapt(i - oldi, out, oldi === 0);
		if (Math.floor(i / out) > MAXINT - n) throw new Error("overflow");
		n += Math.floor(i / out);
		i %= out;
		output.splice(i++, 0, n);
	}
	return String.fromCodePoint(...output);
}
function idnaToAscii(name) {
	return name.split(".").map((label) => {
		if (label === "") return label;
		if (/^[\x00-\x7f]*$/.test(label)) return label.toLowerCase();
		return "xn--" + punyEncode(label.normalize("NFC"));
	}).join(".");
}
function idnaToUnicode(name) {
	return name.split(".").map((label) => {
		if (/^xn--/i.test(label)) {
			try { return punyDecode(label.slice(4)); } catch { return label; }
		}
		return label;
	}).join(".");
}
function titleCase(s) {
	let out = "", prevLetter = false;
	for (const ch of s) {
		const isLetter = /\p{L}/u.test(ch);
		out += isLetter && !prevLetter ? ch.toUpperCase() : ch;
		prevLetter = isLetter;
	}
	return out;
}
// op: 0 lower, 1 upper, 2 title, 3 IDNA ToASCII, 4 IDNA ToUnicode. Returns null on error.
function unicodeTransform(op, s) {
	try {
		switch (op) {
		case 0: return s.toLowerCase();
		case 1: return s.toUpperCase();
		case 2: return titleCase(s);
		case 3: return idnaToAscii(s);
		case 4: return idnaToUnicode(s);
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
			// Single-codepoint case mapping (op 0/1/2 = lower/upper/title).
			unicode_char(op, cp) {
				try {
					const s = String.fromCodePoint(cp >>> 0);
					const r = op === 0 ? s.toLowerCase() : op === 1 ? s.toUpperCase() : titleCase(s);
					const out = r.codePointAt(0);
					return out === undefined ? cp : out;
				} catch {
					return cp;
				}
			},
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
