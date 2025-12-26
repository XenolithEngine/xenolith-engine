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

#ifndef STAPPLER_ZIP_SPZIP_H_
#define STAPPLER_ZIP_SPZIP_H_

#include "SPBytesView.h" // IWYU pragma: keep
#include "SPBuffer.h"
#include "SPTime.h"

#ifdef MODULE_STAPPLER_FILESYSTEM
#include "SPFilepath.h"
#endif

typedef struct zip zip_t;

namespace STAPPLER_VERSIONIZED stappler {

template <typename Interface>
struct ZipBuffer {
	using Buffer = BufferTemplate<Interface>;

	bool readonly = false;
	void *handle = nullptr;
	void (*finalize)(void *) = nullptr;

	Buffer data;
	Buffer buffer;
};

template <typename Interface>
class SP_PUBLIC ZipArchive : public Interface::AllocBaseType {
public:
	static constexpr uint8_t ZIP_SIG1[4] = {0x50, 0x4B, 0x03, 0x04};
	static constexpr uint8_t ZIP_SIG2[4] = {0x50, 0x4B, 0x05, 0x06};
	static constexpr uint8_t ZIP_SIG3[4] = {0x50, 0x4B, 0x07, 0x08};

	using Bytes = typename Interface::BytesType;
	using Buffer = BufferTemplate<Interface>;

	ZipArchive();
	ZipArchive(BytesView, bool readonly);
	ZipArchive(FILE *, bool readonly);

#ifdef MODULE_STAPPLER_FILESYSTEM
	ZipArchive(FileInfo);
#endif

	~ZipArchive();

	bool addDir(StringView name);
	bool addFile(StringView name, BytesView data, bool uncompressed = false);
	bool addFile(StringView name, StringView data, bool uncompressed = false);

	Buffer save();

	explicit operator bool() { return _handle != nullptr; }

	size_t size(bool original = false) const;

	// returns maxOf<uint64_t>() on failure
	uint64_t locateFile(StringView) const;

	StringView getFileName(uint64_t idx, bool original = false) const;

	void ftw(const Callback<void(uint64_t, StringView path, size_t size, Time time)> &,
			bool original = false) const;

	bool readFile(StringView, const Callback<void(BytesView)> &) const;
	bool readFile(uint64_t, const Callback<void(BytesView)> &) const;

protected:
	ZipBuffer<Interface> _data;
	zip_t *_handle = nullptr;
};

template <typename Interface>
ZipArchive<Interface>::ZipArchive() : ZipArchive(BytesView()) { }

template <typename Interface>
bool ZipArchive<Interface>::addFile(StringView name, StringView data, bool uncompressed) {
	return addFile(name, BytesView((const uint8_t *)data.data(), data.size()), uncompressed);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIP_H_ */
