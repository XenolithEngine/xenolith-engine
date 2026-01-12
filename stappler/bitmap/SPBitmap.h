/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_BITMAP_SPBITMAP_H_
#define STAPPLER_BITMAP_SPBITMAP_H_

#include "SPBitmapFormat.h"
#include "SPFilepath.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap {

enum class ResampleFilter {
	Box, // "box"
	Tent, // "tent"
	Bell, // "bell"
	BSpline, // "b-spline"
	Mitchell, // "mitchell"
	Lanczos3, // "lanczos3"
	Blackman, // "blackman"
	Lanczos4, // "Lanczos4"
	Lanczos6, // "lanczos6"
	Lanczos12, // "lanczos12"
	Kaiser, // "kaiser"
	Gaussian, // "gaussian"
	Catmullrom, // "catmullrom"
	QuadInterp, // "quadratic_interp"
	QuadApprox, // "quadratic_approx"
	QuadMix, // "quadratic_mix"

	Default = Lanczos4,
};

template <typename Interface>
class SP_PUBLIC BitmapTemplate : public Interface::AllocBaseType {
public:
	BitmapTemplate();

	// init with jpeg, png or another formatted data
	BitmapTemplate(BytesView, const StrideFn &strideFn = nullptr);

	BitmapTemplate(const uint8_t *, size_t, const StrideFn &strideFn = nullptr);

	// init with raw data
	BitmapTemplate(const uint8_t *d, uint32_t width, uint32_t height,
			PixelFormat c = PixelFormat::RGBA8888, AlphaFormat a = AlphaFormat::Premultiplied,
			uint32_t stride = 0);

	BitmapTemplate(BytesView d, uint32_t width, uint32_t height,
			PixelFormat c = PixelFormat::RGBA8888, AlphaFormat a = AlphaFormat::Premultiplied,
			uint32_t stride = 0);

	BitmapTemplate(typename Interface::BytesType &&d, uint32_t width, uint32_t height,
			PixelFormat c = PixelFormat::RGBA8888, AlphaFormat a = AlphaFormat::Premultiplied,
			uint32_t stride = 0);

	BitmapTemplate(const BitmapTemplate &) = delete;
	BitmapTemplate &operator=(const BitmapTemplate &) = delete;

	BitmapTemplate(BitmapTemplate &&);
	BitmapTemplate &operator=(BitmapTemplate &&);

	uint32_t width() const { return _width; }
	uint32_t height() const { return _height; }
	uint32_t stride() const { return _stride; }

	bool hasStrideOffset() const { return _width * getBytesPerPixel(_color) < _stride; }

	AlphaFormat alpha() const { return _alpha; }
	PixelFormat format() const { return _color; }

	BytesView data() const { return _data; }

	uint8_t *dataPtr() { return _data.data(); }
	const uint8_t *dataPtr() const { return _data.data(); }

	bool updateStride(const StrideFn &strideFn);
	bool convert(PixelFormat, const StrideFn &strideFn = nullptr);
	bool truncate(PixelFormat, const StrideFn &strideFn = nullptr);

	// target should be large enough
	size_t convertWithTarget(uint8_t *target, PixelFormat,
			const StrideFn &strideFn = nullptr) const;

	// init with jpeg or png data
	bool loadData(const uint8_t *data, size_t dataLen, const StrideFn &strideFn = nullptr);
	bool loadData(BytesView, const StrideFn &strideFn = nullptr);

	// init with raw data
	void loadBitmap(const uint8_t *d, uint32_t w, uint32_t h, PixelFormat = PixelFormat::RGBA8888,
			AlphaFormat a = AlphaFormat::Unpremultiplied, uint32_t stride = 0);
	void loadBitmap(BytesView d, uint32_t w, uint32_t h, PixelFormat = PixelFormat::RGBA8888,
			AlphaFormat a = AlphaFormat::Unpremultiplied, uint32_t stride = 0);
	void loadBitmap(typename Interface::BytesType &&d, uint32_t w, uint32_t h,
			PixelFormat = PixelFormat::RGBA8888, AlphaFormat a = AlphaFormat::Unpremultiplied,
			uint32_t stride = 0);

	void alloc(uint32_t w, uint32_t h, PixelFormat = PixelFormat::RGBA8888,
			AlphaFormat a = AlphaFormat::Unpremultiplied, uint32_t stride = 0);

	void alloc(uint8_t, uint32_t w, uint32_t h, PixelFormat = PixelFormat::RGBA8888,
			AlphaFormat a = AlphaFormat::Unpremultiplied, uint32_t stride = 0);

	explicit operator bool() const { return _data.size() != 0; }

	void clear() { _data.clear(); }
	bool empty() const { return _data.empty(); }
	size_t size() const { return _data.size(); }

	FileFormat getOriginalFormat() const { return _originalFormat; }
	StringView getOriginalFormatName() const { return _originalFormatName; }

	bool save(const FileInfo &, bool invert = false);
	bool save(FileFormat, const FileInfo &, bool invert = false);
	bool save(StringView name, const FileInfo &, bool invert = false);

	template <typename BytesType = typename Interface::BytesType>
	auto write(FileFormat = FileFormat::Png, bool invert = false) const -> BytesType;

	template <typename BytesType = typename Interface::BytesType>
	auto write(StringView name, bool invert = false) const -> BytesType;

	// resample with default filter (usually Lanczos4)
	BitmapTemplate resample(uint32_t width, uint32_t height, uint32_t stride = 0) const;

	BitmapTemplate resample(ResampleFilter, uint32_t width, uint32_t height,
			uint32_t stride = 0) const;

protected:
	template <typename BytesType>
	struct BitmapBytesTarget {
		BytesType *bytes = nullptr;
		const StrideFn *strideFn;
	};

	template <typename BytesType = typename Interface::BytesType>
	void _setupWriter(BitmapWriter &, const BitmapBytesTarget<BytesType> *) const;

	void setInfo(uint32_t w, uint32_t h, PixelFormat c,
			AlphaFormat a = AlphaFormat::Unpremultiplied, uint32_t stride = 0);

	PixelFormat _color = PixelFormat::RGBA8888;
	AlphaFormat _alpha = AlphaFormat::Opaque;

	uint32_t _width = 0;
	uint32_t _height = 0;
	uint32_t _stride = 0; // in bytes
	typename Interface::BytesType _data;

	FileFormat _originalFormat = FileFormat::Custom;
	StringView _originalFormatName;
};


template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate() { }

template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate(BytesView data, const StrideFn &strideFn)
: BitmapTemplate(data.data(), data.size(), strideFn) { }


template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate(const uint8_t *data, size_t size,
		const StrideFn &strideFn) {
	if (!loadData(data, size, strideFn)) {
		_data.clear();
	}
}

template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate(const uint8_t *d, uint32_t width, uint32_t height,
		PixelFormat c, AlphaFormat a, uint32_t stride)
: _color(c)
, _alpha(a)
, _width(width)
, _height(height)
, _stride(max(stride, width * getBytesPerPixel(c)))
, _data(d, d + _stride * height) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
}

template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate(BytesView d, uint32_t width, uint32_t height,
		PixelFormat c, AlphaFormat a, uint32_t stride)
: _color(c)
, _alpha(a)
, _width(width)
, _height(height)
, _stride(max(stride, width * getBytesPerPixel(c)))
, _data(d.bytes<Interface>()) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
}

template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate(typename Interface::BytesType &&d, uint32_t width,
		uint32_t height, PixelFormat c, AlphaFormat a, uint32_t stride)
: _color(c)
, _alpha(a)
, _width(width)
, _height(height)
, _stride(max(stride, width * getBytesPerPixel(c)))
, _data(sp::move(d)) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
}

template <typename Interface>
BitmapTemplate<Interface>::BitmapTemplate(BitmapTemplate &&other)
: _color(other._color)
, _alpha(other._alpha)
, _width(other._width)
, _height(other._height)
, _stride(other._stride)
, _data(sp::move(other._data))
, _originalFormat(other._originalFormat)
, _originalFormatName(move(other._originalFormatName)) {
	other._data.clear();
}

template <typename Interface>
auto BitmapTemplate<Interface>::operator=(BitmapTemplate &&other) -> BitmapTemplate & {
	_alpha = other._alpha;
	_color = other._color;
	_width = other._width;
	_height = other._height;
	_stride = other._stride;
	_data = sp::move(other._data);
	_originalFormat = other._originalFormat;
	_originalFormatName = move(other._originalFormatName);
	other._data.clear();
	return *this;
}

template <typename Interface>
void BitmapTemplate<Interface>::loadBitmap(const uint8_t *d, uint32_t w, uint32_t h, PixelFormat c,
		AlphaFormat a, uint32_t stride) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
	setInfo(w, h, c, a, stride);
	_data.clear();
	_data.resize(_stride * h);
	memcpy(_data.data(), d, _data.size());
	_originalFormat = FileFormat::Custom;
	_originalFormatName.clear();
}

template <typename Interface>
void BitmapTemplate<Interface>::loadBitmap(BytesView d, uint32_t w, uint32_t h, PixelFormat c,
		AlphaFormat a, uint32_t stride) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
	setInfo(w, h, c, a, stride);
	_data = d.bytes<Interface>();
	_originalFormat = FileFormat::Custom;
	_originalFormatName.clear();
}

template <typename Interface>
void BitmapTemplate<Interface>::loadBitmap(typename Interface::BytesType &&d, uint32_t w,
		uint32_t h, PixelFormat c, AlphaFormat a, uint32_t stride) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
	setInfo(w, h, c, a, stride);
	_data = sp::move(d);
	_originalFormat = FileFormat::Custom;
	_originalFormatName.clear();
}

template <typename Interface>
void BitmapTemplate<Interface>::alloc(uint32_t w, uint32_t h, PixelFormat c, AlphaFormat a,
		uint32_t stride) {
	alloc(0, w, h, c, a, stride);
}

template <typename Interface>
void BitmapTemplate<Interface>::alloc(uint8_t val, uint32_t w, uint32_t h, PixelFormat c,
		AlphaFormat a, uint32_t stride) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
	setInfo(w, h, c, a, stride);
	_data.clear();
	_data.resize(_stride * h, val);
	_originalFormat = FileFormat::Custom;
	_originalFormatName.clear();
}


template <typename Interface>
template <typename BytesType>
void BitmapTemplate<Interface>::_setupWriter(BitmapWriter &w,
		const BitmapBytesTarget<BytesType> *target) const {
	w.target = (void *)target;
	w.color = format();
	w.alpha = alpha();
	w.width = width();
	w.height = height();
	w.stride = stride();

	if (target && target->strideFn) {
		w.getStride = [](void *ptr, PixelFormat f, uint32_t w) {
			return (*((BitmapBytesTarget<BytesType> *)ptr)->strideFn)(f, w);
		};
	} else {
		w.getStride = nullptr;
	}

	w.push = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto bytes = ((BitmapBytesTarget<BytesType> *)ptr)->bytes;
		auto origSize = bytes->size();
		bytes->resize(origSize + size);
		memcpy(bytes->data() + origSize, data, size);
	};
	w.resize = [](void *ptr, uint32_t size) {
		((BitmapBytesTarget<BytesType> *)ptr)->bytes->resize(size);
	};
	w.getData = [](void *ptr, uint32_t location) {
		return ((BitmapBytesTarget<BytesType> *)ptr)->bytes->data() + location;
	};
	w.assign = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto bytes = ((BitmapBytesTarget<BytesType> *)ptr)->bytes;
		bytes->resize(size);
		memcpy(bytes->data(), data, size);
	};
	w.clear = [](void *ptr) { ((BitmapBytesTarget<BytesType> *)ptr)->bytes->clear(); };
}

template <typename Interface>
void BitmapTemplate<Interface>::setInfo(uint32_t w, uint32_t h, PixelFormat c, AlphaFormat a,
		uint32_t stride) {
	sprt_passert(c != PixelFormat::Auto,
			"Bitmap: Format::Auto should not be used with Bitmap directly");
	_width = w;
	_height = h;
	_stride = max(stride, w * getBytesPerPixel(c));
	_color = c;
	_alpha = a;
}

template <typename Interface>
bool BitmapTemplate<Interface>::updateStride(const StrideFn &strideFn) {
	uint32_t outStride = (strideFn != nullptr)
			? max(strideFn(_color, _width), _width * getBytesPerPixel(_color))
			: _width * getBytesPerPixel(_color);
	if (outStride != _stride) {
		typename Interface::BytesType out;
		out.resize(_height * outStride);
		size_t minStride = _width * getBytesPerPixel(_color);
		for (size_t j = 0; j < _height; j++) {
			memcpy(out.data() + j * outStride, _data.data() + j * _stride, minStride);
		}
		_data = sp::move(out);
		_stride = outStride;
		return true;
	}
	return true;
}

template <typename Interface>
bool BitmapTemplate<Interface>::convert(PixelFormat color, const StrideFn &strideFn) {
	if (color == PixelFormat::Auto) {
		color = _color;
	}
	if (_color == color) {
		return updateStride(strideFn);
	}

	bool ret = false;
	typename Interface::BytesType out;
	uint32_t outStride = (strideFn != nullptr)
			? max(strideFn(color, _width), _width * getBytesPerPixel(color))
			: _width * getBytesPerPixel(color);
	out.resize(_height * outStride);

	ret = convertWithTarget(out.data(), color, strideFn);

	if (ret) {
		_color = color;
		_data = sp::move(out);
		_stride = outStride;
	}

	return ret;
}

template <typename Interface>
bool BitmapTemplate<Interface>::truncate(PixelFormat color, const StrideFn &strideFn) {
	if (color == PixelFormat::Auto) {
		color = _color;
	}
	if (_color == color) {
		return updateStride(strideFn);
	}

	if (getBytesPerPixel(color) == getBytesPerPixel(_color)) {
		_color = color;
		return true;
	}

	auto height = _height;
	auto data = _data.data();
	auto bppIn = getBytesPerPixel(_color);

	typename Interface::BytesType out;
	uint32_t outStride = (strideFn != nullptr)
			? max(strideFn(color, _width), _width * getBytesPerPixel(color))
			: _width * getBytesPerPixel(color);
	out.resize(height * outStride);
	auto bppOut = getBytesPerPixel(color);

	auto fillBytes = min(bppIn, bppOut);
	auto clearBytes = bppOut - fillBytes;

	for (size_t j = 0; j < height; j++) {
		auto inData = data + _stride * j;
		auto outData = out.data() + outStride * j;
		for (size_t i = 0; i < _stride; i += bppIn) {
			for (uint8_t k = 0; k < fillBytes; ++k) { *outData++ = inData[i * bppIn + k]; }
			for (uint8_t k = 0; k < clearBytes; ++k) { *outData++ = 0; }
		}
	}

	_color = color;
	_data = sp::move(out);
	_stride = outStride;

	return true;
}

template <typename Interface>
size_t BitmapTemplate<Interface>::convertWithTarget(uint8_t *target, PixelFormat color,
		const StrideFn &strideFn) const {
	uint32_t outStride = (strideFn != nullptr)
			? max(strideFn(color, _width), _width * getBytesPerPixel(color))
			: _width * getBytesPerPixel(color);
	BytesView out(target, _height * outStride);

	switch (_color) {
	case PixelFormat::A8:
		switch (color) {
		case PixelFormat::A8:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::I8:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::IA88:
			return convertData<PixelFormat::A8, PixelFormat::IA88>(_data, out, _stride, outStride);
			break;
		case PixelFormat::RGB888:
			return convertData<PixelFormat::A8, PixelFormat::RGB888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::RGBA8888:
			return convertData<PixelFormat::A8, PixelFormat::RGBA8888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::Auto: return 0; break;
		}
		break;
	case PixelFormat::I8:
		switch (color) {
		case PixelFormat::A8:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::I8:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::IA88:
			return convertData<PixelFormat::I8, PixelFormat::IA88>(_data, out, _stride, outStride);
			break;
		case PixelFormat::RGB888:
			return convertData<PixelFormat::I8, PixelFormat::RGB888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::RGBA8888:
			return convertData<PixelFormat::I8, PixelFormat::RGBA8888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::Auto: return 0; break;
		}
		break;
	case PixelFormat::IA88:
		switch (color) {
		case PixelFormat::A8:
			return convertData<PixelFormat::IA88, PixelFormat::A8>(_data, out, _stride, outStride);
			break;
		case PixelFormat::I8:
			return convertData<PixelFormat::IA88, PixelFormat::I8>(_data, out, _stride, outStride);
			break;
		case PixelFormat::IA88:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::RGB888:
			return convertData<PixelFormat::IA88, PixelFormat::RGB888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::RGBA8888:
			return convertData<PixelFormat::IA88, PixelFormat::RGBA8888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::Auto: return 0; break;
		}
		break;
	case PixelFormat::RGB888:
		switch (color) {
		case PixelFormat::A8:
			return convertData<PixelFormat::RGB888, PixelFormat::A8>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::I8:
			return convertData<PixelFormat::RGB888, PixelFormat::I8>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::IA88:
			return convertData<PixelFormat::RGB888, PixelFormat::IA88>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::RGB888:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::RGBA8888:
			return convertData<PixelFormat::RGB888, PixelFormat::RGBA8888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::Auto: return 0; break;
		}
		break;
	case PixelFormat::RGBA8888:
		switch (color) {
		case PixelFormat::A8:
			return convertData<PixelFormat::RGBA8888, PixelFormat::A8>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::I8:
			return convertData<PixelFormat::RGBA8888, PixelFormat::I8>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::IA88:
			return convertData<PixelFormat::RGBA8888, PixelFormat::IA88>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::RGB888:
			return convertData<PixelFormat::RGBA8888, PixelFormat::RGB888>(_data, out, _stride,
					outStride);
			break;
		case PixelFormat::RGBA8888:
			memcpy(target, _data.data(), out.size());
			return out.size();
			break;
		case PixelFormat::Auto: return 0; break;
		}
		break;
	case PixelFormat::Auto: return 0; break;
	}

	return 0;
}

template <typename Interface>
bool BitmapTemplate<Interface>::save(const FileInfo &path, bool invert) {
	FileFormat fmt = FileFormat::Png;
	auto ext = filepath::lastExtension(path.path);
	if (ext == "png") {
		fmt = FileFormat::Png;
	} else if (ext == "jpeg" || ext == "jpg") {
		fmt = FileFormat::Jpeg;
	} else if (ext == "webp") {
		fmt = FileFormat::WebpLossless;
	}
	return save(fmt, path, invert);
}

template <typename Interface>
bool BitmapTemplate<Interface>::loadData(BytesView d, const StrideFn &strideFn) {
	return loadData(d.data(), d.size(), strideFn);
}


template <typename Interface>
template <typename BytesType>
auto BitmapTemplate<Interface>::write(FileFormat fmt, bool invert) const -> BytesType {
	BytesType ret;

	BitmapWriter w;
	BitmapBytesTarget<BytesType> target{&ret, nullptr};
	_setupWriter(w, &target);

	auto support = getDefaultFormat(fmt);
	if (support->isWritable() && support->write(_data.data(), w, invert)) {
		return ret;
	} else if (getDefaultFormat(FileFormat::Png)->write(_data.data(), w, invert)) {
		return ret;
	}
	return BytesType();
}

template <typename Interface>
template <typename BytesType>
auto BitmapTemplate<Interface>::write(StringView fmt, bool invert) const -> BytesType {
	BytesType ret;

	BitmapWriter w;
	BitmapBytesTarget<BytesType> target{&ret, nullptr};
	_setupWriter(w, &target);

	auto support = getCustomFormat(fmt);
	if (support->isWritable() && support->write(_data.data(), w, invert)) {
		return ret;
	}
	return BytesType();
}

} // namespace stappler::bitmap

namespace STAPPLER_VERSIONIZED stappler::mem_std {

using Bitmap = bitmap::BitmapTemplate<memory::StandartInterface>;

}

namespace STAPPLER_VERSIONIZED stappler::mem_pool {

using Bitmap = bitmap::BitmapTemplate<memory::PoolInterface>;

}

#endif /* STAPPLER_BITMAP_SPBITMAP_H_ */
