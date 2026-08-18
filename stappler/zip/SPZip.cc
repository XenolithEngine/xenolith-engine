/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

/* The public archive type, on the engine's own reader and writer.
 *
 * Stages 6 and 7 of docs/design/libzip-removal-plan.adoc: libzip is no longer called from anywhere
 * in this module. The public signatures are unchanged and stappler/document/epub/ was not touched.
 *
 * The 26 explicit specializations this file used to carry are gone: they existed because the
 * libzip glue could not be written once, and the reader and writer beneath this are templates that
 * can. What remains is one implementation plus two explicit instantiations at the bottom.
 *
 * Behaviour differences from the libzip-backed version, every one of them measured and recorded in
 * the corpus (tests/stappler/zip/corpus.cpp) before the switch:
 *
 *   * An entry whose CRC does not match its data is refused. libzip handed the bytes back without
 *     checking, and did the same for a corrupted deflate stream.
 *   * An entry of length zero reads as empty. libzip's wrapper refused it on a `stat.size == 0`
 *     early-out, which made a legitimately empty file unreadable.
 *   * An entry whose name is absolute, escapes its root, or carries a NUL is listed but cannot be
 *     read. libzip did not sanitize at all.
 *   * An archive preceded by a prefix (a self-extracting stub, an embedded archive) now opens: the
 *     four-byte signature probe that used to reject it is gone, and the catalog locates the
 *     directory from the end of the file.
 *   * Entry timestamps are read as UTC rather than as local time.
 */

#include "SPCommon.h" // IWYU pragma: keep
#include "SPZip.h"
#include "SPLog.h"

#include <stdint.h>

#ifdef MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

namespace STAPPLER_VERSIONIZED stappler {

// An archive being built serves reads out of the writer's output, which grows with every entry -
// hence re-pointing the source rather than caching it. ZipSource::setMemory exists for exactly this.
template <typename Interface>
static ZipSource &_sourceForRead(ZipBuffer<Interface> &d) {
	if (!d.readonly) {
		d.source.setMemory(d.writer.bytes());
	}
	return d.source;
}

template <typename Interface>
static const ZipEntry *_entryAt(const ZipBuffer<Interface> &d, uint64_t index) {
	return d.readonly ? d.catalog.entry(index) : d.writer.entry(index);
}

template <typename Interface>
ZipArchive<Interface>::ZipArchive(BytesView b, bool readonly) {
	_data.readonly = readonly;

	if (!readonly) {
		// Creating an archive. Opening an existing one for modification is not supported - see the
		// note on ZipWriter - so a non-empty buffer here is a request this cannot honour, and
		// saying so is better than silently discarding the caller's bytes.
		_valid = b.empty();
		return;
	}

	if (b.empty()) {
		return;
	}

	_data.data.put((const char *)b.data(), b.size());
	_data.source.setMemory(BytesView(_data.data.data(), _data.data.input()));

	_valid = sprt::status::isSuccessful(_data.catalog.read(_data.source));
}

template <typename Interface>
ZipArchive<Interface>::ZipArchive(FILE *file, bool readonly) {
	_data.readonly = readonly;

	if (!readonly || !file) {
		_valid = (!readonly && !file);
		return;
	}

	/* Read the whole file in.
	 *
	 * ZipSource can back onto a filesystem::File, but not onto a foreign FILE * - there is no
	 * public way to adopt one, and io::Producer has no traits for it. This constructor has no
	 * caller anywhere in the tree, so a few lines here beat a new adapter maintained for a path
	 * nothing takes. If that ever changes, the fix is a ProducerTraits<FILE *>, not a bigger slurp.
	 */
	if (fseek(file, 0, SEEK_END) != 0) {
		return;
	}
	auto length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
		return;
	}

	if (length > 0) {
		size_t want = size_t(length);
		auto buf = _data.data.prepare(want);
		if (!buf || want < size_t(length)) {
			return;
		}
		if (fread(buf, 1, size_t(length), file) != size_t(length)) {
			return;
		}
		_data.data.save(buf, size_t(length));
	}

	_data.source.setMemory(BytesView(_data.data.data(), _data.data.input()));
	_valid = sprt::status::isSuccessful(_data.catalog.read(_data.source));
}

#ifdef MODULE_STAPPLER_FILESYSTEM

template <typename Interface>
ZipArchive<Interface>::ZipArchive(FileInfo info) {
	_data.readonly = true;

	auto f = filesystem::openForReading(info);
	if (!f) {
		return;
	}

	_data.source.setFile(sprt::move(f));
	_valid = sprt::status::isSuccessful(_data.catalog.read(_data.source));
}

#endif

// Nothing to unwind by hand any more: the catalog, the writer and the source all own what they
// hold. The zip_discard() this used to need - and the placement-new file dance before that - are
// gone with libzip.
template <typename Interface>
ZipArchive<Interface>::~ZipArchive() = default;

template <typename Interface>
bool ZipArchive<Interface>::addDir(StringView name) {
	if (_data.readonly || !_valid) {
		return false;
	}
	return _data.writer.addDir(name);
}

template <typename Interface>
bool ZipArchive<Interface>::addFile(StringView name, BytesView data, bool uncompressed) {
	if (_data.readonly || !_valid) {
		return false;
	}
	return _data.writer.addFile(name, data, uncompressed);
}

template <typename Interface>
auto ZipArchive<Interface>::save() -> BufferTemplate<Interface> {
	if (_data.readonly || !_valid) {
		return Buffer();
	}
	return _data.writer.save();
}

// `original` used to select libzip's ZIP_FL_UNCHANGED - the archive as it was before pending
// modifications. Modification in place is not supported here, so there is no "before" to report;
// the parameter is kept for source compatibility and ignored.
template <typename Interface>
size_t ZipArchive<Interface>::size(bool original) const {
	return _data.readonly ? _data.catalog.size() : _data.writer.size();
}

template <typename Interface>
uint64_t ZipArchive<Interface>::locateFile(StringView path) const {
	return _data.readonly ? _data.catalog.locate(path) : _data.writer.locate(path);
}

template <typename Interface>
StringView ZipArchive<Interface>::getFileName(uint64_t idx, bool original) const {
	auto entry = _entryAt(_data, idx);
	return entry ? entry->name : StringView();
}

template <typename Interface>
void ZipArchive<Interface>::ftw(
		const Callback<void(uint64_t, StringView path, size_t size, Time time)> &cb,
		bool original) const {
	auto count = size(original);
	for (uint64_t i = 0; i < count; ++i) {
		auto entry = _entryAt(_data, i);
		if (entry) {
			cb(i, entry->name, size_t(entry->uncompressedSize), entry->mtime);
		}
	}
}

template <typename Interface>
bool ZipArchive<Interface>::readFile(uint64_t index, const Callback<void(BytesView)> &cb) const {
	if (!_valid) {
		return false;
	}

	auto entry = _entryAt(_data, index);
	if (!entry) {
		return false;
	}

	// const_cast: reading moves the source's position, which is an implementation detail of how the
	// bytes are fetched rather than a change to the archive the caller sees.
	auto &d = const_cast<ZipBuffer<Interface> &>(_data);
	auto &source = _sourceForRead(d);
	auto prefix = d.readonly ? d.catalog.prefix() : uint64_t(0);

	typename Interface::BytesType content;
	if (!sprt::status::isSuccessful(zipReadEntry<Interface>(source, *entry, prefix, content))) {
		return false;
	}

	// One callback with the whole buffer, including when it is empty - that is the contract this
	// method has always had, and what the EPUB reader parses XML out of.
	cb(BytesView(content.data(), content.size()));
	return true;
}

template <typename Interface>
bool ZipArchive<Interface>::readFile(StringView name,
		const Callback<void(BytesView)> &cb) const {
	auto idx = locateFile(name);
	if (idx == maxOf<uint64_t>()) {
		return false;
	}
	return readFile(idx, cb);
}

template class ZipArchive<mem_std::Interface>;
template class ZipArchive<memory::PoolInterface>;

} // namespace STAPPLER_VERSIONIZED stappler
