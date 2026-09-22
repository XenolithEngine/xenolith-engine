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

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_THUMBNAILCACHE_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_THUMBNAILCACHE_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLTexture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* Thumbnails for the files a tile is showing, decoded off the app thread.

ResourceCache::addExternalImage(key, info, FileInfo) is the engine's own way to put a picture on
screen, and it is the wrong tool here for two reasons: it reads the file header on the CALLING
thread, and it uploads the image at its source resolution. A directory of photographs would mean a
few hundred synchronous header reads in one frame and a texture per file measured in tens of
megabytes. So the decode is ours: read, decode, resample to MaxEdge on the worker pool, and hand
the finished pixels to addExternalBitmapImage on the app thread.

Everything below runs on the app thread except the body of the worker task.

Three rules this class exists to keep:

  * a request is made only by a tile that was actually materialized, so what is off screen is
    never queued, and MaxInFlight bounds what is being decoded at once;
  * an answer is matched against the generation it was asked in - navigating away invalidates the
    generation, and a late answer is dropped - but the bookkeeping that frees the slot runs either
    way, or the queue stops;
  * a texture is only ever registered for a tile that is about to show it: a TemporaryResource
    that is never compiled is also never deprecated, and would stay in the cache for good. */
class ThumbnailCache : public Ref {
public:
	// The long edge of a decoded thumbnail. One size for every tile size: rescaling a 256px
	// texture down costs nothing, and re-decoding on every notch of the slider costs plenty.
	static constexpr uint32_t MaxEdge = 256;

	// Decodes running at once. A number about latency, not about cores: the queue is fed by what
	// is on screen, and a deeper one only delays the tiles the user is looking at.
	static constexpr uint32_t MaxInFlight = 4;

	// Retained thumbnails, evicted least-recently-used first.
	static constexpr size_t MaxEntries = 256;

	// Pictures shown on a directory tile.
	static constexpr size_t FolderPreviewCount = 3;

	// One picture a directory tile may show. The mtime travels with the path so the preview and
	// the file's own tile ask for the same cache key and share one texture.
	struct PreviewRef {
		String path;
		int64_t mtime = 0;
	};

	using TextureCallback = Function<void(Rc<Texture> &&)>;
	using FolderCallback = Function<void(Vector<PreviewRef> &&)>;

	virtual ~ThumbnailCache() = default;

	virtual bool init();

	// The ResourceCache comes from here. Null until the owning node has entered a scene, and a
	// request made before that answers as a failure.
	void setDirector(Director *);

	/* A thumbnail for one file. `mtime` takes part in the key, so an edited file is decoded again
	rather than answered from the cache. The callback runs exactly once, on the app thread, with a
	null texture when the file is not an image, cannot be read or does not decode. */
	void requestThumbnail(StringView path, int64_t mtime, TextureCallback &&);

	/* The first few pictures inside a directory, for its tile. Up to ScanLimit entries are looked
	at, the image files among them are ordered by name and the first FolderPreviewCount are
	answered - so what a directory tile shows is the beginning of what the browser would show in
	it, and a directory of a hundred thousand files still costs one bounded walk. */
	void requestFolderPreview(StringView path, int64_t mtime, FolderCallback &&);

	/* Retire every answer still in flight and drop what has not started. Called on navigation:
	the thumbnails of the directory being left are no longer worth finishing, and the tiles that
	asked for them are gone. Decoded entries stay in the cache - walking back up a tree is the one
	move that has to feel instant. */
	void invalidate();

	// For the inspector and the self-check.
	struct Stats {
		uint32_t queued = 0;
		uint32_t inFlight = 0;
		uint32_t cached = 0;
		uint32_t failed = 0;
		uint32_t folders = 0;
	};

	Stats getStats() const;

protected:
	static constexpr size_t ScanLimit = 64;

	struct Entry {
		Rc<Texture> texture;
		uint64_t atime = 0; // a counter, not a clock: only the order matters
		bool failed = false;
	};

	struct Request : public Ref {
		String key;
		String path;
		Vector<TextureCallback> callbacks;
	};

	// The decoded pixels on their way back to the app thread. Plain data: it crosses threads.
	struct Result {
		String key;
		Bytes data;
		uint32_t width = 0;
		uint32_t height = 0;
		bool ok = false;
	};

	static String makeKey(StringView path, int64_t mtime);

	// Extension only. The real test is the decoder, which runs on the worker; this one keeps a
	// directory walk from opening every file it passes.
	static bool isImageName(StringView name);

	// The worker body of a thumbnail request, and of a directory scan. Static: both run off the
	// app thread, where nothing of this object may be touched.
	static Result decode(StringView key, StringView path);
	static void scanFolder(StringView path, Vector<PreviewRef> &out);

	void enqueue(Rc<Request> &&);
	void pump();
	void start(Rc<Request> &&);
	void complete(Result &&, uint64_t generation);

	Entry *touch(StringView key);
	void evict();

	Director *_director = nullptr;

	Map<String, Entry> _entries;
	Map<String, Rc<Request>> _pending; // by key, so two tiles on one file decode once
	Vector<Rc<Request>> _queue;
	Map<String, Vector<PreviewRef>> _folders;
	Set<String> _foldersInFlight;

	uint64_t _generation = 1;
	uint64_t _clock = 0;
	uint32_t _inFlight = 0;
	uint32_t _failed = 0;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_THUMBNAILCACHE_H_
