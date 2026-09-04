// Headless Node.js runner for a sprt wasm module (e.g. tests/libc's libctest.wasm).
//
// Mirrors the browser worker.mjs (compile → shared memory → makeImports → _start) but for
// Node: no fetch/Web-Worker/DOM. It reuses the SAME sprt-imports.mjs host table so the ABI is
// identical to the browser harness. Threads run as node worker_threads (thread-node.mjs) over
// the one shared memory. Unlike the browser (Atomics.wait is banned on the main thread, so the
// engine must live in a worker and a broker relays spawns), Node allows Atomics.wait on the
// main thread and worker_threads are independent OS threads that run even while the parent is
// blocked in a pthread_join — so the engine runs here on main and spawns threads directly.
//
// Usage: node run-node.mjs <module.wasm> [argv0 [args...]]
// Exit code = the module's proc_exit code (or 0 on normal _start return; 70 on a wasm trap).

import { readFileSync, readdirSync, statSync } from "node:fs";
import { join, relative, sep } from "node:path";
import { Worker } from "node:worker_threads";
import { makeImports, PROC_CTRL_BYTES, PROC_OUT_BYTES, writeProcessCompletion } from "./sprt-imports.mjs";

const wasmPath = process.argv[2];
if (!wasmPath) {
	process.stderr.write("usage: node run-node.mjs <module.wasm> [argv0 [args...]]\n");
	process.exit(2);
}
const argv = process.argv.slice(3);

// Filesystem for the runner: preload the working directory into the `bundle` overlay.
//
// The wasm libc's VFS (libc_file_ops.cc) serves regular paths from an in-linear-memory
// memfs (writable, in-process scratch) and falls back to the read-only `bundle` host
// import for a missing path. That is exactly how the browser harness feeds fetched
// resources in; here we feed the real host directory the program is launched from, so a
// wasm `open("test.dat", O_RDONLY)` resolves to the actual file on disk. Reads come from
// the bundle; writes/creates land in memfs (shared linear memory, so spawned threads see
// them too). Keys are the absolute, "/"-rooted paths the wasm normalizes to (getcwd is
// "/"), so "sub/f" is reachable as "/sub/f". Read-only, capped, best-effort.
const BUNDLE_MAX_FILE = 64 * 1024 * 1024;
function loadBundle(root) {
	const out = {};
	let budget = 256 * 1024 * 1024; // total cap, guards against a huge launch dir
	const walk = (dir) => {
		let ents;
		try { ents = readdirSync(dir, { withFileTypes: true }); } catch { return; }
		for (const e of ents) {
			if (e.name.startsWith(".") || e.name === "node_modules") continue;
			const abs = join(dir, e.name);
			if (e.isDirectory()) { walk(abs); continue; }
			if (!e.isFile()) continue;
			let sz;
			try { sz = statSync(abs).size; } catch { continue; }
			if (sz > BUNDLE_MAX_FILE || sz > budget) continue;
			let data;
			try { data = readFileSync(abs); } catch { continue; }
			budget -= data.length;
			// "/"-rooted, forward-slash key matching the wasm's normalized absolute path.
			const key = "/" + relative(root, abs).split(sep).join("/");
			out[key] = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
		}
	};
	walk(root);
	return out;
}
const bundle = loadBundle(process.cwd());

const module = await WebAssembly.compile(readFileSync(wasmPath));
// Shared linear memory: same shape as the browser harness (worker.mjs). `shared: true` is
// required — the module is built with atomics/bulk-memory and imports env.memory.
const memory = new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });

// Atomic tid source shared by every thread worker (1 is reserved for this main entry thread).
const tidBuf = new SharedArrayBuffer(4);
const tidCounter = new Int32Array(tidBuf);
Atomics.store(tidCounter, 0, 2);

const threadURL = new URL("./thread-node.mjs", import.meta.url);

// thread_spawn: draw a unique tid, start a node worker that instantiates the same module over
// the shared memory and enters __xl_thread_entry. The worker runs on its own OS thread, so it
// makes progress even while this thread is parked in Atomics.wait inside pthread_join.
const spawn = (threadPtr, stackTop, stackSize, tlsBase) => {
	const tid = Atomics.add(tidCounter, 0, 1);
	const w = new Worker(threadURL, {
		workerData: { module, memory, tidBuf, tid, threadPtr, stackTop, stackSize, tlsBase },
	});
	w.on("error", (e) => process.stderr.write(`[thread ${tid} error] ${(e && e.stack) || e}\n`));
	w.unref(); // a still-running detached thread must not keep the process alive past exit
	return tid;
};

let exitCode = 0;
const processCtrl = new SharedArrayBuffer(PROC_CTRL_BYTES);
const processOut = new SharedArrayBuffer(PROC_OUT_BYTES);
let processWakePtr = 0;
const onProcess = globalThis.__xlmakeOnProcess || (async (cmd) => {
	return { code: 127, stdout: "xlmake: no process worker: " + cmd.slice(0, 80) + "\n" };
});
const postProcess = (msg) => {
	if (msg.type !== "process-spawn") {
		return;
	}
	processWakePtr = msg.wakePtr;
	const finish = (r) => {
		writeProcessCompletion(processCtrl, processOut, memory, processWakePtr, {
			id: msg.id,
			code: (r && r.code) | 0,
			stdout: (r && r.stdout) || "",
			bytes: r && r.bytes,
		});
	};
	let r;
	try {
		r = onProcess(msg.cmd);
	} catch (err) {
		finish({ code: 127, stdout: String(err) });
		return;
	}
	// Node runs the engine on this thread: Atomics.wait blocks the event loop, so a
	// Promise completion would never run. Finish synchronously when we can.
	if (r && typeof r.then === "function") {
		r.then(finish, (err) => finish({ code: 127, stdout: String(err) }));
	} else {
		finish(r);
	}
};
const imports = makeImports({
	memory,
	argv: [argv[0] || "libctest", ...argv.slice(1)],
	bundle,
	log: (stream, text) => (stream === "stderr" ? process.stderr : process.stdout).write(text),
	spawn,
	processCtrl,
	processOut,
	postProcess,
	onExit: (code) => { exitCode = code; },
});

const instance = await WebAssembly.instantiate(module, imports);
try {
	instance.exports._start();
} catch (err) {
	// proc_exit unwinds by throwing { __exit: code } — that is a normal exit, not a trap.
	if (!(err && typeof err === "object" && "__exit" in err)) {
		process.stderr.write(`[wasm trap] ${(err && err.stack) || err}\n`);
		process.exit(70);
	}
}
process.exit(exitCode);
