/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef STAPPLER_GIT_SPGIT_H_
#define STAPPLER_GIT_SPGIT_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include <sprt/runtime/utils/base16.h>

namespace STAPPLER_VERSIONIZED stappler::git {

// This module speaks the Git Smart HTTP protocol (protocol v2) over the
// stappler_network HTTP client. Objects here outlive individual calls and cross
// thread boundaries (the async Remote runs on its own worker thread), so all
// owning containers use the malloc-backed StandardInterface (mem_std).

using String = mem_std::String;
using Bytes = mem_std::Bytes;

template <typename T>
using Vector = mem_std::Vector<T>;

template <typename T>
using Function = mem_std::Function<T>;

template <typename K, typename V>
using Map = mem_std::Map<K, V>;

template <typename K, typename V>
using HashMap = mem_std::HashMap<K, V>;

// Object-id hash algorithm. Git repositories are SHA-1 by default; SHA-256
// repositories advertise `object-format=sha256`.
enum class ObjectFormat {
	Sha1,
	Sha256,
};

constexpr inline size_t getOidSize(ObjectFormat f) { return f == ObjectFormat::Sha256 ? 32 : 20; }

// Fixed-size git object id (commit/tag/tree/blob hash). Holds up to 32 bytes;
// only `getOidSize(format)` bytes are significant.
struct SP_PUBLIC Oid {
	ObjectFormat format = ObjectFormat::Sha1;
	uint8_t bytes[32] = {0};

	// Parse a lowercase/uppercase hex string. A too-short input yields an empty Oid.
	static Oid fromHex(StringView, ObjectFormat = ObjectFormat::Sha1);

	size_t size() const { return getOidSize(format); }

	// True when every significant byte is zero (e.g. unset / failed parse).
	bool empty() const;

	// Emit the lowercase hex form to the callback.
	void toHex(const Callback<void(StringView)> &) const;

	String str() const;
};

// A single advertised reference: a branch (refs/heads/*), tag (refs/tags/*),
// or HEAD. `oid` is the object it points at; for annotated tags `peeled` holds
// the target commit; for symbolic refs (HEAD) `symref` names the target ref.
struct SP_PUBLIC RefInfo {
	String name;
	Oid oid;
	Oid peeled;
	String symref;

	bool isBranch() const { return StringView(name).starts_with("refs/heads/"); }
	bool isTag() const { return StringView(name).starts_with("refs/tags/"); }
	bool isHead() const { return name == "HEAD"; }
};

// Result of a ref-discovery (ls-refs) operation.
struct SP_PUBLIC RefListResult {
	Status status = Status::Ok;
	long httpCode = 0;
	ObjectFormat format = ObjectFormat::Sha1;
	Vector<RefInfo> refs;
	Vector<String> capabilities;
};

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGIT_H_ */
