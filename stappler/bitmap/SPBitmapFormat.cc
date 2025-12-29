/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPBitmapFormat.h"
#include "SPStringView.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap {

const BitmapFormat &getDefaultFormat(uint32_t);
static std::unique_lock<std::mutex> lockFormatList();
static void addCustomFormat(BitmapFormat &&fmt);
static const std::vector<BitmapFormat *> &getCustomFormats();

void BitmapFormat::add(BitmapFormat &&fmt) { addCustomFormat(move(fmt)); }

BitmapFormat::BitmapFormat(FileFormat f, const check_fn &c, const size_fn &s, const info_fn &i,
		const load_fn &l, const write_fn &wr, const save_fn &sv)
: check_ptr(c)
, size_ptr(s)
, info_ptr(i)
, load_ptr(l)
, write_ptr(wr)
, save_ptr(sv)
, _format(f)
, _name()
, _mime(getMimeType(f)) {
	assert(f != FileFormat::Custom);
	if (check_ptr && size_ptr) {
		_flags |= Recognizable;
	}
	if (load_ptr) {
		_flags |= Readable;
	}
	if (save_ptr || write_ptr) {
		_flags |= Writable;
	}

	switch (_format) {
	case FileFormat::Png: _name = StringView("PNG"); break;
	case FileFormat::Jpeg: _name = StringView("JPEG"); break;
	case FileFormat::WebpLossless: _name = StringView("WebP-lossless"); break;
	case FileFormat::WebpLossy: _name = StringView("WebP-lossy"); break;
	case FileFormat::Svg: _name = StringView("SVG"); break;
	case FileFormat::Gif: _name = StringView("GIF"); break;
	case FileFormat::Tiff: _name = StringView("TIFF"); break;
	default: break;
	}
}

BitmapFormat::BitmapFormat(StringView n, StringView mime, const check_fn &c, const size_fn &s,
		const info_fn &i, const load_fn &l, const write_fn &wr, const save_fn &sv)
: check_ptr(c)
, size_ptr(s)
, info_ptr(i)
, load_ptr(l)
, write_ptr(wr)
, save_ptr(sv)
, _format(FileFormat::Custom)
, _name(n)
, _mime(mime) {
	if (check_ptr && size_ptr) {
		_flags |= Recognizable;
	}
	if (load_ptr) {
		_flags |= Readable;
	}
	if (save_ptr || write_ptr) {
		_flags |= Writable;
	}
}

bool BitmapFormat::isRecognizable() const { return (_flags & Recognizable) != None; }
bool BitmapFormat::isReadable() const { return (_flags & Readable) != None; }
bool BitmapFormat::isWritable() const { return (_flags & Writable) != None; }

bool BitmapFormat::is(const uint8_t *data, size_t dataLen) const {
	if (check_ptr) {
		return check_ptr(data, dataLen);
	}
	return false;
}
bool BitmapFormat::getSize(const io::Producer &file, StackBuffer<512> &buf, uint32_t &width,
		uint32_t &height) const {
	if (size_ptr) {
		return size_ptr(file, buf, width, height);
	}
	return false;
}

bool BitmapFormat::getInfo(const uint8_t *data, size_t size, ImageInfo &info) const {
	if (info_ptr) {
		return info_ptr(data, size, info);
	}
	return false;
}

bool BitmapFormat::load(const uint8_t *data, size_t size, BitmapWriter &state) const {
	if (load_ptr) {
		return load_ptr(data, size, state);
	}
	return false;
}

bool BitmapFormat::write(const uint8_t *data, BitmapWriter &state, bool invert) const {
	if (write_ptr) {
		return write_ptr(data, state, invert);
	}
	return false;
}

bool BitmapFormat::save(const FileInfo &path, const uint8_t *data, BitmapWriter &state,
		bool invert) const {
	if (save_ptr) {
		return save_ptr(path, data, state, invert);
	}
	return false;
}

bool getImageSize(const FileInfo &fileInfo, uint32_t &width, uint32_t &height) {
	auto file = filesystem::openForReading(fileInfo);
	return getImageSize(file, width, height);
}

bool getImageSize(const io::Producer &file, uint32_t &width, uint32_t &height) {
	StackBuffer<512> data;
	auto dataSize = file.seekAndRead(0, data, 512);
	if (dataSize < 32) {
		return false;
	}

	for (int i = 0; i < toInt(FileFormat::Custom); ++i) {
		if (getDefaultFormat(i).isRecognizable()
				&& getDefaultFormat(i).getSize(file, data, width, height)) {
			return true;
		}
	}

	memory::vector<BitmapFormat::size_fn> fns;

	auto lock = lockFormatList();
	fns.reserve(getCustomFormats().size());

	for (auto &it : getCustomFormats()) {
		if (it->isRecognizable()) {
			fns.emplace_back(it->getSizeFn());
		}
	}

	lock.unlock();

	for (auto &it : fns) {
		if (it(file, data, width, height)) {
			return true;
		}
	}

	return false;
}

bool getImageInfo(BytesView data, ImageInfo &info) {
	for (int i = 0; i < toInt(FileFormat::Custom); ++i) {
		if (getDefaultFormat(i).isReadable() && getDefaultFormat(i).is(data.data(), data.size())
				&& getDefaultFormat(i).getInfo(data.data(), data.size(), info)) {
			info.format = &getDefaultFormat(i);
			return true;
		}
	}

	memory::vector<BitmapFormat *> fns;

	auto lock = lockFormatList();
	fns.reserve(getCustomFormats().size());

	for (auto &it : getCustomFormats()) {
		if (it->isReadable() && it->is(data.data(), data.size())) {
			fns.emplace_back(it);
		}
	}

	lock.unlock();

	for (auto &it : fns) {
		if (it->getInfo(data.data(), data.size(), info)) {
			info.format = it;
			return true;
		}
	}

	return false;
}

bool isImage(const FileInfo &path, bool readable) {
	auto file = filesystem::openForReading(path);
	return isImage(file, readable);
}
bool isImage(const io::Producer &file, bool readable) {
	StackBuffer<512> data;
	if (file.seekAndRead(0, data, 512) < 32) {
		return false;
	}

	return isImage(data.data(), data.size(), readable);
}

bool isImage(const uint8_t *data, size_t dataLen, bool readable) {
	for (int i = 0; i < toInt(FileFormat::Custom); ++i) {
		if (getDefaultFormat(i).isRecognizable() && (!readable || getDefaultFormat(i).isReadable())
				&& getDefaultFormat(i).is(data, dataLen)) {
			return true;
		}
	}

	memory::vector<BitmapFormat::check_fn> fns;

	auto lock = lockFormatList();
	fns.reserve(getCustomFormats().size());

	for (auto &it : getCustomFormats()) {
		if (it->isRecognizable() && (!readable || it->isReadable())) {
			fns.emplace_back(it->getCheckFn());
		}
	}

	lock.unlock();

	for (auto &it : fns) {
		if (it(data, dataLen)) {
			return true;
		}
	}

	return false;
}

Pair<FileFormat, StringView> detectFormat(const FileInfo &path) {
	auto file = filesystem::openForReading(path);
	return detectFormat(file);
}

Pair<FileFormat, StringView> detectFormat(const io::Producer &file) {
	StackBuffer<512> data;
	if (file.seekAndRead(0, data, 512) < 32) {
		return pair(FileFormat::Custom, StringView());
	}

	return detectFormat(data.data(), data.size());
}

Pair<FileFormat, StringView> detectFormat(const uint8_t *data, size_t dataLen) {
	for (int i = 0; i < toInt(FileFormat::Custom); ++i) {
		if (getDefaultFormat(i).isRecognizable() && getDefaultFormat(i).is(data, dataLen)) {
			return pair(getDefaultFormat(i).getFormat(), getDefaultFormat(i).getName());
		}
	}

	memory::vector<Pair<StringView, BitmapFormat::check_fn>> fns;

	auto lock = lockFormatList();
	fns.reserve(getCustomFormats().size());

	for (auto &it : getCustomFormats()) {
		if (it->isRecognizable()) {
			fns.emplace_back(it->getName(), it->getCheckFn());
		}
	}

	lock.unlock();

	for (auto &it : fns) {
		if (it.second(data, dataLen)) {
			return pair(FileFormat::Custom, it.first);
		}
	}

	return pair(FileFormat::Custom, StringView());
}

StringView getMimeType(FileFormat fmt) {
	switch (fmt) {
	case FileFormat::Png: return "image/png"; break;
	case FileFormat::Jpeg: return "image/jpeg"; break;
	case FileFormat::WebpLossless: return "image/webp"; break;
	case FileFormat::WebpLossy: return "image/webp"; break;
	case FileFormat::Svg: return "image/svg+xml"; break;
	case FileFormat::Gif: return "image/gif"; break;
	case FileFormat::Tiff: return "image/tiff"; break;
	case FileFormat::Custom: break;
	}
	return StringView();
}

StringView getMimeType(StringView name) {
	for (uint32_t i = 0; i < toInt(FileFormat::Custom); ++i) {
		if (getDefaultFormat(i).getName() == name) {
			return getDefaultFormat(i).getMime();
		}
	}

	auto lock = lockFormatList();
	for (auto &it : getCustomFormats()) {
		if (it->getName() == name) {
			return it->getMime();
		}
	}
	return StringView();
}

bool check(FileFormat fmt, const uint8_t *data, size_t dataLen) {
	assert(fmt != FileFormat::Custom);
	return getDefaultFormat(toInt(fmt)).is(data, dataLen);
}

bool check(StringView name, const uint8_t *data, size_t dataLen) {
	memory::vector<BitmapFormat::check_fn> fns;

	auto lock = lockFormatList();
	fns.reserve(getCustomFormats().size());

	for (auto &it : getCustomFormats()) {
		if (it->isRecognizable() && it->getName() == name) {
			fns.emplace_back(it->getCheckFn());
		}
	}

	lock.unlock();

	for (auto &it : fns) {
		if (it(data, dataLen)) {
			return true;
		}
	}

	return false;
}

template <>
void convertLine<PixelFormat::RGB888, PixelFormat::RGBA8888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 2; i < l; i += 3) {
		*out++ = in[i]; //R
		*out++ = in[i + 1]; //G
		*out++ = in[i + 2]; //B
		*out++ = 0xFF; //A
	}
}

template <>
void convertLine<PixelFormat::I8, PixelFormat::RGB888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0; i < ins; ++i) {
		*out++ = in[i]; //R
		*out++ = in[i]; //G
		*out++ = in[i]; //B
	}
}

template <>
void convertLine<PixelFormat::IA88, PixelFormat::RGB888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 1; i < l; i += 2) {
		*out++ = in[i]; //R
		*out++ = in[i]; //G
		*out++ = in[i]; //B
	}
}

template <>
void convertLine<PixelFormat::I8, PixelFormat::RGBA8888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0; i < ins; ++i) {
		*out++ = in[i]; //R
		*out++ = in[i]; //G
		*out++ = in[i]; //B
		*out++ = 0xFF; //A
	}
}

template <>
void convertLine<PixelFormat::IA88, PixelFormat::RGBA8888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 1; i < l; i += 2) {
		*out++ = in[i]; //R
		*out++ = in[i]; //G
		*out++ = in[i]; //B
		*out++ = in[i + 1]; //A
	}
}

template <>
void convertLine<PixelFormat::I8, PixelFormat::IA88>(const uint8_t *in, uint8_t *out, uint32_t ins,
		uint32_t outs) {
	for (size_t i = 0; i < ins; ++i) {
		*out++ = in[i];
		*out++ = 0xFF;
	}
}

template <>
void convertLine<PixelFormat::IA88, PixelFormat::A8>(const uint8_t *in, uint8_t *out, uint32_t ins,
		uint32_t outs) {
	for (size_t i = 1; i < ins; i += 2) {
		*out++ = in[i]; //A
	}
}

template <>
void convertLine<PixelFormat::IA88, PixelFormat::I8>(const uint8_t *in, uint8_t *out, uint32_t ins,
		uint32_t outs) {
	for (size_t i = 0, l = ins - 1; i < l; i += 2) {
		*out++ = in[i]; //R
	}
}

template <>
void convertLine<PixelFormat::RGBA8888, PixelFormat::RGB888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 3; i < l; i += 4) {
		*out++ = in[i]; //R
		*out++ = in[i + 1]; //G
		*out++ = in[i + 2]; //B
	}
}

template <>
void convertLine<PixelFormat::RGB888, PixelFormat::I8>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 2; i < l; i += 3) {
		*out++ = (in[i] * 299 + in[i + 1] * 587 + in[i + 2] * 114 + 500)
				/ 1'000; //I =  (R*299 + G*587 + B*114 + 500) / 1000
	}
}

template <>
void convertLine<PixelFormat::RGBA8888, PixelFormat::I8>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 3; i < l; i += 4) {
		*out++ = (in[i] * 299 + in[i + 1] * 587 + in[i + 2] * 114 + 500)
				/ 1'000; //I =  (R*299 + G*587 + B*114 + 500) / 1000
	}
}

template <>
void convertLine<PixelFormat::RGBA8888, PixelFormat::A8>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 3; i < l; i += 4) {
		*out++ = in[i + 3]; //A
	}
}

template <>
void convertLine<PixelFormat::RGB888, PixelFormat::IA88>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 2; i < l; i += 3) {
		*out++ = (in[i] * 299 + in[i + 1] * 587 + in[i + 2] * 114 + 500)
				/ 1'000; //I =  (R*299 + G*587 + B*114 + 500) / 1000
		*out++ = 0xFF;
	}
}

template <>
void convertLine<PixelFormat::RGBA8888, PixelFormat::IA88>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0, l = ins - 3; i < l; i += 4) {
		*out++ = (in[i] * 299 + in[i + 1] * 587 + in[i + 2] * 114 + 500)
				/ 1'000; //I =  (R*299 + G*587 + B*114 + 500) / 1000
		*out++ = in[i + 3];
	}
}

template <>
void convertLine<PixelFormat::A8, PixelFormat::IA88>(const uint8_t *in, uint8_t *out, uint32_t ins,
		uint32_t outs) {
	for (size_t i = 0; i < ins; ++i) {
		*out++ = 0xFF;
		*out++ = in[i];
	}
}

template <>
void convertLine<PixelFormat::A8, PixelFormat::RGB888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	memset(out, 0, outs);
}

template <>
void convertLine<PixelFormat::A8, PixelFormat::RGBA8888>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	for (size_t i = 0; i < ins; ++i) {
		*out++ = 0x00;
		*out++ = 0x00;
		*out++ = 0x00;
		*out++ = in[i];
	}
}

template <>
void convertLine<PixelFormat::RGB888, PixelFormat::A8>(const uint8_t *in, uint8_t *out,
		uint32_t ins, uint32_t outs) {
	memset(out, 0, outs);
}

} // namespace stappler::bitmap
