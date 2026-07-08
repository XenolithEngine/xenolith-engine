// A spawned thread. Instantiates the same compiled module against the shared memory, wires
// the stack and TLS block the creator pre-allocated, and enters __xl_thread_entry (which
// runs the portable __runthead trampoline). Threads may spawn further threads recursively.

import { makeImports } from "./sprt-imports.mjs";

self.onmessage = async (e) => {
	const { module, memory, tid, threadPtr, stackTop, stackSize, tlsBase, bundle, tidBuf, opfsSab } = e.data;
	const tidCounter = new Int32Array(tidBuf);

	// Nested spawn: like the engine worker, delegate creation to the main thread (this
	// worker is about to block in Atomics.wait too). self.postMessage reaches main, which
	// created this worker.
	const spawn = (tp, st, ss, tls) => {
		const ntid = Atomics.add(tidCounter, 0, 1);
		self.postMessage({ type: "spawn", tid: ntid, threadPtr: tp, stackTop: st, stackSize: ss, tlsBase: tls });
		return ntid;
	};

	try {
		const imports = makeImports({ memory, bundle, opfsSab, log: (s, t) => self.postMessage({ type: s, text: t }), spawn });
		const instance = await WebAssembly.instantiate(module, imports); // module is compiled → Instance
		const ex = instance.exports;
		ex.__stack_pointer.value = stackTop;   // run on this thread's own stack
		ex.__wasm_init_tls(tlsBase || 0);       // initialize this thread's TLS block
		try {
			ex.__xl_thread_entry(tid, threadPtr);
		} catch (err) {
			if (!(err && typeof err === "object" && err.__thread_exit)) {
				self.postMessage({ type: "error", message: String((err && err.stack) || err) });
			}
		}
	} catch (err) {
		self.postMessage({ type: "error", message: "thread " + tid + ": " + String((err && err.stack) || err) });
	}
	self.close();
};
