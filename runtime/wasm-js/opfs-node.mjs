// The persistent (/opfs) filesystem for the headless Node runner: the same seven operations
// opfs-worker.mjs brokers to the browser's Origin Private File System, done synchronously on a
// host directory. A browser needs a broker because OPFS handles are async and worker-only; Node
// has a synchronous fs, so every module instance - the engine and each thread worker - calls
// straight into this, and two instances pointed at the same root see the same files.
//
// The semantics follow opfs-worker.mjs, not POSIX, because that is what wasm/libc_opfs.cc is
// written against: store creates missing parent directories, rename moves files only, stat
// reports a size truncated to 32 bits, and readdir writes "<name>\0<type>" records.
//
// makeNodeOpfs({ memory, root }) -> (op, a0, a1, a2, a3) => result (>= 0) or -errno

import { mkdirSync, openSync, readSync, writeSync, closeSync, ftruncateSync, statSync,
	readdirSync, rmdirSync, unlinkSync, renameSync } from "node:fs";
import { join } from "node:path";

// Ops (must match opfs-worker.mjs and wasm/libc_opfs.cc).
const OP_STAT = 1, OP_LOAD = 2, OP_STORE = 3, OP_MKDIR = 4, OP_UNLINK = 5,
	OP_RENAME = 6, OP_READDIR = 7;

const ENOENT = 2, EIO = 5, EEXIST = 17, ENOTDIR = 20, EISDIR = 21, EINVAL = 22,
	ENOSYS = 38, ENOTEMPTY = 39;

const errnoOf = (e) => {
	switch (e && e.code) {
	case "ENOENT": return -ENOENT;
	case "ENOTDIR": return -ENOTDIR;
	case "ENOTEMPTY": return -ENOTEMPTY;
	case "EEXIST": return -EEXIST;
	case "EISDIR": return -EISDIR;
	default: return -EIO;
	}
};

export function makeNodeOpfs({ memory, root }) {
	const dec = new TextDecoder();
	const enc = new TextEncoder();
	const u8 = () => new Uint8Array(memory.buffer);
	const i32 = () => new Int32Array(memory.buffer);
	// Int32 index of an address: floor division, a signed shift breaks past 2 GiB.
	const cell = (p) => Math.floor(p / 4);
	// TextDecoder rejects SharedArrayBuffer views, so copy the bytes out first.
	const readPath = (p, l) => dec.decode(u8().slice(p, p + l));

	// "a/b/c" -> { parts: ["a", "b"], name: "c" }; "" is the root (name empty). An OPFS name is
	// never "..", so such a path cannot leave the root here either.
	const splitPath = (rel) => {
		const segs = rel.split("/").filter((s) => s.length && s !== ".");
		if (segs.includes("..")) {
			return null;
		}
		return { parts: segs.slice(0, -1), name: segs[segs.length - 1] || "" };
	};
	const hostPath = (s) => join(root, ...s.parts, s.name);
	const dirPath = (s, create) => {
		const dir = join(root, ...s.parts);
		if (create) {
			mkdirSync(dir, { recursive: true });
		} else if (!statSync(dir).isDirectory()) {
			throw Object.assign(new Error("not a directory"), { code: "ENOTDIR" });
		}
		return dir;
	};

	const opStat = (pathPtr, pathLen, outPtr) => {
		const s = splitPath(readPath(pathPtr, pathLen));
		if (!s) return -EIO;
		let isDir = true, size = 0;
		if (s.name) {
			try {
				dirPath(s, false);
				const st = statSync(hostPath(s));
				isDir = st.isDirectory();
				size = isDir ? 0 : st.size;
			} catch (e) { return errnoOf(e); }
		}
		const m = i32();
		m[cell(outPtr)] = size | 0;
		m[cell(outPtr) + 1] = isDir ? 1 : 0;
		return 0;
	};

	const opLoad = (pathPtr, pathLen, bufPtr, cap) => {
		const s = splitPath(readPath(pathPtr, pathLen));
		if (!s || !s.name) return s ? -EISDIR : -EIO;
		let fd;
		try {
			dirPath(s, false);
			fd = openSync(hostPath(s), "r");
		} catch (e) { return errnoOf(e); }
		try {
			let got = 0;
			while (got < cap) {
				const n = readSync(fd, u8(), bufPtr + got, cap - got, got);
				if (n <= 0) break;
				got += n;
			}
			return got | 0;
		} catch (e) {
			return errnoOf(e);
		} finally {
			closeSync(fd);
		}
	};

	const opStore = (pathPtr, pathLen, bufPtr, size) => {
		const s = splitPath(readPath(pathPtr, pathLen));
		if (!s) return -EIO;
		if (!s.name) return -EINVAL;
		let fd;
		try {
			dirPath(s, true);
			fd = openSync(hostPath(s), "w");
		} catch (e) { return errnoOf(e); }
		try {
			ftruncateSync(fd, size);
			let put = 0;
			while (put < size) {
				put += writeSync(fd, u8(), bufPtr + put, size - put, put);
			}
			return 0;
		} catch (e) {
			return errnoOf(e);
		} finally {
			closeSync(fd);
		}
	};

	const opMkdir = (pathPtr, pathLen) => {
		const s = splitPath(readPath(pathPtr, pathLen));
		if (!s) return -EIO;
		if (!s.name) return -EEXIST; // the root always exists
		try {
			dirPath(s, false);
			mkdirSync(hostPath(s));
			return 0;
		} catch (e) { return errnoOf(e); }
	};

	const opUnlink = (pathPtr, pathLen) => {
		const s = splitPath(readPath(pathPtr, pathLen));
		if (!s) return -EIO;
		if (!s.name) return -EINVAL;
		try {
			dirPath(s, false);
			const p = hostPath(s);
			// OPFS removeEntry takes whichever kind the entry is; non-recursive.
			if (statSync(p).isDirectory()) {
				rmdirSync(p);
			} else {
				unlinkSync(p);
			}
			return 0;
		} catch (e) { return errnoOf(e); }
	};

	const opRename = (fromPtr, fromLen, toPtr, toLen) => {
		const from = splitPath(readPath(fromPtr, fromLen));
		const to = splitPath(readPath(toPtr, toLen));
		if (!from || !to || !from.name || !to.name) return -EIO;
		try {
			dirPath(from, false);
			if (statSync(hostPath(from)).isDirectory()) {
				return -EISDIR; // directory rename is not supported by the OPFS backend either
			}
			dirPath(to, true);
			renameSync(hostPath(from), hostPath(to));
			return 0;
		} catch (e) { return errnoOf(e); }
	};

	const opReaddir = (pathPtr, pathLen, outPtr, outCap) => {
		const s = splitPath(readPath(pathPtr, pathLen));
		if (!s) return -EIO;
		let ents;
		try {
			dirPath(s, false);
			ents = readdirSync(hostPath(s), { withFileTypes: true });
		} catch (e) { return errnoOf(e); }
		const buf = u8();
		let off = outPtr, count = 0;
		for (const ent of ents) {
			const nb = enc.encode(ent.name);
			if (off + nb.length + 2 > outPtr + outCap) break; // out of room
			buf.set(nb, off);
			off += nb.length;
			buf[off++] = 0;
			buf[off++] = ent.isDirectory() ? 1 : 0;
			++count;
		}
		return count;
	};

	mkdirSync(root, { recursive: true });

	return (op, a0, a1, a2, a3) => {
		switch (op) {
		case OP_STAT: return opStat(a0, a1, a2);
		case OP_LOAD: return opLoad(a0, a1, a2, a3);
		case OP_STORE: return opStore(a0, a1, a2, a3);
		case OP_MKDIR: return opMkdir(a0, a1);
		case OP_UNLINK: return opUnlink(a0, a1);
		case OP_RENAME: return opRename(a0, a1, a2, a3);
		case OP_READDIR: return opReaddir(a0, a1, a2, a3);
		default: return -ENOSYS;
		}
	};
}
