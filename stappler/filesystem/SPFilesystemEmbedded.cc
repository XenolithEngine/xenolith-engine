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

#include "SPFilesystemEmbedded.h"
#include "SPFilesystemMap.h" // IWYU pragma: keep — MappingType
#include "SPMemory.h" // IWYU pragma: keep — mem_std::Vector / mem_std::Bytes
#include "SPSharedModule.h"

#include <sprt/c/bits/seek.h>
#include <sprt/runtime/hash.h>
#include <sprt/runtime/thread/qonce.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem::embedded {

using sprt::callback;
using sprt::hash64;

// Registry of every Bundle linked into this binary. Generated TUs publish themselves through an
// extensible SharedModule at static-init time, so the list is complete by the time anything can
// ask the filesystem for a file; it is snapshot once, on first use.
struct BundleRegistry {
	mem_std::Vector<const Bundle *> bundles;

	static const BundleRegistry *get() {
		static sprt::qonce s_once;
		static BundleRegistry *s_registry = nullptr;
		s_once([] {
			s_registry = new (sprt::nothrow) BundleRegistry;
			if (!s_registry) {
				return;
			}
			SharedModule::enumerateSymbols(BundleModuleName, s_registry,
					[](void *userdata, const char *name, const void *symbol) {
				auto bundle = reinterpret_cast<const Bundle *>(symbol);
				if (!bundle) {
					return;
				}
				if (bundle->version != BundleVersion) {
					log::source().error("filesystem", "Embedded bundle '", name,
							"' has unsupported format version ", bundle->version, ", expected ",
							BundleVersion);
					return;
				}
				reinterpret_cast<BundleRegistry *>(userdata)->bundles.emplace_back(bundle);
			});
		});
		return s_registry;
	}
};

void enumerateBundles(const Callback<bool(const Bundle &)> &cb) {
	auto registry = BundleRegistry::get();
	if (!registry) {
		return;
	}
	for (auto &it : registry->bundles) {
		if (!cb(*it)) {
			return;
		}
	}
}

const Bundle *getBundle(StringView name) {
	auto registry = BundleRegistry::get();
	if (!registry) {
		return nullptr;
	}
	for (auto &it : registry->bundles) {
		if (it->getName() == name) {
			return it;
		}
	}
	return nullptr;
}

bool hasBundles() {
	auto registry = BundleRegistry::get();
	return registry && !registry->bundles.empty();
}

// Splits an incoming path into the bundle it names and the path within that bundle. The location
// root is empty, so `path` arrives exactly as the caller wrote it: `<bundle>[/<subpath>]`.
static const Bundle *resolveBundle(StringView path, StringView &subpath) {
	path.skipChars<StringView::Chars<'/'>>();

	auto name = path.readUntil<StringView::Chars<'/'>>();
	if (name.empty()) {
		return nullptr;
	}

	subpath = path;
	subpath.skipChars<StringView::Chars<'/'>>();
	subpath.backwardSkipChars<StringView::Chars<'/'>>();

	return getBundle(name);
}

// Binary search for an exact entry. Returns nullptr when the path names nothing in the bundle.
static const Entry *findEntry(const Bundle &bundle, StringView path) {
	size_t lo = 0;
	size_t hi = bundle.entriesCount;
	while (lo < hi) {
		auto mid = lo + (hi - lo) / 2;
		auto cmp = comparePath(bundle.entries[mid].getPath(), path);
		if (cmp < 0) {
			lo = mid + 1;
		} else if (cmp > 0) {
			hi = mid;
		} else {
			return &bundle.entries[mid];
		}
	}
	return nullptr;
}

// Index of the first entry not less than `path`. With comparePath's ordering this is also the
// first entry of a directory's subtree, when `path` is that directory.
static size_t lowerBound(const Bundle &bundle, StringView path) {
	size_t lo = 0;
	size_t hi = bundle.entriesCount;
	while (lo < hi) {
		auto mid = lo + (hi - lo) / 2;
		if (comparePath(bundle.entries[mid].getPath(), path) < 0) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

// True when `path` is `prefix` itself or lies below it. An empty prefix means the bundle root.
static bool isBelow(StringView path, StringView prefix) {
	if (prefix.empty()) {
		return true;
	}
	if (!path.starts_with(prefix)) {
		return false;
	}
	return path.size() == prefix.size() || path[prefix.size()] == '/';
}

// Half-open range of the entries making up `prefix`'s subtree, excluding the directory entry for
// `prefix` itself. comparePath keeps that range contiguous.
static void subtreeRange(const Bundle &bundle, StringView prefix, size_t &begin, size_t &end) {
	begin = prefix.empty() ? 0 : lowerBound(bundle, prefix);
	if (!prefix.empty() && begin < bundle.entriesCount
			&& bundle.entries[begin].getPath() == prefix) {
		++begin; // skip the directory entry itself
	}
	end = begin;
	while (end < bundle.entriesCount && isBelow(bundle.entries[end].getPath(), prefix)) { ++end; }
}

// Depth of a path relative to a prefix: 1 for an immediate child, 2 for a grandchild, ...
static int relativeDepth(StringView path, StringView prefix) {
	auto rest = path;
	rest += prefix.empty() ? 0 : prefix.size() + 1;

	int depth = 1;
	for (auto c : rest) {
		if (c == '/') {
			++depth;
		}
	}
	return depth;
}

struct EmbeddedFile {
	const Bundle *bundle = nullptr;
	const Entry *entry = nullptr;
	const uint8_t *data = nullptr;
	size_t size = 0;
	size_t position = 0;
	mem_std::Bytes storage; // decompressed content; empty when the entry is stored raw
};

#if MODULE_STAPPLER_DATA

// Decompression lives in stappler_data, which in a shared build depends on stappler_filesystem;
// a hard dependency would be a cycle, so the function is picked up through the SharedModule seam.
using DecompressFn = size_t (*)(const uint8_t *, size_t, uint8_t *, size_t);

static DecompressFn getDecompressFn() {
	static DecompressFn s_fn = SharedModule::acquireTypedSymbol<DecompressFn>(
			buildconfig::MODULE_STAPPLER_DATA_NAME, "decompress");
	return s_fn;
}

#endif

// Materializes an entry's content: a view straight into .rodata when stored raw, a decompressed
// copy otherwise. Returns false when the content can not be produced.
static bool loadContent(const Bundle &bundle, const Entry &entry, EmbeddedFile &file) {
	if (entry.offset + entry.size > bundle.dataSize) {
		log::source().error("filesystem", "Embedded entry '", entry.getPath(),
				"' points outside of the bundle data block");
		return false;
	}

	if (!entry.isCompressed()) {
		file.data = bundle.data + entry.offset;
		file.size = size_t(entry.size);
		return true;
	}

#if MODULE_STAPPLER_DATA
	auto fn = getDecompressFn();
	if (!fn) {
		log::
				source()
						.error("filesystem",
								"Module MODULE_STAPPLER_DATA declared, but not available in "
								"runtime; " "can not read a compressed embedded file '",
								entry.getPath(), "'");
		return false;
	}

	file.storage.resize(size_t(entry.originalSize));
	auto decoded = fn(bundle.data + entry.offset, size_t(entry.size), file.storage.data(),
			file.storage.size());
	if (decoded != file.storage.size()) {
		log::source().error("filesystem", "Fail to decompress an embedded file '", entry.getPath(),
				"'");
		return false;
	}

	file.data = file.storage.data();
	file.size = file.storage.size();
	return true;
#else
	log::source().error("filesystem", "Embedded file '", entry.getPath(),
			"' is compressed, but stappler_data is not linked in to decompress it");
	return false;
#endif
}

static void fillStat(const Bundle &bundle, const Entry &entry, struct __SPRT_STAT_NAME *out) {
	sprt::memset(out, 0, sizeof(struct __SPRT_STAT_NAME));

	out->st_ino = hash64(entry.path, entry.pathSize);
	out->st_nlink = 1;
	out->st_mode = entry.isDir() ? (__SPRT_S_IFDIR | 0555) : (__SPRT_S_IFREG | 0444);
	out->st_size = entry.isDir() ? 0 : __sprt_off_t(entry.originalSize);
	out->st_blksize = out->st_size;
	out->st_blocks = 1;

	auto sec = bundle.buildTime / 1'000'000;
	auto nsec = (bundle.buildTime % 1'000'000) * 1'000;

	out->st_atim.tv_sec = sec;
	out->st_atim.tv_nsec = nsec;
	out->st_ctim.tv_sec = sec;
	out->st_ctim.tv_nsec = nsec;
	out->st_mtim.tv_sec = sec;
	out->st_mtim.tv_nsec = nsec;
}

// Walks a directory's subtree, honoring the ftw contract of the default interface: paths are
// reported relative to the walked root (empty string == the root itself), a false from the
// callback aborts with Suspended, dirFirst selects pre- or post-order, depth == -1 is unlimited
// and depth == N reports a directory at level N without descending into it.
static Status walkSubtree(const Bundle &bundle, StringView root, StringView prefix,
		const callback<bool(StringView, FileType)> &cb, int depth, bool dirFirst) {
	auto relative = [&](StringView path) {
		path += root.empty() ? 0 : root.size() + 1;
		return path;
	};

	size_t begin = 0;
	size_t end = 0;
	subtreeRange(bundle, prefix, begin, end);

	for (size_t i = begin; i < end;) {
		auto &entry = bundle.entries[i];
		if (relativeDepth(entry.getPath(), prefix) != 1) {
			++i; // not an immediate child; it is covered by the recursion of its own parent
			continue;
		}

		if (!entry.isDir()) {
			if (!cb(relative(entry.getPath()), FileType::File)) {
				return Status::Suspended;
			}
			++i;
			continue;
		}

		auto childDepth = relativeDepth(entry.getPath(), root);
		if (depth >= 0 && childDepth >= depth) {
			// at the depth limit: report the directory, do not descend
			if (!cb(relative(entry.getPath()), FileType::Dir)) {
				return Status::Suspended;
			}
			++i;
			continue;
		}

		if (dirFirst && !cb(relative(entry.getPath()), FileType::Dir)) {
			return Status::Suspended;
		}

		auto st = walkSubtree(bundle, root, entry.getPath(), cb, depth, dirFirst);
		if (st != Status::Ok) {
			return st;
		}

		if (!dirFirst && !cb(relative(entry.getPath()), FileType::Dir)) {
			return Status::Suspended;
		}
		++i;
	}

	return Status::Ok;
}

static LocationInterface s_embeddedInterface = {
	._access = [](const LocationInfo &loc, StringView path, Access mode) -> Status {
	if (hasFlag(mode, Access::Write) || hasFlag(mode, Access::Execute) || mode == Access::None) {
		return Status::ErrorInvalidArguemnt;
	}

	StringView subpath;
	auto bundle = resolveBundle(path, subpath);
	auto entry = bundle ? findEntry(*bundle, subpath) : nullptr;

	// An empty subpath names the bundle root, which always exists when the bundle does
	if (bundle && !entry && subpath.empty()) {
		return hasFlag(mode, Access::Empty) ? Status::ErrorAlreadyPerformed : Status::Ok;
	}

	if (!entry) {
		return hasFlag(mode, Access::Empty) ? Status::Ok : Status::ErrorNotFound;
	}
	return hasFlag(mode, Access::Empty) ? Status::ErrorAlreadyPerformed : Status::Ok;
},

	._stat = [](const LocationInfo &loc, StringView path, struct __SPRT_STAT_NAME *out) -> Status {
	StringView subpath;
	auto bundle = resolveBundle(path, subpath);
	if (!bundle) {
		return Status::ErrorNotFound;
	}

	if (subpath.empty()) {
		// the bundle root: a directory that has no entry of its own
		Entry root{bundle->name, uint32_t(::strlen(bundle->name)), EntryFlags::Dir, 0, 0, 0};
		fillStat(*bundle, root, out);
		return Status::Ok;
	}

	auto entry = findEntry(*bundle, subpath);
	if (!entry) {
		return Status::ErrorNotFound;
	}

	fillStat(*bundle, *entry, out);
	return Status::Ok;
},

	._open = [](const LocationInfo &loc, StringView path, OpenFlags flags, Status *st) -> void * {
	if (hasFlag(flags,
				OpenFlags::Write | OpenFlags::Append | OpenFlags::Create | OpenFlags::Truncate
						| OpenFlags::CreateExclusive | OpenFlags::DelOnClose)) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return nullptr;
	}

	StringView subpath;
	auto bundle = resolveBundle(path, subpath);
	auto entry = bundle ? findEntry(*bundle, subpath) : nullptr;
	if (!entry || entry->isDir()) {
		if (st) {
			*st = Status::ErrorNotFound;
		}
		return nullptr;
	}

	auto file = new (sprt::nothrow) EmbeddedFile;
	if (!file) {
		if (st) {
			*st = Status::ErrorOutOfHostMemory;
		}
		return nullptr;
	}

	file->bundle = bundle;
	file->entry = entry;

	if (!loadContent(*bundle, *entry, *file)) {
		delete file;
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return nullptr;
	}

	if (st) {
		*st = Status::Ok;
	}
	return file;
},
	._read = [](void *ptr, uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	auto file = reinterpret_cast<EmbeddedFile *>(ptr);
	if (!file) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}

	auto remains = (file->position < file->size) ? file->size - file->position : 0;
	auto read = sprt::min(remains, nbytes);
	if (read) {
		sprt::memcpy(buf, file->data + file->position, read);
		file->position += read;
	}
	if (st) {
		*st = Status::Ok;
	}
	return read;
},
	._write = [](void *ptr, const uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
},
	._seek = [](void *ptr, int64_t offset, int w, Status *st) -> size_t {
	auto file = reinterpret_cast<EmbeddedFile *>(ptr);
	if (!file) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}

	int64_t base = 0;
	switch (w) {
	case __SPRT_SEEK_SET: base = 0; break;
	case __SPRT_SEEK_CUR: base = int64_t(file->position); break;
	case __SPRT_SEEK_END: base = int64_t(file->size); break;
	default:
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}

	auto target = base + offset;
	if (target < 0) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}

	// Seeking past the end is legal (and File::size() relies on SEEK_END); a read there returns 0
	file->position = size_t(target);
	if (st) {
		*st = Status::Ok;
	}
	return file->position;
},
	._tell = [](void *ptr, Status *st) -> size_t {
	auto file = reinterpret_cast<EmbeddedFile *>(ptr);
	if (!file) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}
	if (st) {
		*st = Status::Ok;
	}
	return file->position;
},
	._flush =
			[](void *ptr, Status *st) {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
},
	._close =
			[](void *ptr, Status *st) {
	auto file = reinterpret_cast<EmbeddedFile *>(ptr);
	if (!file) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return;
	}
	delete file;
	if (st) {
		*st = Status::Ok;
	}
},

	._unlink = [](const LocationInfo &loc, StringView path) -> Status {
	return Status::ErrorNotImplemented; //
},

	._remove = [](const LocationInfo &loc, StringView path) -> Status {
	return Status::ErrorNotImplemented; //
},

	._touch = [](const LocationInfo &loc, StringView path) -> Status {
	return Status::ErrorNotImplemented; //
},

	._mkdir = [](const LocationInfo &loc, StringView path, __sprt_mode_t mode) -> Status {
	return Status::ErrorNotImplemented; //
},

	._rename = [](const LocationInfo &loc1, StringView from, const LocationInfo &loc2,
					   StringView to) -> Status {
	return Status::ErrorNotImplemented; //
},

	// No cross-location fast path: doCopyFile falls back to File-to-File streaming, which works
	._copy = nullptr,

	._ftw = [](const LocationInfo &loc, StringView path,
					const callback<bool(StringView, FileType)> &cb, int depth,
					bool dirFirst) -> Status {
	StringView subpath;
	auto bundle = resolveBundle(path, subpath);
	if (!bundle) {
		return Status::ErrorNotFound;
	}

	if (!subpath.empty()) {
		auto entry = findEntry(*bundle, subpath);
		if (!entry) {
			return Status::ErrorNotFound;
		}
		if (!entry->isDir()) {
			// a plain file is reported as the walk's single element
			return cb(StringView(), FileType::File) ? Status::Ok : Status::Suspended;
		}
	}

	if (dirFirst && !cb(StringView(), FileType::Dir)) {
		return Status::Suspended;
	}

	if (depth != 0) {
		auto st = walkSubtree(*bundle, subpath, subpath, cb, depth, dirFirst);
		if (st != Status::Ok) {
			return st;
		}
	}

	if (!dirFirst && !cb(StringView(), FileType::Dir)) {
		return Status::Suspended;
	}

	return Status::Ok;
},

	._write_oneshot = [](const LocationInfo &loc, StringView path, const uint8_t *buf,
							  size_t nbytes, __sprt_mode_t mode, bool override) -> Status {
	return Status::ErrorNotImplemented; //
},

	// Raw entries are already a contiguous read-only block in the image, so a mapping is just a
	// pointer into it and unmapping is a no-op. A compressed entry has nothing to point at.
	._mmap = [](uint8_t storage[16], const LocationInfo &loc, StringView path, MappingType type,
					 __sprt_mode_t prot, size_t offset, size_t len, Status *st) -> uint8_t * {
	if (type != MappingType::Private || (prot & __SPRT_S_IWOTH) != 0) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return nullptr;
	}

	StringView subpath;
	auto bundle = resolveBundle(path, subpath);
	auto entry = bundle ? findEntry(*bundle, subpath) : nullptr;
	if (!entry || entry->isDir() || entry->isCompressed()) {
		if (st) {
			*st = entry ? Status::ErrorNotImplemented : Status::ErrorNotFound;
		}
		return nullptr;
	}

	if (offset > entry->size || entry->offset + entry->size > bundle->dataSize) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return nullptr;
	}

	if (st) {
		*st = Status::Ok;
	}
	return const_cast<uint8_t *>(bundle->data + entry->offset + offset);
},
	._munmap = [](uint8_t *region, uint8_t storage[16]) -> Status { return Status::Ok; },
	._msync = [](uint8_t *region, uint8_t storage[16]) -> Status { return Status::Ok; },

	._fdopen = [](void *ptr, const char *mode, Status *st) -> __sprt_FILE * { return nullptr; }};

void ensureRegistered() {
	// The location is added on first use, not from a static initializer: the runtime's own
	// filesystem tables may not exist yet while the app's static objects are being constructed.
	static bool s_registered = [] {
		if (!hasBundles()) {
			return false;
		}
		// An empty root keeps the incoming path intact, so its first component names the bundle
		sprt::filesystem::addLocation(LocationCategory::Embedded, StringView(),
				LookupFlags::Private, LocationFlags::Specific, &s_embeddedInterface);
		return true;
	}();
	(void)s_registered;
}

} // namespace stappler::filesystem::embedded
