// Unpack sdk.bin.gz + makefile blob into the sprt bundle xlmake.wasm reads.
// Engine sources stay in the SDK; keys are aliased onto /xenolith /stappler /runtime.

import { gunzip } from "./gunzip.mjs";
export { gunzip };

const MAGIC = new TextEncoder().encode("XLSDK1\n");

const SDK_TO_MAKE = [
	["/darwin", "/toolchains/targets/aarch64-apple-macosx+open/include_libc"],
	["/sysroot", "/toolchains/targets/aarch64-apple-macosx+open"],
	["/core", "/stappler/core"],
	["/xl", "/xenolith"],
	["/sp", "/stappler"],
	["/cxx", "/runtime/include_libc/cxx"],
	["/libcxx", "/runtime/libcxx/include"],
	["/libc", "/runtime/include_libc"],
	["/rt/include", "/runtime/include"],
	["/rt", "/runtime"],
];

function decodeUtf8(bytes, start, end) {
	const span = bytes.subarray(start, end);
	// TextDecoder rejects views over SharedArrayBuffer (Safari/Chrome). Paths are
	// short; copy only those bytes. File payloads stay as SAB views.
	if (span.buffer instanceof SharedArrayBuffer) {
		return new TextDecoder().decode(span.slice());
	}
	return new TextDecoder().decode(span);
}

export function unpackSdk(bytes) {
	const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
	for (let i = 0; i < MAGIC.length; i++) {
		if (bytes[i] !== MAGIC[i]) {
			throw new Error("bad SDK magic");
		}
	}
	let o = MAGIC.length;
	const count = view.getUint32(o, true);
	o += 4;
	const files = [];
	for (let i = 0; i < count; i++) {
		const pathLen = view.getUint16(o, true);
		o += 2;
		const guest = decodeUtf8(bytes, o, o + pathLen);
		o += pathLen;
		const size = view.getUint32(o, true);
		o += 4;
		files.push({ guest, data: bytes.subarray(o, o + size) });
		o += size;
	}
	return files;
}

export function mapSdkToMakePath(path) {
	if (!path) {
		return path;
	}
	for (const [from, to] of SDK_TO_MAKE) {
		if (path === from) {
			return to;
		}
		if (path.startsWith(from + "/")) {
			return to + path.slice(from.length);
		}
	}
	return path;
}

export function aliasSdkIntoMakeBundle(bundle, packed) {
	for (const { guest, data } of packed) {
		bundle[guest] = data;
		const makePath = mapSdkToMakePath(guest);
		if (makePath !== guest) {
			bundle[makePath] = data;
		}
		if (guest.startsWith("/glslang/")) {
			const name = guest.slice("/glslang/".length);
			bundle["/glslang/" + name] = data;
			bundle["glslang/" + name] = data;
			bundle["/stappler-build/aarch64-apple-macosx+open/release/glslang/" + name] = data;
		}
	}
	return bundle;
}

export async function fetchPacked(url) {
	const res = await fetch(url);
	if (!res.ok) {
		throw new Error("fetch " + url + " -> " + res.status);
	}
	return unpackSdk(await gunzip(new Uint8Array(await res.arrayBuffer())));
}
