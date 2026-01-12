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

#ifndef STAPPLER_BITMAP_SPBITMAPFORMAT_H_
#define STAPPLER_BITMAP_SPBITMAPFORMAT_H_

#include "SPIO.h"
#include "SPStringView.h"
#include "SPFilepath.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap {

class BitmapFormat;

enum class FileFormat {
	Png,
	Jpeg,
	WebpLossless,
	WebpLossy,
	Svg,
	Gif,
	Tiff,
	Custom
};

enum class AlphaFormat {
	Premultiplied,
	Unpremultiplied,
	Opaque,
};

enum class PixelFormat {
	Auto, // used by application, do not use with Bitmap directly
	A8,
	I8,
	IA88,
	RGB888,
	RGBA8888,
};

struct ImageInfo {
	PixelFormat color = PixelFormat::Auto;
	AlphaFormat alpha = AlphaFormat::Premultiplied;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t stride = 0;

	const BitmapFormat *format = nullptr;
};

using StrideFn = Callback<uint32_t(PixelFormat, uint32_t)>;

SP_PUBLIC bool getImageSize(const FileInfo &, uint32_t &width, uint32_t &height);
SP_PUBLIC bool getImageSize(const io::Producer &file, uint32_t &width, uint32_t &height);

SP_PUBLIC bool getImageInfo(BytesView, ImageInfo &);

SP_PUBLIC bool isImage(const FileInfo &, bool readable = true);
SP_PUBLIC bool isImage(const io::Producer &file, bool readable = true);
SP_PUBLIC bool isImage(const uint8_t *data, size_t dataLen, bool readable = true);

SP_PUBLIC sprt::pair<FileFormat, StringView> detectFormat(const FileInfo &);
SP_PUBLIC sprt::pair<FileFormat, StringView> detectFormat(const io::Producer &file);
SP_PUBLIC sprt::pair<FileFormat, StringView> detectFormat(const uint8_t *data, size_t dataLen);

SP_PUBLIC StringView getMimeType(FileFormat);
SP_PUBLIC StringView getMimeType(StringView);

SP_PUBLIC bool check(FileFormat, const uint8_t *data, size_t dataLen);
SP_PUBLIC bool check(StringView, const uint8_t *data, size_t dataLen);

SP_PUBLIC const BitmapFormat *getDefaultFormat(FileFormat);
SP_PUBLIC const BitmapFormat *getCustomFormat(StringView);

inline uint8_t getBytesPerPixel(PixelFormat c) {
	switch (c) {
	case PixelFormat::A8: return 1; break;
	case PixelFormat::I8: return 1; break;
	case PixelFormat::IA88: return 2; break;
	case PixelFormat::RGB888: return 3; break;
	case PixelFormat::RGBA8888: return 4; break;
	case PixelFormat::Auto: return 0; break;
	}
	return 0;
}

template <PixelFormat Source, PixelFormat Target>
SP_PUBLIC void convertLine(const uint8_t *in, uint8_t *out, uint32_t ins, uint32_t outs);

template <PixelFormat Source, PixelFormat Target>
size_t convertData(BytesView dataVec, BytesView out, uint32_t inStride, uint32_t outStride) {
	auto dataLen = dataVec.size();
	auto height = dataLen / inStride;
	auto data = dataVec.data();
	// out.resize(height * outStride);
	auto outData = (uint8_t *)out.data();
	for (size_t j = 0; j < height; j++) {
		convertLine<Source, Target>(data + inStride * j, outData + outStride * j, inStride,
				outStride);
	}
	return out.size();
}

struct BitmapWriter : ImageInfo {
	void *target;

	uint32_t (*getStride)(void *, PixelFormat, uint32_t);

	void (*push)(void *, const uint8_t *, uint32_t);
	void (*resize)(void *, uint32_t size);
	uint8_t *(*getData)(void *, uint32_t location);
	void (*assign)(void *, const uint8_t *, uint32_t);
	void (*clear)(void *);
};

class SP_PUBLIC BitmapFormat {
public:
	enum Flags : uint32_t {
		None = 0,
		Recognizable = 1 << 0,
		Readable = 1 << 1,
		Writable = 1 << 2,
	};

	using check_fn = bool (*)(const uint8_t *data, size_t dataLen);
	using size_fn = bool (*)(const io::Producer &file, StackBuffer<512> &, uint32_t &width,
			uint32_t &height);
	using info_fn = bool (*)(const uint8_t *data, size_t size, ImageInfo &);
	using load_fn = bool (*)(const uint8_t *data, size_t size, BitmapWriter &);
	using write_fn = bool (*)(const uint8_t *data, BitmapWriter &, bool invert);
	using save_fn = bool (*)(const FileInfo &, const uint8_t *data, BitmapWriter &, bool invert);

	static void add(BitmapFormat &&);

	BitmapFormat(FileFormat, const check_fn &, const size_fn &, const info_fn & = nullptr,
			const load_fn & = nullptr, const write_fn & = nullptr, const save_fn & = nullptr);

	BitmapFormat(StringView name, StringView mime, const check_fn &, const size_fn &,
			const info_fn & = nullptr, const load_fn & = nullptr, const write_fn & = nullptr,
			const save_fn & = nullptr);

	StringView getName() const { return _name; }
	StringView getMime() const { return _mime; }

	bool isRecognizable() const;
	bool isReadable() const;
	bool isWritable() const;

	Flags getFlags() const { return _flags; }
	FileFormat getFormat() const { return _format; }

	bool is(const uint8_t *data, size_t dataLen) const;
	bool getSize(const io::Producer &file, StackBuffer<512> &, uint32_t &width,
			uint32_t &height) const;

	bool getInfo(const uint8_t *data, size_t size, ImageInfo &) const;

	bool load(const uint8_t *data, size_t size, BitmapWriter &) const;

	bool write(const uint8_t *data, BitmapWriter &, bool invert) const;

	bool save(const FileInfo &, const uint8_t *data, BitmapWriter &, bool invert) const;

	check_fn getCheckFn() const { return check_ptr; }
	size_fn getSizeFn() const { return size_ptr; }
	info_fn getInfoFn() const { return info_ptr; }
	load_fn getLoadFn() const { return load_ptr; }
	write_fn getWriteFn() const { return write_ptr; }
	save_fn getSaveFn() const { return save_ptr; }

protected:
	check_fn check_ptr = nullptr;
	size_fn size_ptr = nullptr;
	info_fn info_ptr = nullptr;
	load_fn load_ptr = nullptr;
	write_fn write_ptr = nullptr;
	save_fn save_ptr = nullptr;

	Flags _flags = None;
	FileFormat _format = FileFormat::Custom;
	StringView _name;
	StringView _mime;
};

SP_DEFINE_ENUM_AS_MASK(BitmapFormat::Flags)

} // namespace stappler::bitmap

#endif /* STAPPLER_BITMAP_SPBITMAPFORMAT_H_ */
