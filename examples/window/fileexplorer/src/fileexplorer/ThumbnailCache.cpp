/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "fileexplorer/ThumbnailCache.h"
#include "XLDirector.h"
#include "XLResourceCache.h"
#include "SPBitmap.h"
#include "SPBitmapFormat.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"
#include "SPString.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// The raster formats the engine's decoder actually reads. Deliberately shorter than
// FilesystemModel's image list, which also names the ones that only get an icon: an svg is a
// picture, but not one bitmap::loadData can answer for.
static StringView s_thumbExt[] = {"png", "jpg", "jpeg", "webp", "gif", "tiff", "tif", "bmp"};

} // namespace

bool ThumbnailCache::init() { return true; }

void ThumbnailCache::setDirector(Director *director) { _director = director; }

String ThumbnailCache::makeKey(StringView path, int64_t mtime) {
	return toString("thumb:", MaxEdge, ":", mtime, ":", path);
}

bool ThumbnailCache::isImageName(StringView name) {
	auto ext = filepath::lastExtension(name);
	if (ext.empty()) {
		return false;
	}
	for (auto &it : s_thumbExt) {
		if (sp::platform::caseCompare_u(ext, it) == 0) {
			return true;
		}
	}
	return false;
}

// --- requests ---------------------------------------------------------------------------------

void ThumbnailCache::requestThumbnail(StringView path, int64_t mtime, TextureCallback &&cb) {
	if (!cb) {
		return;
	}

	if (!isImageName(filepath::lastComponent(path))) {
		cb(nullptr);
		return;
	}

	auto key = makeKey(path, mtime);
	if (auto entry = touch(key)) {
		cb(Rc<Texture>(entry->texture));
		return;
	}

	// A second tile on the same file joins the request that is already running or queued.
	auto it = _pending.find(key);
	if (it != _pending.end()) {
		it->second->callbacks.emplace_back(sp::move(cb));
		return;
	}

	auto request = Rc<Request>::alloc();
	request->key = key;
	request->path = path.str<Interface>();
	request->callbacks.emplace_back(sp::move(cb));

	_pending.emplace(key, request);
	enqueue(sp::move(request));
}

void ThumbnailCache::requestFolderPreview(StringView path, int64_t mtime, FolderCallback &&cb) {
	if (!cb) {
		return;
	}

	auto key = makeKey(path, mtime);
	if (auto it = _folders.find(key); it != _folders.end()) {
		cb(Vector<PreviewRef>(it->second));
		return;
	}

	// Unlike a thumbnail, a second asker is not joined: the walk is cheap next to a decode, and
	// the caller that arrives while one is running simply gets nothing this time and asks again
	// on the next rebuild, when the answer is in the map.
	if (_foldersInFlight.find(key) != _foldersInFlight.end()) {
		cb(Vector<PreviewRef>());
		return;
	}

	auto looper = sprt::dispatch::Looper::acquire();
	Rc<ThumbnailCache> self(this);
	const auto generation = _generation;
	auto dir = path.str<Interface>();

	auto st = looper->performAsync(
			[self, looper, key, dir, generation, cb = sp::move(cb)]() mutable {
		Vector<PreviewRef> found;
		scanFolder(dir, found);

		looper->performOnThread([self, key, generation, found = sp::move(found),
										cb = sp::move(cb)]() mutable {
			self->_foldersInFlight.erase(key);
			if (generation == self->_generation) {
				self->_folders.emplace(key, found);
				cb(sp::move(found));
			} else {
				cb(Vector<PreviewRef>());
			}
		}, self);
	}, this);

	if (!sprt::status::isSuccessful(st)) {
		cb(Vector<PreviewRef>());
		return;
	}

	_foldersInFlight.emplace(key);
}

void ThumbnailCache::invalidate() {
	++_generation;

	/* What has not started is dropped outright, and its askers are answered with nothing rather
	than left waiting: a callback runs exactly once whatever becomes of the request. What is
	already in flight is retired by the generation when it lands, and its slot is freed there. */
	auto dropped = sp::move(_queue);
	_queue.clear();

	for (auto &request : dropped) {
		_pending.erase(request->key);
		for (auto &cb : request->callbacks) { cb(nullptr); }
	}
}

// --- the queue --------------------------------------------------------------------------------

void ThumbnailCache::enqueue(Rc<Request> &&request) {
	_queue.emplace_back(sp::move(request));
	pump();
}

void ThumbnailCache::pump() {
	while (_inFlight < MaxInFlight && !_queue.empty()) {
		auto request = _queue.front();
		_queue.erase(_queue.begin());
		start(sp::move(request));
	}
}

void ThumbnailCache::start(Rc<Request> &&request) {
	auto looper = sprt::dispatch::Looper::acquire();
	Rc<ThumbnailCache> self(this);
	const auto generation = _generation;
	auto key = request->key;
	auto path = request->path;

	++_inFlight;

	auto st = looper->performAsync([self, looper, key, path, generation]() mutable {
		auto result = decode(key, path);

		looper->performOnThread(
				[self, generation, result = sp::move(result)]() mutable {
			self->complete(sp::move(result), generation);
		}, self);
	}, this);

	if (!sprt::status::isSuccessful(st)) {
		// No worker pool on this looper. A photograph is never decoded on the app thread to make
		// up for it: the request simply fails, and the tile keeps its icon.
		Result result;
		result.key = key;
		complete(sp::move(result), generation);
	}
}

void ThumbnailCache::complete(Result &&result, uint64_t generation) {
	// The slot is freed whatever the answer was worth, or the queue stops here.
	if (_inFlight > 0) {
		--_inFlight;
	}

	auto it = _pending.find(result.key);
	Rc<Request> request;
	if (it != _pending.end()) {
		request = it->second;
		_pending.erase(it);
	}

	if (generation != _generation) {
		pump();
		return;
	}

	Rc<Texture> texture;
	if (result.ok && _director) {
		if (auto cache = _director->getResourceCache()) {
			/* The pixels are tight RGBA8 and the extent is ours: the bitmap overloads, unlike
			addExternalImage, do not derive one from the file, and a buffer that does not match is
			uploaded with a tail of garbage rather than refused. */
			sprt_passert(result.data.size() == size_t(result.width) * size_t(result.height) * 4,
					"thumbnail pixels must be tight RGBA8");

			/* CompileWhenAdded, because a TemporaryResource that is never compiled is never
			deprecated either and would stay here for the rest of the session; RemoveOnClear,
			because a cleared entry that stays in the map is a gutted shell, and the next add
			under the same key finds it and answers with no texture at all. */
			texture = cache->addExternalBitmapImage(result.key,
					core::ImageInfo(Extent2(result.width, result.height),
							core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled),
					BytesView(result.data), TimeInterval::seconds(120),
					TemporaryResourceFlags::RemoveOnClear
							| TemporaryResourceFlags::CompileWhenAdded);
		}
	}

	Entry entry;
	entry.texture = texture;
	entry.failed = !texture;
	entry.atime = ++_clock;
	if (entry.failed) {
		++_failed;
	}
	_entries.emplace(result.key, sp::move(entry));
	evict();

	if (request) {
		for (auto &cb : request->callbacks) { cb(Rc<Texture>(texture)); }
	}

	pump();
}

auto ThumbnailCache::touch(StringView key) -> Entry * {
	auto it = _entries.find(key);
	if (it == _entries.end()) {
		return nullptr;
	}
	it->second.atime = ++_clock;
	return &it->second;
}

void ThumbnailCache::evict() {
	while (_entries.size() > MaxEntries) {
		auto oldest = _entries.begin();
		for (auto it = _entries.begin(); it != _entries.end(); ++it) {
			if (it->second.atime < oldest->second.atime) {
				oldest = it;
			}
		}
		if (oldest->second.failed && _failed > 0) {
			--_failed;
		}
		_entries.erase(oldest);
	}
}

// --- the worker -------------------------------------------------------------------------------

auto ThumbnailCache::decode(StringView key, StringView path) -> Result {
	Result result;
	result.key = key.str<Interface>();

	FileInfo info{path};
	if (!bitmap::isImage(info, true)) {
		return result;
	}

	auto raw = filesystem::readIntoMemory<Interface>(info);
	if (raw.empty()) {
		return result;
	}

	mem_std::Bitmap bitmap;
	if (!bitmap.loadData(BytesView(raw))) {
		return result;
	}

	// A null strideFn is what makes the result tight, which the upload above depends on.
	if (!bitmap.convert(bitmap::PixelFormat::RGBA8888)) {
		return result;
	}

	auto width = bitmap.width();
	auto height = bitmap.height();
	auto longest = sprt::max(width, height);
	if (longest == 0) {
		return result;
	}

	if (longest > MaxEdge) {
		auto scaled = bitmap.resample(sprt::max(width * MaxEdge / longest, 1u),
				sprt::max(height * MaxEdge / longest, 1u));
		if (!scaled) {
			return result;
		}
		bitmap = sp::move(scaled);
		width = bitmap.width();
		height = bitmap.height();
	}

	result.data = Bytes(bitmap.data().data(), bitmap.data().data() + bitmap.data().size());
	result.width = width;
	result.height = height;
	result.ok = result.data.size() == size_t(width) * size_t(height) * 4;
	return result;
}

void ThumbnailCache::scanFolder(StringView path, Vector<PreviewRef> &out) {
	Vector<String> images;
	size_t seen = 0;

	filesystem::ftw(FileInfo{path}, [&](const FileInfo &info, FileType type) -> bool {
		if (info.path == path) {
			return true; // dirFirst reports the directory itself
		}
		if (++seen > ScanLimit) {
			return false;
		}
		if (type != FileType::Dir) {
			auto name = filepath::lastComponent(info.path);
			if (isImageName(name)) {
				images.emplace_back(info.path.str<Interface>());
			}
		}
		return true;
	}, 1, true);

	// By name, so what a directory tile shows is the beginning of what its listing would show.
	sprt::sort(images.begin(), images.end(),
			[](const String &l, const String &r) { return l < r; });

	// Only the few that are kept are stat()ed, for the mtime their cache key needs.
	for (auto &it : images) {
		if (out.size() >= FolderPreviewCount) {
			break;
		}
		PreviewRef ref;
		ref.path = sp::move(it);
		filesystem::Stat stat;
		if (filesystem::stat(FileInfo{ref.path}, stat)) {
			ref.mtime = int64_t(stat.mtime.toMicros());
		}
		out.emplace_back(sp::move(ref));
	}
}

auto ThumbnailCache::getStats() const -> Stats {
	Stats stats;
	stats.queued = uint32_t(_queue.size());
	stats.inFlight = _inFlight;
	stats.failed = _failed;
	stats.folders = uint32_t(_folders.size());
	for (auto &it : _entries) {
		if (!it.second.failed) {
			++stats.cached;
		}
	}
	return stats;
}

} // namespace stappler::xenolith::examples
