// A spawned sprt thread under Node.js — the worker_threads analogue of the browser's
// wasm-thread.mjs. Instantiates the same compiled module against the shared memory, wires the
// stack and TLS block the creator pre-allocated, and enters __xl_thread_entry (the portable
// __runthread trampoline). Threads may spawn further threads recursively (Node allows nested
// workers, so — unlike the browser — no main-thread broker is needed).

import { workerData, Worker } from "node:worker_threads";
import { writeSync } from "node:fs";
import { makeImports } from "./sprt-imports.mjs";

// A worker's process.stdout/stderr are Writable streams piped to the PARENT and flushed on
// the parent's event loop. When the parent is blocked synchronously in Atomics.wait (inside
// a wasm pthread_join), that loop never turns, so piped output is buffered and invisible.
// Write straight to the inherited OS fds (1/2) instead — a synchronous syscall on THIS
// thread — so thread output (and fd_write traces) appear immediately regardless.
const wout = (fd, text) => { try { writeSync(fd, text); } catch { /* ignore */ } };

const { module, memory, tidBuf, tid, threadPtr, stackTop, stackSize, tlsBase } = workerData;
const tidCounter = new Int32Array(tidBuf);
const threadURL = new URL("./thread-node.mjs", import.meta.url);

const spawn = (tp, st, ss, tls) => {
	const ntid = Atomics.add(tidCounter, 0, 1);
	const w = new Worker(threadURL, {
		workerData: { module, memory, tidBuf, tid: ntid, threadPtr: tp, stackTop: st, stackSize: ss, tlsBase: tls },
	});
	// A spawned thread's error surfaces on the PARENT's event loop, which may be blocked in
	// Atomics.wait — write straight to fd 2 so it is not lost.
	w.on("error", (e) => wout(2, `[thread ${ntid} error] ${(e && e.stack) || e}\n`));
	w.unref();
	return ntid;
};

// fd_write from this thread routes to the inherited OS fds via wout (see above), so its
// output is not swallowed while the parent is blocked in a synchronous wasm join.
const imports = makeImports({
	memory,
	log: (stream, text) => wout(stream === "stderr" ? 2 : 1, text),
	spawn,
});

const instance = await WebAssembly.instantiate(module, imports);
const ex = instance.exports;
ex.__stack_pointer.value = stackTop; // run on this thread's own pre-allocated stack
ex.__wasm_init_tls(tlsBase || 0);    // initialize this thread's TLS block
try {
	ex.__xl_thread_entry(tid, threadPtr);
} catch (err) {
	// thread_exit unwinds by throwing { __thread_exit: true } — a normal thread return.
	if (!(err && typeof err === "object" && err.__thread_exit)) {
		wout(2, `[thread ${tid}] ${(err && err.stack) || err}\n`);
	}
}
