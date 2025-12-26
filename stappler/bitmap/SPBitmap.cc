/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "SPBitmap.h"
#include "SPBytesView.h"
#include "SPFilepath.h"
#include "SPString.h"
#include "SPBuffer.h"
#include "SPLog.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap {

const BitmapFormat &getDefaultFormat(uint32_t);

SPUNUSED static std::unique_lock<std::mutex> lockFormatList();
SPUNUSED static void addCustomFormat(BitmapFormat &&fmt);
SPUNUSED static const std::vector<BitmapFormat *> &getCustomFormats();

static Pair<FileFormat, StringView> _loadData(BitmapWriter &w, const uint8_t *data,
		size_t dataLen) {
	for (int i = 0; i < toInt(FileFormat::Custom); ++i) {
		auto &fmt = getDefaultFormat(i);
		if (fmt.is(data, dataLen) && fmt.isReadable()) {
			if (fmt.load(data, dataLen, w)) {
				return pair(FileFormat(i), fmt.getName());
			}
		}
	}

	memory::vector<BitmapFormat *> fns;

	auto lock = lockFormatList();

	fns.reserve(getCustomFormats().size());
	for (auto &it : getCustomFormats()) {
		if (it->isReadable()) {
			fns.emplace_back(it);
		}
	}

	lock.unlock();

	for (auto &it : fns) {
		if (it->is(data, dataLen) && it->load(data, dataLen, w)) {
			return pair(FileFormat::Custom, it->getName());
		}
	}

	return pair(FileFormat::Custom, StringView());
}

struct BitmapPoolTarget {
	memory::PoolInterface::BytesType *bytes;
	const StrideFn *strideFn;
};

struct BitmapStdTarget {
	memory::StandartInterface::BytesType *bytes;
	const StrideFn *strideFn;
};

static void _makeBitmapWriter(BitmapWriter &w, BitmapPoolTarget *target,
		const BitmapTemplate<memory::PoolInterface> &bmp) {
	w.target = target;
	w.color = bmp.format();
	w.alpha = bmp.alpha();
	w.width = bmp.width();
	w.height = bmp.height();
	w.stride = bmp.stride();

	if (target && target->strideFn) {
		w.getStride = [](void *ptr, PixelFormat f, uint32_t w) {
			return (*((BitmapPoolTarget *)ptr)->strideFn)(f, w);
		};
	} else {
		w.getStride = nullptr;
	}

	w.push = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto bytes = ((BitmapPoolTarget *)ptr)->bytes;
		auto origSize = bytes->size();
		bytes->resize(origSize + size);
		memcpy(bytes->data() + origSize, data, size);
	};
	w.resize = [](void *ptr, uint32_t size) { ((BitmapPoolTarget *)ptr)->bytes->resize(size); };
	w.getData = [](void *ptr, uint32_t location) {
		return ((BitmapPoolTarget *)ptr)->bytes->data() + location;
	};
	w.assign = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto bytes = ((BitmapPoolTarget *)ptr)->bytes;
		bytes->resize(size);
		memcpy(bytes->data(), data, size);
	};
	w.clear = [](void *ptr) { ((BitmapPoolTarget *)ptr)->bytes->clear(); };
}

static void _makeBitmapWriter(BitmapWriter &w, BitmapStdTarget *target,
		const BitmapTemplate<memory::StandartInterface> &bmp) {
	w.target = target;
	w.color = bmp.format();
	w.alpha = bmp.alpha();
	w.width = bmp.width();
	w.height = bmp.height();
	w.stride = bmp.stride();

	if (target && target->strideFn) {
		w.getStride = [](void *ptr, PixelFormat f, uint32_t w) {
			return (*((BitmapPoolTarget *)ptr)->strideFn)(f, w);
		};
	} else {
		w.getStride = nullptr;
	}

	w.push = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto bytes = ((BitmapStdTarget *)ptr)->bytes;
		auto origSize = bytes->size();
		bytes->resize(origSize + size);
		memcpy(bytes->data() + origSize, data, size);
	};
	w.resize = [](void *ptr, uint32_t size) { ((BitmapStdTarget *)ptr)->bytes->resize(size); };
	w.getData = [](void *ptr, uint32_t location) {
		return ((BitmapStdTarget *)ptr)->bytes->data() + location;
	};
	w.assign = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto bytes = ((BitmapStdTarget *)ptr)->bytes;
		bytes->resize(size);
		memcpy(bytes->data(), data, size);
	};
	w.clear = [](void *ptr) { ((BitmapStdTarget *)ptr)->bytes->clear(); };
}

template <>
bool BitmapTemplate<memory::PoolInterface>::loadData(const uint8_t *data, size_t dataLen,
		const StrideFn &strideFn) {
	BitmapPoolTarget target{&_data, strideFn ? &strideFn : nullptr};
	BitmapWriter w;
	_makeBitmapWriter(w, &target, *this);
	auto ret = _loadData(w, data, dataLen);
	if (!ret.second.empty()) {
		_color = w.color;
		_alpha = w.alpha;
		_width = w.width;
		_height = w.height;
		_stride = w.stride;
		_originalFormat = ret.first;
		_originalFormatName = ret.second;
		return true;
	}
	return false;
}

template <>
bool BitmapTemplate<memory::StandartInterface>::loadData(const uint8_t *data, size_t dataLen,
		const StrideFn &strideFn) {
	BitmapStdTarget target{&_data, strideFn ? &strideFn : nullptr};
	BitmapWriter w;
	_makeBitmapWriter(w, &target, *this);
	auto ret = _loadData(w, data, dataLen);
	if (!ret.second.empty()) {
		_color = w.color;
		_alpha = w.alpha;
		_width = w.width;
		_height = w.height;
		_stride = w.stride;
		_originalFormat = ret.first;
		_originalFormatName = ret.second;
		return true;
	}
	return false;
}

template <>
bool BitmapTemplate<memory::StandartInterface>::save(FileFormat fmt, const FileInfo &path,
		bool invert) {
	BitmapWriter w;
	_makeBitmapWriter(w, nullptr, *this);
	auto &support = getDefaultFormat(toInt(fmt));
	if (support.isWritable()) {
		return support.save(path, _data.data(), w, invert);
	} else {
		// fallback to png
		return getDefaultFormat(toInt(FileFormat::Png)).save(path, _data.data(), w, invert);
	}
	return false;
}

template <>
bool BitmapTemplate<memory::PoolInterface>::save(FileFormat fmt, const FileInfo &path,
		bool invert) {
	BitmapWriter w;
	_makeBitmapWriter(w, nullptr, *this);
	auto &support = getDefaultFormat(toInt(fmt));
	if (support.isWritable()) {
		return support.save(path, _data.data(), w, invert);
	} else {
		// fallback to png
		return getDefaultFormat(toInt(FileFormat::Png)).save(path, _data.data(), w, invert);
	}
	return false;
}

template <>
bool BitmapTemplate<memory::StandartInterface>::save(StringView name, const FileInfo &path,
		bool invert) {
	BitmapFormat::save_fn fn = nullptr;

	auto lock = lockFormatList();

	for (auto &it : getCustomFormats()) {
		if (it->getName() == name && it->isWritable()) {
			fn = it->getSaveFn();
		}
	}

	lock.unlock();

	if (fn) {
		BitmapWriter w;
		_makeBitmapWriter(w, nullptr, *this);
		return fn(path, _data.data(), w, invert);
	}
	return false;
}

template <>
bool BitmapTemplate<memory::PoolInterface>::save(StringView name, const FileInfo &path,
		bool invert) {
	BitmapFormat::save_fn fn = nullptr;

	auto lock = lockFormatList();

	for (auto &it : getCustomFormats()) {
		if (it->getName() == name && it->isWritable()) {
			fn = it->getSaveFn();
		}
	}

	lock.unlock();

	if (fn) {
		BitmapWriter w;
		_makeBitmapWriter(w, nullptr, *this);
		return fn(path, _data.data(), w, invert);
	}
	return false;
}

template <>
auto BitmapTemplate<memory::StandartInterface>::write(FileFormat fmt, bool invert) const
		-> memory::StandartInterface::BytesType {
	memory::StandartInterface::BytesType ret;
	BitmapStdTarget target{&ret, nullptr};
	BitmapWriter w;
	_makeBitmapWriter(w, &target, *this);

	auto &support = getDefaultFormat(toInt(fmt));
	if (support.isWritable() && support.write(_data.data(), w, invert)) {
		return ret;
	} else if (getDefaultFormat(toInt(FileFormat::Png)).write(_data.data(), w, invert)) {
		return ret;
	}
	return memory::StandartInterface::BytesType();
}

template <>
auto BitmapTemplate<memory::PoolInterface>::write(FileFormat fmt, bool invert) const
		-> memory::PoolInterface::BytesType {
	memory::PoolInterface::BytesType ret;
	BitmapPoolTarget target{&ret, nullptr};
	BitmapWriter w;
	_makeBitmapWriter(w, &target, *this);

	auto &support = getDefaultFormat(toInt(fmt));
	if (support.isWritable() && support.write(_data.data(), w, invert)) {
		return ret;
	} else if (getDefaultFormat(toInt(FileFormat::Png)).write(_data.data(), w, invert)) {
		return ret;
	}
	return memory::PoolInterface::BytesType();
}

template <>
auto BitmapTemplate<memory::StandartInterface>::write(StringView name, bool invert) const
		-> memory::StandartInterface::BytesType {
	BitmapFormat::write_fn fn = nullptr;

	auto lock = lockFormatList();

	for (auto &it : getCustomFormats()) {
		if (it->getName() == name && it->isWritable()) {
			fn = it->getWriteFn();
		}
	}

	lock.unlock();

	if (fn) {
		memory::StandartInterface::BytesType ret;
		BitmapStdTarget target{&ret, nullptr};
		BitmapWriter w;
		_makeBitmapWriter(w, &target, *this);
		if (fn(_data.data(), w, invert)) {
			return ret;
		}
	}
	return memory::StandartInterface::BytesType();
}

template <>
auto BitmapTemplate<memory::PoolInterface>::write(StringView name, bool invert) const
		-> memory::PoolInterface::BytesType {
	BitmapFormat::write_fn fn = nullptr;

	auto lock = lockFormatList();

	for (auto &it : getCustomFormats()) {
		if (it->getName() == name && it->isWritable()) {
			fn = it->getWriteFn();
		}
	}

	lock.unlock();

	if (fn) {
		memory::PoolInterface::BytesType ret;
		BitmapPoolTarget target{&ret, nullptr};
		BitmapWriter w;
		_makeBitmapWriter(w, &target, *this);
		if (fn(_data.data(), w, invert)) {
			return ret;
		}
	}
	return memory::PoolInterface::BytesType();
}

} // namespace stappler::bitmap
