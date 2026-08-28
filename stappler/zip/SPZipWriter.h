/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_ZIP_SPZIPWRITER_H_
#define STAPPLER_ZIP_SPZIPWRITER_H_

#include "SPBuffer.h"
#include "SPZipCatalog.h"

namespace STAPPLER_VERSIONIZED stappler {

/* Builds a ZIP archive, entry by entry, into a buffer.
 *
 * Only CREATES archives. Opening an existing one to append is not supported and is not a gap being
 * left for later: modifying an archive in place is where most of libzip's complexity - and most of
 * its historical CVE class - lives, and nothing in the tree does it.
 *
 * The output is buffered rather than streamed, which is what the module's save() has always
 * returned. That has a pleasant consequence for the format: a payload is compressed before its
 * local header is written, so every size is known at the moment it has to be recorded, and ZIP64
 * can be switched on PER ENTRY only where it is actually needed. A truly streaming writer would
 * have to either guess or use ZIP64 unconditionally and pay for it in compatibility with old
 * unpackers.
 */
template <typename Interface>
class SP_PUBLIC ZipWriter : public Interface::AllocBaseType {
public:
	using Bytes = typename Interface::BytesType;
	using String = typename Interface::StringType;
	using Buffer = BufferTemplate<Interface>;

	template <typename T>
	using Vector = typename Interface::template VectorType<T>;

	// Appends a directory entry: stored, empty, name ending in '/'. A trailing slash is added when
	// missing, since that is the only thing distinguishing a directory entry from an empty file.
	bool addDir(StringView name);

	// Appends a file. `uncompressed` forces Store; otherwise Deflate is used, unless it fails to
	// make the data smaller.
	bool addFile(StringView name, BytesView data, bool uncompressed);

	size_t size() const { return _records.size(); }

	// nullptr when out of range. Reading back from a half-built archive goes through these: the
	// entries carry real offsets into the buffer below, so the ordinary reader can serve them.
	const ZipEntry *entry(uint64_t index) const;

	uint64_t locate(StringView name) const;

	// The bytes written so far - what `entry()`'s offsets refer to.
	BytesView bytes() const;

	/* Writes the central directory and the end records, and hands over the archive.
	 *
	 * The writer is spent afterwards: `save()` moves the buffer out. Calling it twice yields an
	 * empty result rather than a second, header-only archive.
	 */
	Buffer save();

protected:
	/* Names live in one arena, exactly as in ZipCatalog, and for the same two reasons: a NUL after
	 * each one so that StringView::terminated() has a byte to read, and a single block so that
	 * nothing owns a name individually.
	 *
	 * Unlike the catalog, entries arrive one at a time, so the arena cannot be sized up front and
	 * will occasionally move. Each record therefore remembers where its name IS rather than only
	 * pointing at it, and the views are re-pointed after a move. Geometric growth makes that rare
	 * enough not to matter.
	 */
	struct Record {
		uint64_t nameOffset = 0;
		uint32_t nameLength = 0;
		ZipEntry entry;
	};

	Buffer _out;
	Bytes _names;
	Vector<Record> _records;

	bool _finished = false;
};

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIPWRITER_H_ */
