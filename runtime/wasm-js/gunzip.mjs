// Browser-safe gzip. Safari 16.0–16.3 has no DecompressionStream; never import node:zlib
// (HTTPS pages treat `node:` as mixed content and block the module).

const LEN_BASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258];
const LEN_EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const DIST_BASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
const DIST_EXTRA = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
const CLEN_ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

function bits(s, n) {
	while (s.nbits < n) {
		if (s.i >= s.src.length) {
			throw new Error("gzip: truncated");
		}
		s.acc |= s.src[s.i++] << s.nbits;
		s.nbits += 8;
	}
	const v = s.acc & ((1 << n) - 1);
	s.acc >>>= n;
	s.nbits -= n;
	return v;
}

function buildTree(lens) {
	const max = Math.max(0, ...lens);
	const counts = new Uint16Array(max + 1);
	for (let i = 0; i < lens.length; i++) {
		counts[lens[i]]++;
	}
	counts[0] = 0;
	const next = new Uint16Array(max + 1);
	let code = 0;
	for (let len = 1; len <= max; len++) {
		code = (code + counts[len - 1]) << 1;
		next[len] = code;
	}
	const syms = new Int32Array(1 << max).fill(-1);
	for (let i = 0; i < lens.length; i++) {
		const len = lens[i];
		if (!len) {
			continue;
		}
		let c = next[len]++;
		let rev = 0;
		for (let b = 0; b < len; b++) {
			rev = (rev << 1) | (c & 1);
			c >>= 1;
		}
		const stride = 1 << len;
		for (let fill = rev; fill < syms.length; fill += stride) {
			syms[fill] = i;
		}
	}
	return { max, syms };
}

function decode(s, tree) {
	let code = 0;
	for (let len = 1; len <= tree.max; len++) {
		code |= bits(s, 1) << (len - 1);
		const mask = (1 << len) - 1;
		const idx = code & mask;
		if (idx < tree.syms.length) {
			const sym = tree.syms[idx];
			if (sym >= 0) {
				let c = 0, t = idx;
				for (let b = 0; b < len; b++) {
					c = (c << 1) | (t & 1);
					t >>= 1;
				}
				let want = 0, orig = idx;
				for (let b = 0; b < len; b++) {
					want = (want << 1) | (orig & 1);
					orig >>= 1;
				}
			}
		}
	}
	throw new Error("gzip: bad huffman");
}

function decodeFast(s, tree) {
	let acc = s.acc, nbits = s.nbits, i = s.i;
	const src = s.src;
	while (nbits < tree.max) {
		if (i >= src.length) {
			throw new Error("gzip: truncated");
		}
		acc |= src[i++] << nbits;
		nbits += 8;
	}
	const idx = acc & ((1 << tree.max) - 1);
	const packed = tree.pack[idx];
	if (packed < 0) {
		throw new Error("gzip: bad huffman");
	}
	const len = packed >>> 16;
	const sym = packed & 0xffff;
	s.acc = acc >>> len;
	s.nbits = nbits - len;
	s.i = i;
	return sym;
}

function buildPack(lens) {
	const max = Math.max(0, ...lens);
	if (max > 15) {
		throw new Error("gzip: oversubscribed");
	}
	const counts = new Uint16Array(16);
	for (let i = 0; i < lens.length; i++) {
		if (lens[i]) {
			counts[lens[i]]++;
		}
	}
	const next = new Uint16Array(16);
	let code = 0;
	for (let len = 1; len <= max; len++) {
		code = (code + counts[len - 1]) << 1;
		next[len] = code;
	}
	const pack = new Int32Array(1 << max).fill(-1);
	for (let i = 0; i < lens.length; i++) {
		const len = lens[i];
		if (!len) {
			continue;
		}
		let c = next[len]++;
		let rev = 0;
		for (let b = 0; b < len; b++) {
			rev = (rev << 1) | (c & 1);
			c >>= 1;
		}
		const stride = 1 << len;
		const val = (len << 16) | i;
		for (let fill = rev; fill < pack.length; fill += stride) {
			pack[fill] = val;
		}
	}
	return { max, pack };
}

function fixedLit() {
	const lens = new Uint8Array(288);
	for (let i = 0; i <= 143; i++) {
		lens[i] = 8;
	}
	for (let i = 144; i <= 255; i++) {
		lens[i] = 9;
	}
	for (let i = 256; i <= 279; i++) {
		lens[i] = 7;
	}
	for (let i = 280; i <= 287; i++) {
		lens[i] = 8;
	}
	return buildPack(lens);
}

function fixedDist() {
	const lens = new Uint8Array(32);
	lens.fill(5);
	return buildPack(lens);
}

const FIXED_LIT = fixedLit();
const FIXED_DIST = fixedDist();

function inflate(src, outHint) {
	const s = { src, i: 0, acc: 0, nbits: 0 };
	let out = outHint ? new Uint8Array(outHint) : new Uint8Array(1 << 16);
	let o = 0;
	const push = (n) => {
		if (o + n > out.length) {
			const nbuf = new Uint8Array(Math.max(out.length * 2, o + n));
			nbuf.set(out);
			out = nbuf;
		}
	};
	for (;;) {
		const last = bits(s, 1);
		const type = bits(s, 2);
		if (type === 0) {
			s.acc = 0;
			s.nbits = 0;
			if (s.i + 4 > src.length) {
				throw new Error("gzip: stored");
			}
			const len = src[s.i] | (src[s.i + 1] << 8);
			s.i += 4;
			push(len);
			out.set(src.subarray(s.i, s.i + len), o);
			o += len;
			s.i += len;
		} else if (type === 3) {
			throw new Error("gzip: bad block");
		} else {
			let lit, dist;
			if (type === 1) {
				lit = FIXED_LIT;
				dist = FIXED_DIST;
			} else {
				const nlit = bits(s, 5) + 257;
				const ndist = bits(s, 5) + 1;
				const nclen = bits(s, 4) + 4;
				const clens = new Uint8Array(19);
				for (let i = 0; i < nclen; i++) {
					clens[CLEN_ORDER[i]] = bits(s, 3);
				}
				const ctree = buildPack(clens);
				const lens = new Uint8Array(nlit + ndist);
				for (let i = 0; i < lens.length;) {
					const sym = decodeFast(s, ctree);
					if (sym < 16) {
						lens[i++] = sym;
					} else if (sym === 16) {
						const r = bits(s, 2) + 3;
						const prev = lens[i - 1];
						for (let k = 0; k < r; k++) {
							lens[i++] = prev;
						}
					} else {
						const r = sym === 17 ? bits(s, 3) + 3 : bits(s, 7) + 11;
						for (let k = 0; k < r; k++) {
							lens[i++] = 0;
						}
					}
				}
				lit = buildPack(lens.subarray(0, nlit));
				dist = buildPack(lens.subarray(nlit));
			}
			for (;;) {
				const sym = decodeFast(s, lit);
				if (sym < 256) {
					push(1);
					out[o++] = sym;
				} else if (sym === 256) {
					break;
				} else {
					const li = sym - 257;
					const length = LEN_BASE[li] + bits(s, LEN_EXTRA[li]);
					const di = decodeFast(s, dist);
					const distance = DIST_BASE[di] + bits(s, DIST_EXTRA[di]);
					push(length);
					let from = o - distance;
					for (let k = 0; k < length; k++) {
						out[o++] = out[from++];
					}
				}
			}
		}
		if (last) {
			break;
		}
	}
	return out.subarray(0, o);
}

function gunzipSync(src) {
	const u8 = src instanceof Uint8Array ? src : new Uint8Array(src);
	if (u8.length < 18 || u8[0] !== 0x1f || u8[1] !== 0x8b) {
		throw new Error("gzip: bad header");
	}
	if (u8[2] !== 8) {
		throw new Error("gzip: not deflate");
	}
	const flg = u8[3];
	let o = 10;
	if (flg & 4) {
		if (o + 2 > u8.length) {
			throw new Error("gzip: extra");
		}
		o += 2 + (u8[o] | (u8[o + 1] << 8));
	}
	if (flg & 8) {
		while (o < u8.length && u8[o++]) { /* name */ }
	}
	if (flg & 16) {
		while (o < u8.length && u8[o++]) { /* comment */ }
	}
	if (flg & 2) {
		o += 2;
	}
	const isize = (u8[u8.length - 4] | (u8[u8.length - 3] << 8) | (u8[u8.length - 2] << 16) | (u8[u8.length - 1] << 24)) >>> 0;
	return inflate(u8.subarray(o, u8.length - 8), isize || 0);
}

export async function gunzip(bytes) {
	if (typeof DecompressionStream === "function") {
		try {
			const ds = new DecompressionStream("gzip");
			const stream = new Blob([bytes]).stream().pipeThrough(ds);
			return new Uint8Array(await new Response(stream).arrayBuffer());
		} catch {
			/* Safari can expose the ctor and still fail; fall through. */
		}
	}
	return gunzipSync(bytes);
}

export { gunzipSync };
