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

#ifndef STAPPLER_GIT_SPGITPACK_H_
#define STAPPLER_GIT_SPGITPACK_H_

#include "SPGitObject.h"

// Packfile decoding: zlib inflate, object-header/delta varints, OFS/REF delta
// application, and full-pack materialization into an in-memory object store.
// Pure — no network, no filesystem.

namespace STAPPLER_VERSIONIZED stappler::git {

// Materialized git objects keyed by their lowercase hex object id.
struct SP_PUBLIC ObjectStore {
	HashMap<String, Object> objects;

	const Object *find(const Oid &) const;
	const Object *find(StringView hexKey) const;
	size_t size() const { return objects.size(); }
};

// Decode a full packfile into `out`. Handles non-delta objects plus OFS_DELTA and
// REF_DELTA (bases must be present in the same pack — no thin packs).
SP_PUBLIC Status parsePack(BytesView pack, ObjectFormat, ObjectStore &out);

// --- exposed for unit tests ---

// Inflate one zlib stream starting at `data`; append output to `out` and set
// `consumed` to the number of compressed bytes read.
SP_PUBLIC Status inflateStream(const uint8_t *data, size_t avail, Bytes &out, size_t &consumed);

// Apply a git delta (`delta`) against `base`, producing `out`.
SP_PUBLIC Status applyDelta(BytesView base, BytesView delta, Bytes &out);

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGITPACK_H_ */
