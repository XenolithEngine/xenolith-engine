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

#ifndef STAPPLER_GIT_SPGITPROTOCOL_H_
#define STAPPLER_GIT_SPGITPROTOCOL_H_

#include "SPGit.h"

// Pure (network-free) implementation of the Git wire "pkt-line" framing and the
// protocol-v2 messages this module needs: the service advertisement returned by
// GET /info/refs and the ls-refs command request/response. Everything operates
// on plain byte views so it can be unit-tested against captured fixtures.

namespace STAPPLER_VERSIONIZED stappler::git {

enum class PktType {
	Data, // regular payload pkt-line
	Flush, // 0000
	Delim, // 0001 (protocol v2 section delimiter)
	ResponseEnd, // 0002
};

struct PktLine {
	PktType type = PktType::Data;
	StringView data; // payload for Data lines (length prefix stripped)
};

// Sequential reader over a buffer of pkt-lines.
class SP_PUBLIC PktLineReader {
public:
	explicit PktLineReader(BytesView data);

	// Reads the next pkt-line into `out`. Returns false on end-of-input or on a
	// malformed / truncated frame.
	bool next(PktLine &out);

	bool empty() const { return _size == 0; }

private:
	const char *_ptr = nullptr;
	size_t _size = 0;
};

// Accumulates pkt-lines into a byte buffer for sending.
class SP_PUBLIC PktLineWriter {
public:
	void writeLine(StringView); // prepends the 4-hex length header
	void writeFlush(); // 0000
	void writeDelim(); // 0001

	const Bytes &data() const { return _buf; }
	Bytes takeData() { return sp::move(_buf); }

private:
	Bytes _buf;
};

// Parsed capability advertisement from GET /info/refs?service=git-upload-pack
// with `Git-Protocol: version=2`.
struct SP_PUBLIC ServiceAdvertisement {
	Status status = Status::Ok;
	uint32_t version = 0;
	ObjectFormat format = ObjectFormat::Sha1;
	Vector<String> capabilities;

	bool hasCapability(StringView name) const;
};

// Parse the v2 service advertisement body.
SP_PUBLIC ServiceAdvertisement parseServiceAdvertisement(BytesView);

// Build an ls-refs request body (pkt-lines) that asks for HEAD, all branches and
// all tags, with peeled tags and symref resolution.
SP_PUBLIC Bytes buildLsRefsRequest(ObjectFormat, bool sendObjectFormat = true);

// Parse the ls-refs response body into `out`. Returns Status::Ok on success.
SP_PUBLIC Status parseLsRefsResponse(BytesView, ObjectFormat, Vector<RefInfo> &out);

// Build a v2 `fetch` request body wanting a single `oid`. `depth > 0` requests a
// shallow fetch (`deepen <depth>`); `depth == 0` fetches full history.
SP_PUBLIC Bytes buildFetchRequest(const Oid &oid, uint32_t depth, ObjectFormat,
		bool sendObjectFormat = true);

// Parse a v2 `fetch` response: demultiplex the sideband packfile stream into
// `packOut` (channel 1) and collect shallow oids from the shallow-info section.
// Returns a non-Ok Status if the server reported an error (sideband channel 3)
// or the response is malformed.
SP_PUBLIC Status parseFetchResponse(BytesView, ObjectFormat, Bytes &packOut,
		Vector<Oid> &shallowOut);

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGITPROTOCOL_H_ */
