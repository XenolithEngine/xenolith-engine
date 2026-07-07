// Shared builder for the `sprt` host import table, used by both the engine worker and each
// spawned thread worker so every module instance sees the same ABI over the one shared
// memory. Callers supply the sinks that differ per context (console, thread spawn, bundle).

export function makeImports({ memory, bundle = {}, argv = ["app"], log, spawn, onExit }) {
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
			bundle_read(p, l, buf, cap) { const f = bundle[bkey(p, l)]; if (!f) return -1; const n = Math.min(cap, f.length); u8().set(f.subarray(0, n), buf); return n; },
			// Thread spawn: create a worker that runs __xl_thread_entry over the shared memory.
			// The creator pre-allocated the stack and TLS block; the worker only wires them.
			// Returns the new native tid (>= 2), or -1 if spawning is unavailable here.
			thread_spawn(threadPtr, stackTop, stackSize, tlsBase) { return spawn ? spawn(threadPtr, stackTop, stackSize, tlsBase) : -1; },
			thread_exit() { throw { __thread_exit: true }; },
			proc_exit(code) { onExit?.(code); throw { __exit: code }; },
		},
	};
}
