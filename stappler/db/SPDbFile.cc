/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#include "SPDbFile.h"

#include "SPDbAdapter.h"
#include "SPFilepath.h"
#include "SPIO.h"
#include "SPDbField.h"
#include "SPDbScheme.h"
#include "SPDbTransaction.h"
#include "SPDbWorker.h"
#include "SPFilesystem.h"

#if MODULE_STAPPLER_BITMAP
#include "SPBitmap.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::db {

String File::getFilesystemPath(const ApplicationInterface *app, uint64_t oid) {
	return toString(app->getDocumentRoot(), "/uploads/", oid);
}

static bool File_isImage(const StringView &type) {
	return type == "image/gif" || type == "image/jpeg" || type == "image/pjpeg"
			|| type == "image/png" || type == "image/tiff" || type == "image/webp"
			|| type == "image/svg+xml";
}

static bool File_validateFileField(const ApplicationInterface *app, const Field &field,
		size_t writeSize, const StringView &type) {
	auto ffield = static_cast<const FieldFile *>(field.getSlot());
	// check size
	if (writeSize > ffield->maxSize) {
		app->error("Storage", "File is larger then max file size in field",
				Value{std::make_pair("field", Value(field.getName())),
					std::make_pair("max", Value((int64_t)ffield->maxSize)),
					std::make_pair("size", Value((int64_t)writeSize))});
		return false;
	}

	// check type
	auto &types = ffield->allowedTypes;
	if (!types.empty()) {
		bool ret = false;
		for (auto &it : types) {
			if (type == it) {
				ret = true;
				break;
			}
		}
		if (!ret) {
			app->error("Storage", "Invalid file type for field",
					Value{std::make_pair("field", Value(field.getName())),
						std::make_pair("type", Value(type))});
			return false;
		}
	}
	return true;
}

static bool File_validateImageField(const ApplicationInterface *app, const Field &field,
		size_t writeSize, StringView type, stappler::io::Producer file) {
#ifndef MODULE_STAPPLER_BITMAP
	app->error("Storage",
			"MODULE_STAPPLER_BITMAP was not enabled to support bitmaps within storage");
	return false;
#else
	auto ffield = static_cast<const FieldImage *>(field.getSlot());

	// check size
	if (writeSize > ffield->maxSize) {
		app->error("Storage", "File is larger then max file size in field",
				Value{std::make_pair("field", Value(field.getName())),
					std::make_pair("max", Value((int64_t)ffield->maxSize)),
					std::make_pair("size", Value((int64_t)writeSize))});
		return false;
	}

	if (!File_isImage(type)) {
		app->error("Storage", "Unknown image type for field",
				Value{std::make_pair("field", Value(field.getName())),
					std::make_pair("type", Value(type))});
		return false;
	}

	// check type
	auto &types = ffield->allowedTypes;
	if (!types.empty()) {
		bool ret = false;
		for (auto &it : types) {
			if (type == it) {
				ret = true;
				break;
			}
		}
		if (!ret) {
			app->error("Storage", "Invalid file type for field",
					Value{std::make_pair("field", Value(field.getName())),
						std::make_pair("type", Value(type))});
			return false;
		}
	}

	uint32_t width = 0, height = 0;
	if (!bitmap::getImageSize(file, width, height) || width == 0 || height == 0) {
		app->error("Storage", "Fail to detect file size with");
		return false;
	}

	if (ffield->minImageSize.policy == ImagePolicy::Reject) {
		if (ffield->minImageSize.width > width || ffield->minImageSize.height > height) {
			app->error("Storage", "Image is to small, rejected by policy rule",
					Value{
						std::make_pair("min",
								Value{std::make_pair("width", Value(ffield->minImageSize.width)),
									std::make_pair("height", Value(ffield->minImageSize.height))}),
						std::make_pair("current",
								Value{std::make_pair("width", Value(width)),
									std::make_pair("height", Value(height))})});
			return false;
		}
	}

	if (ffield->maxImageSize.policy == ImagePolicy::Reject) {
		if (ffield->maxImageSize.width < width || ffield->maxImageSize.height < height) {
			app->error("Storage", "Image is to large, rejected by policy rule",
					Value{
						std::make_pair("max",
								Value{std::make_pair("width", Value(ffield->maxImageSize.width)),
									std::make_pair("height", Value(ffield->maxImageSize.height))}),
						std::make_pair("current",
								Value{std::make_pair("width", Value(width)),
									std::make_pair("height", Value(height))})});
			return false;
		}
	}
	return true;
#endif
}

bool File::validateFileField(const ApplicationInterface *app, const Field &field,
		const InputFile &file) {
	if (field.getType() == db::Type::File) {
		return File_validateFileField(app, field, file.writeSize, file.type);
	} else if (field.getType() == db::Type::Image) {
		return File_validateImageField(app, field, file.writeSize, file.type, file.file);
	}
	return true;
}

bool File::validateFileField(const ApplicationInterface *app, const Field &field,
		const StringView &type, const BytesView &data) {
	if (field.getType() == db::Type::File) {
		return File_validateFileField(app, field, data.size(), type);
	} else if (field.getType() == db::Type::Image) {
		stappler::CoderSource source(data);
		return File_validateImageField(app, field, data.size(), type, source);
	}
	return true;
}

Value File::createFile(const Transaction &t, const Field &f, InputFile &file) {
	auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme();
	Value fileData;
	fileData.setString(file.type, "type");
	fileData.setInteger(file.writeSize, "size");

	if (f.getType() == db::Type::Image || File_isImage(file.type)) {
#if MODULE_STAPPLER_BITMAP
		uint32_t width = 0, height = 0;
		if (bitmap::getImageSize(file.file, width, height)) {
			auto &val = fileData.emplace("image");
			val.setInteger(width, "width");
			val.setInteger(height, "height");
		}
#endif
	}

	fileData = Worker(*scheme, t).create(fileData, true);
	if (fileData && fileData.isInteger("__oid")) {
		auto id = fileData.getInteger("__oid");
		if (file.save(FileInfo{
				File::getFilesystemPath(t.getAdapter().getApplicationInterface(), id)})) {
			return Value(id);
		}
	}

	file.close();
	return Value();
}

Value File::createFile(const Transaction &t, const StringView &type, const StringView &path,
		int64_t mtime) {
	auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme();
	Value fileData;
	filesystem::Stat stat;
	if (filesystem::stat(FileInfo(path), stat)) {
		fileData.setInteger(stat.size, "size");
	}

	if (mtime) {
		fileData.setInteger(mtime, "mtime");
	}

#if MODULE_STAPPLER_BITMAP
	uint32_t width = 0, height = 0;
	auto file = filesystem::openForReading(FileInfo(path));
	auto fmt = bitmap::detectFormat(file);
	if ((fmt.second.empty() && fmt.first == bitmap::FileFormat::Custom)
			|| !bitmap::getImageSize(file, width, height)) {
		fileData.setString(type, "type");
	} else {
		auto &val = fileData.emplace("image");
		val.setInteger(width, "width");
		val.setInteger(height, "height");
		if (fmt.first != bitmap::FileFormat::Custom) {
			fileData.setString(bitmap::getMimeType(fmt.first), "type");
		} else {
			fileData.setString(type, "type");
		}
	}
#else
	fileData.setString(type, "type");
#endif

	fileData = Worker(*scheme, t).create(fileData, true);
	if (fileData && fileData.isInteger("__oid")) {
		auto id = fileData.getInteger("__oid");
		auto filePath = File::getFilesystemPath(t.getAdapter().getApplicationInterface(), id);
		if (filesystem::move(FileInfo(path), FileInfo{filePath})) {
			return Value(id);
		} else {
			Worker(*scheme, t).remove(fileData.getInteger("__oid"));
		}
	}

	filesystem::remove(FileInfo(path));
	return Value();
}

Value File::createFile(const Transaction &t, const StringView &type, const BytesView &data,
		int64_t mtime) {
	auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme();
	auto size = data.size();

	Value fileData;
	fileData.setString(type, "type");
	fileData.setInteger(size, "size");
	if (mtime) {
		fileData.setInteger(mtime, "mtime");
	}

#if MODULE_STAPPLER_BITMAP
	uint32_t width = 0, height = 0;
	stappler::CoderSource source(data);
	if (bitmap::getImageSize(source, width, height)) {
		auto &val = fileData.emplace("image");
		val.setInteger(width, "width");
		val.setInteger(height, "height");
	}
#endif

	fileData = Worker(*scheme, t).create(fileData, true);
	if (fileData && fileData.isInteger("__oid")) {
		auto id = fileData.getInteger("__oid");
		auto filePath = File::getFilesystemPath(t.getAdapter().getApplicationInterface(), id);
		if (stappler::filesystem::write(FileInfo{filePath}, data)) {
			return Value(id);
		} else {
			Worker(*scheme, t).remove(fileData.getInteger("__oid"));
		}
	}

	return Value();
}

#if MODULE_STAPPLER_BITMAP
static bool getTargetImageSize(uint32_t W, uint32_t H, const MinImageSize &min,
		const MaxImageSize &max, uint32_t &tW, uint32_t &tH) {

	if (min.width > W || min.height > H) {
		float scale = 0.0f;
		if (min.width == 0) {
			scale = (float)min.height / (float)H;
		} else if (min.height == 0) {
			scale = (float)min.width / (float)W;
		} else {
			scale = std::min((float)min.width / (float)W, (float)min.height / (float)H);
		}
		tW = W * scale;
		tH = H * scale;
		return true;
	}

	if ((max.width != 0 && max.width < W) || (max.height != 0 && max.height < H)) {
		float scale = 0.0f;
		if (max.width == 0) {
			scale = (float)max.height / (float)H;
		} else if (max.height == 0) {
			scale = (float)max.width / (float)W;
		} else {
			scale = std::min((float)max.width / (float)W, (float)max.height / (float)H);
		}
		tW = (size_t)W * scale;
		tH = (size_t)H * scale;
		return true;
	}

	tW = W;
	tH = H;
	return false;
}

static String saveImage(Bitmap &bmp) {
	filesystem::File file = filesystem::File::open_tmp(config::UPLOAD_TMP_IMAGE_PREFIX,
			filesystem::OpenFlags::Write | filesystem::OpenFlags::Read);
	String path(file.path());
	file.close();

	if (!path.empty()) {
		bool ret = false;
		auto fmt = bmp.getOriginalFormat();
		if (fmt == bitmap::FileFormat::Custom) {
			ret = bmp.save(bmp.getOriginalFormatName(), FileInfo{path});
		} else {
			ret = bmp.save(bmp.getOriginalFormat(), FileInfo{path});
		}

		if (ret) {
			return String(path);
		}
	}

	return String();
}

static String resizeImage(Bitmap &bmp, uint32_t width, uint32_t height) {
	auto newImage = bmp.resample(width, height);
	if (newImage) {
		return saveImage(newImage);
	}

	return String();
}

static Map<String, String> writeImages(const ApplicationInterface *app, const Field &f,
		InputFile &file) {
	auto field = static_cast<const FieldImage *>(f.getSlot());

	uint32_t width = 0, height = 0;
	uint32_t targetWidth, targetHeight;
	if (!bitmap::getImageSize(file.file, width, height)) {
		return Map<String, String>();
	}

	Map<String, String> ret;

	bool needResize = getTargetImageSize(width, height, field->minImageSize, field->maxImageSize,
			targetWidth, targetHeight);
	if (needResize || field->thumbnails.size() > 0) {
		BufferTemplate<Interface> data(file.writeSize);
		stappler::io::Producer prod(file.file);
		prod.seek(0, stappler::io::Seek::Set);
		prod.read(data, file.writeSize);

		Bitmap bmp(data.data(), data.size());
		if (!bmp) {
			app->error("Storage", "Fail to open image");
		} else {
			if (needResize) {
				auto fpath = resizeImage(bmp, targetWidth, targetHeight);
				if (!fpath.empty()) {
					ret.emplace(f.getName().str<Interface>(), sp::move(fpath));
				}
			} else {
				ret.emplace(f.getName().str<Interface>(), file.file.path());
			}

			if (field->thumbnails.size() > 0) {
				for (auto &it : field->thumbnails) {
					getTargetImageSize(width, height, MinImageSize(),
							MaxImageSize(it.width, it.height), targetWidth, targetHeight);

					auto fpath = resizeImage(bmp, targetWidth, targetHeight);
					if (!fpath.empty()) {
						ret.emplace(it.name, sp::move(fpath));
					}
				}
			}
		}
	} else {
		ret.emplace(f.getName().str<Interface>(), file.path);
	}

	return ret;
}

static Map<String, String> writeImages(const ApplicationInterface *app, const Field &f,
		const StringView &type, const BytesView &data) {
	auto field = static_cast<const FieldImage *>(f.getSlot());

	uint32_t width = 0, height = 0;
	uint32_t targetWidth, targetHeight;
	stappler::CoderSource source(data);
	if (!bitmap::getImageSize(source, width, height)) {
		return Map<String, String>();
	}

	Map<String, String> ret;

	bool needResize = getTargetImageSize(width, height, field->minImageSize, field->maxImageSize,
			targetWidth, targetHeight);
	if (needResize || field->thumbnails.size() > 0) {
		Bitmap bmp(data);
		if (!bmp) {
			app->error("Storage", "Fail to open image");
		} else {
			if (needResize) {
				auto fpath = resizeImage(bmp, targetWidth, targetHeight);
				if (!fpath.empty()) {
					ret.emplace(f.getName().str<Interface>(), sp::move(fpath));
				}
			} else {
				auto fpath = saveImage(bmp);
				if (!fpath.empty()) {
					ret.emplace(f.getName().str<Interface>(), sp::move(fpath));
				}
			}

			if (field->thumbnails.size() > 0) {
				for (auto &it : field->thumbnails) {
					getTargetImageSize(width, height, MinImageSize(),
							MaxImageSize(it.width, it.height), targetWidth, targetHeight);

					auto fpath = resizeImage(bmp, targetWidth, targetHeight);
					if (!fpath.empty()) {
						ret.emplace(it.name, sp::move(fpath));
					}
				}
			}
		}
	} else {
		filesystem::File file = filesystem::File::open_tmp(config::UPLOAD_TMP_IMAGE_PREFIX,
				filesystem::OpenFlags::Write | filesystem::OpenFlags::Read);
		file.xsputn((const char *)data.data(), data.size());
		ret.emplace(f.getName().str<Interface>(), file.path());
		file.close();
	}

	return ret;
}
#endif

Value File::createImage(const Transaction &t, const Field &f, InputFile &file) {
	Value ret;

#if MODULE_STAPPLER_BITMAP
	auto files = writeImages(t.getAdapter().getApplicationInterface(), f, file);
	for (auto &it : files) {
		if (it.first == f.getName() && it.second == file.path) {
			auto val = createFile(t, f, file);
			if (val.isInteger()) {
				ret.setValue(sp::move(val), it.first);
			}
		} else {
			auto &field = it.first;
			auto &filePath = it.second;

			auto val = createFile(t, file.type, filePath);
			if (val.isInteger()) {
				ret.setValue(sp::move(val), field);
			}
		}
	}
#endif
	return ret;
}

Value File::createImage(const Transaction &t, const Field &f, const StringView &type,
		const BytesView &data, int64_t mtime) {
	Value ret;

#if MODULE_STAPPLER_BITMAP
	auto files = writeImages(t.getAdapter().getApplicationInterface(), f, type, data);
	for (auto &it : files) {
		auto &field = it.first;
		auto &filePath = it.second;

		auto val = createFile(t, type, filePath, mtime);
		if (val.isInteger()) {
			ret.setValue(sp::move(val), field);
		}
	}
#endif
	return ret;
}

bool File::removeFile(const ApplicationInterface *app, const Value &val) {
	int64_t id = 0;
	if (val.isInteger()) {
		id = val.asInteger();
	} else if (val.isInteger("__oid")) {
		id = val.getInteger("__oid");
	}

	return removeFile(app, id);
}
bool File::removeFile(const ApplicationInterface *app, int64_t id) {
	if (id) {
		auto filePath = File::getFilesystemPath(app, id);
		filesystem::remove(FileInfo{filePath});
		return true;
	}

	return false;
}

bool File::purgeFile(const Transaction &t, const Value &val) {
	int64_t id = 0;
	if (val.isInteger()) {
		id = val.asInteger();
	} else if (val.isInteger("__oid")) {
		id = val.getInteger("__oid");
	}
	return purgeFile(t, id);
}

bool File::purgeFile(const Transaction &t, int64_t id) {
	if (id) {
		if (auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme()) {
			Worker(*scheme, t).remove(id);
			auto filePath = File::getFilesystemPath(t.getAdapter().getApplicationInterface(), id);
			filesystem::remove(FileInfo{filePath});
			return true;
		}
	}
	return false;
}

Value File::getData(const Transaction &t, uint64_t id) {
	if (auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme()) {
		return Worker(*scheme, t).get(id);
	}
	return Value();
}

void File::setData(const Transaction &t, uint64_t id, const Value &val) {
	if (auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme()) {
		Worker(*scheme, t).update(id, val);
	}
}

} // namespace stappler::db
