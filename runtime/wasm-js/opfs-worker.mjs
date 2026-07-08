// Dedicated OPFS worker: the persistent (/opfs) filesystem backend.
//
// The wasm libc is synchronous, but OPFS handle acquisition (getDirectory /
// getFileHandle / createSyncAccessHandle) is async and SyncAccessHandle is
// worker-only. So the engine/thread workers broker each op to THIS worker over a
// shared control block: they write op+args, bump a request counter and block in
// Atomics.wait; this worker drains requests with Atomics.waitAsync (which, unlike
// Atomics.wait, does NOT freeze the event loop, so the async OPFS calls can run),
// executes them, writes the result and bumps the response counter. Paths and file
// buffers are pointers into the SHARED wasm memory, read/written here directly.
//
// The protocol (indices + op codes) must match wasm/libc_opfs.cc.

// Control-block layout (Int32 indices).
const LOCK = 0, REQSEQ = 1, RESPSEQ = 2, OP = 3, RESULT = 4;
const A0 = 5, A1 = 6, A2 = 7, A3 = 8; // A4 = 9 reserved

// Ops.
const OP_STAT = 1, OP_LOAD = 2, OP_STORE = 3, OP_MKDIR = 4, OP_UNLINK = 5,
	OP_RENAME = 6, OP_READDIR = 7;

// errno (Linux/wasm values).
const ENOENT = 2, EIO = 5, EEXIST = 17, ENOTDIR = 20, EISDIR = 21, EINVAL = 22,
	ENOSYS = 38, ENOTEMPTY = 39;

let ROOT = null; // OPFS root FileSystemDirectoryHandle
let ctrl = null; // Int32Array over the control SAB
let memory = null; // the shared WebAssembly.Memory
const dec = new TextDecoder();
const enc = new TextEncoder();

const u8 = () => new Uint8Array(memory.buffer);
const i32 = () => new Int32Array(memory.buffer);
// TextDecoder rejects SharedArrayBuffer views, so copy the bytes out first.
const readPath = (p, l) => dec.decode(u8().slice(p, p + l));

// Split "a/b/c" into { parts:["a","b"], name:"c" }; "" -> root (name empty).
function splitPath(rel) {
	const segs = rel.split("/").filter((s) => s.length && s !== ".");
	return { parts: segs.slice(0, -1), name: segs[segs.length - 1] || "" };
}

async function getDir(parts, create) {
	let d = ROOT;
	for (const part of parts) {
		d = await d.getDirectoryHandle(part, { create });
	}
	return d;
}

// Map a DOM exception to a -errno.
function mapErr(e) {
	if (e && e.name === "NotFoundError") return -ENOENT;
	if (e && e.name === "TypeMismatchError") return -ENOTDIR;
	if (e && e.name === "InvalidModificationError") return -ENOTEMPTY;
	return -EIO;
}

async function opStat(pathPtr, pathLen, outPtr) {
	const rel = readPath(pathPtr, pathLen);
	if (rel === "") { // the /opfs root itself
		i32()[outPtr >> 2] = 0;
		i32()[(outPtr >> 2) + 1] = 1;
		return 0;
	}
	const { parts, name } = splitPath(rel);
	let dir;
	try { dir = await getDir(parts, false); } catch (e) { return mapErr(e); }
	// Try file first, then directory.
	try {
		const fh = await dir.getFileHandle(name, { create: false });
		const f = await fh.getFile();
		i32()[outPtr >> 2] = f.size | 0;
		i32()[(outPtr >> 2) + 1] = 0;
		return 0;
	} catch (_) { /* not a file */ }
	try {
		await dir.getDirectoryHandle(name, { create: false });
		i32()[outPtr >> 2] = 0;
		i32()[(outPtr >> 2) + 1] = 1;
		return 0;
	} catch (e) { return mapErr(e); }
}

async function opLoad(pathPtr, pathLen, bufPtr, cap) {
	const rel = readPath(pathPtr, pathLen);
	const { parts, name } = splitPath(rel);
	let handle;
	try {
		const dir = await getDir(parts, false);
		const fh = await dir.getFileHandle(name, { create: false });
		handle = await fh.createSyncAccessHandle();
	} catch (e) { return mapErr(e); }
	try {
		const size = handle.getSize();
		const n = Math.min(cap, size);
		// Read into a private buffer, then blit into shared wasm memory.
		const tmp = new Uint8Array(n);
		const got = handle.read(tmp, { at: 0 });
		u8().set(tmp.subarray(0, got), bufPtr);
		return got | 0;
	} catch (_) {
		return -EIO;
	} finally {
		handle.close();
	}
}

async function opStore(pathPtr, pathLen, bufPtr, size) {
	const rel = readPath(pathPtr, pathLen);
	const { parts, name } = splitPath(rel);
	if (!name) return -EINVAL;
	let handle;
	try {
		const dir = await getDir(parts, true);
		const fh = await dir.getFileHandle(name, { create: true });
		handle = await fh.createSyncAccessHandle();
	} catch (e) { return mapErr(e); }
	try {
		handle.truncate(size);
		if (size > 0) {
			const tmp = new Uint8Array(size);
			tmp.set(u8().subarray(bufPtr, bufPtr + size));
			handle.write(tmp, { at: 0 });
		}
		handle.flush();
		return 0;
	} catch (_) {
		return -EIO;
	} finally {
		handle.close();
	}
}

async function opMkdir(pathPtr, pathLen) {
	const rel = readPath(pathPtr, pathLen);
	const { parts, name } = splitPath(rel);
	if (!name) return -EEXIST; // the root always exists
	let dir;
	try { dir = await getDir(parts, false); } catch (e) { return mapErr(e); }
	// EEXIST if already present.
	try { await dir.getDirectoryHandle(name, { create: false }); return -EEXIST; } catch (_) {}
	try { await dir.getFileHandle(name, { create: false }); return -EEXIST; } catch (_) {}
	try { await dir.getDirectoryHandle(name, { create: true }); return 0; } catch (e) { return mapErr(e); }
}

async function opUnlink(pathPtr, pathLen, isDir) {
	const rel = readPath(pathPtr, pathLen);
	const { parts, name } = splitPath(rel);
	if (!name) return -EINVAL;
	let dir;
	try { dir = await getDir(parts, false); } catch (e) { return mapErr(e); }
	try {
		await dir.removeEntry(name, { recursive: false });
		return 0;
	} catch (e) { return mapErr(e); }
}

async function opRename(fromPtr, fromLen, toPtr, toLen) {
	const from = readPath(fromPtr, fromLen);
	const to = readPath(toPtr, toLen);
	const s = splitPath(from), d = splitPath(to);
	try {
		const sdir = await getDir(s.parts, false);
		let fh;
		try { fh = await sdir.getFileHandle(s.name, { create: false }); }
		catch (_) { return -EISDIR; } // directory rename not supported here
		// FileSystemHandle.move (Chrome) does an atomic rename/move.
		if (typeof fh.move === "function") {
			const ddir = await getDir(d.parts, true);
			await fh.move(ddir, d.name);
			return 0;
		}
		// Fallback: copy bytes then delete the source.
		const file = await fh.getFile();
		const bytes = new Uint8Array(await file.arrayBuffer());
		const ddir = await getDir(d.parts, true);
		const dfh = await ddir.getFileHandle(d.name, { create: true });
		const w = await dfh.createSyncAccessHandle();
		w.truncate(bytes.length); w.write(bytes, { at: 0 }); w.flush(); w.close();
		await sdir.removeEntry(s.name, { recursive: false });
		return 0;
	} catch (e) { return mapErr(e); }
}

async function opReaddir(pathPtr, pathLen, outPtr, outCap) {
	const rel = readPath(pathPtr, pathLen);
	const { parts, name } = splitPath(rel);
	let dir;
	try {
		dir = name ? await (await getDir(parts, false)).getDirectoryHandle(name, { create: false })
				   : await getDir(parts, false);
	} catch (e) { return mapErr(e); }
	// Serialise entries as: <name-bytes> 0x00 <type-byte(0 file/1 dir)>, repeated.
	let off = outPtr, count = 0;
	const buf = u8();
	try {
		for await (const [ename, handle] of dir.entries()) {
			const nb = enc.encode(ename);
			if (off + nb.length + 2 > outPtr + outCap) break; // out of room
			buf.set(nb, off); off += nb.length;
			buf[off++] = 0;
			buf[off++] = handle.kind === "directory" ? 1 : 0;
			++count;
		}
	} catch (e) { return mapErr(e); }
	return count;
}

async function handleOp() {
	const op = Atomics.load(ctrl, OP);
	const a0 = Atomics.load(ctrl, A0), a1 = Atomics.load(ctrl, A1);
	const a2 = Atomics.load(ctrl, A2), a3 = Atomics.load(ctrl, A3);
	switch (op) {
	case OP_STAT: return opStat(a0, a1, a2);
	case OP_LOAD: return opLoad(a0, a1, a2, a3);
	case OP_STORE: return opStore(a0, a1, a2, a3);
	case OP_MKDIR: return opMkdir(a0, a1);
	case OP_UNLINK: return opUnlink(a0, a1, a2);
	case OP_RENAME: return opRename(a0, a1, a2, a3);
	case OP_READDIR: return opReaddir(a0, a1, a2, a3);
	default: return -ENOSYS;
	}
}

async function loop() {
	let last = 0; // process any request already pending when we start
	while (true) {
		const cur = Atomics.load(ctrl, REQSEQ);
		if (cur === last) {
			const w = Atomics.waitAsync(ctrl, REQSEQ, last);
			if (w.async) { await w.value; }
			continue;
		}
		let res;
		try { res = await handleOp(); } catch (_) { res = -EIO; }
		Atomics.store(ctrl, RESULT, res | 0);
		Atomics.store(ctrl, RESPSEQ, cur);
		last = cur;
		Atomics.notify(ctrl, RESPSEQ);
	}
}

self.onmessage = async (e) => {
	const { opfsSab, mem } = e.data;
	ctrl = new Int32Array(opfsSab);
	memory = mem;
	try {
		ROOT = await navigator.storage.getDirectory();
	} catch (_) {
		ROOT = null; // OPFS unavailable — every op will report -EIO
	}
	self.postMessage({ type: "opfs-ready" });
	loop();
};
