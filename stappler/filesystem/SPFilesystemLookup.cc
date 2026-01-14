/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include "SPFilesystemLookup.h"
#include "SPFilesystem.h"
#include "SPString.h"
#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem {

void enumeratePaths(FileCategory cat, StringView filename, FileFlags flags, Access a,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	if (filepath::isAboveRoot(filename)) {
		return;
	}

	if (cat < FileCategory::Custom) {
		auto res = sprt::filesystem::getLookupInfo(cat);
		if (!res) {
			slog().warn("filesystem", "No runtime locations for category (", toInt(cat),
					") defined");
		}

		if (hasFlag(flags, FileFlags::MakeDir)) {
			flags |= FileFlags::Writable;
		}

		if (hasFlag(flags, FileFlags::PathMask)) {
			flags |= res->defaultLookupFlags;
		}

		sprt::filepath::reconstructPath([&](StringView path) {
			enumeratePaths(*res, filename, flags, a, cb); //
		}, filename);
	} else {
		sprt::filesystem::getCurrentDir([&](StringView path) {
			auto &info = sprt::filesystem::getCurrentLocation();
			if (a == Access::None || info.interface->_access(info, filename, a) == Status::Ok) {
				cb(info, path);
			}
		}, filename);
	}
}

FileCategory detectResourceCategory(StringView ipath,
		const Callback<void(const ReverseLookupInfo &)> &cb, Access access) {
	if (filepath::isAboveRoot(ipath)) {
		return FileCategory::Custom;
	}

	auto findPathInCategory = [&](LocationCategory cat, const LookupInfo &res,
									  StringView isubpath) {
		bool found = false;
		sprt::filepath::reconstructPath([&](StringView subpath) {
			for (auto &it : res.paths) {
				if (access == Access::None
						|| it.interface->_access(it, subpath, access) == Status::Ok) {
					if (cb) {
						sprt::filepath::merge([&](StringView path) {
							cb(ReverseLookupInfo{
								cat,
								path,
								ipath,
								&it,
							});
						}, it.path, subpath);
					}
					found = true;
					break;
				}
			}
		}, isubpath);
		return found;
	};

	if (ipath.is('%')) {
		StringView subPath = ipath;
		subPath.skipUntil<StringView::Chars<':'>>();
		if (subPath.is(':')) {
			++subPath;
		}

		auto cat = sprt::filesystem::getResourceCategoryByPrefix(ipath);
		if (cat != FileCategory::Custom) {
			auto res = sprt::filesystem::getLookupInfo(cat);
			if (res) {
				subPath.skipChars<StringView::Chars<'/'>>();
				if (findPathInCategory(cat, *res, subPath)) {
					return cat;
				}
			}
		}
	} else if (filepath::isAbsolute(ipath)) {
		LocationCategory resultCat = LocationCategory::Custom;
		const LocationInfo *targetLoc = nullptr;
		uint32_t match = 0;

		auto defaultInterface = sprt::filesystem::getDefaultInterface();

		sprt::filepath::reconstructPath([&](StringView rpath) {
			// cache access result for the default interface, becouse path is absolute
			bool accessResult = true;
			if (access != Access::None) {
				accessResult = defaultInterface->_access(sprt::filesystem::getCurrentLocation(),
									   rpath, access)
						== Status::Ok;
			}

			for (auto lcat : each<LocationCategory>()) {
				auto res = sprt::filesystem::getLookupInfo(lcat);
				if (!res) {
					continue;
				}

				for (auto &it : res->paths) {
					if (it.path > match && hasFlag(it.locationFlags, LocationFlags::Locateable)
							&& rpath.starts_with(it.path) && rpath.at(it.path.size()) == '/'
							&& (access == Access::None
									|| (it.interface == defaultInterface && accessResult)
									|| it.interface->_access(it, rpath, access) == Status::Ok)) {
						targetLoc = &it;
						match = it.path.size();
						resultCat = lcat;
					}
				}
			}

			if (targetLoc) {
				if (cb) {
					auto res = sprt::filesystem::getLookupInfo(resultCat);

					auto subPath = rpath.sub(targetLoc->path.size());
					subPath.skipChars<StringView::Chars<'/'>>();

					sprt::StreamTraits<char>::merge([&](StringView prefixed) {
						cb(ReverseLookupInfo{
							resultCat,
							rpath,
							prefixed,
							targetLoc,
						});
					}, res->prefix, subPath);
				}
			}
		}, ipath);
		return resultCat;
	} else {
		if (access == Access::None) {
			access = Access::Exists;
		}

		LocationCategory resultCat = LocationCategory::Custom;
		sprt::filepath::reconstructPath([&](StringView rpath) {
			for (auto lcat : each<LocationCategory>()) {
				auto res = sprt::filesystem::getLookupInfo(lcat);
				if (!res) {
					continue;
				}

				for (auto &it : res->paths) {
					if (it.interface->_access(it, ipath, access) == Status::Ok) {
						sprt::filepath::merge([&](StringView fullPath) {
							sprt::StreamTraits<char>::merge([&](StringView prefixed) {
								cb(ReverseLookupInfo{
									lcat,
									rpath,
									prefixed,
									&it,
								});
							}, res->prefix, rpath);
						}, it.path, rpath);
						resultCat = lcat;
						break;
					}
				}
				if (resultCat != FileCategory::Custom) {
					break;
				}
			}
		}, ipath);
		return resultCat;
	}

	return LocationCategory::Custom;
}

FileCategory detectResourceCategory(FileCategory category, StringView ipath, FileFlags flags,
		const Callback<void(const ReverseLookupInfo &)> &cb, Access access) {
	if (category == FileCategory::Custom) {
		return detectResourceCategory(ipath, cb, access);
	}

	auto res = sprt::filesystem::getLookupInfo(category);

	if (filepath::isAbsolute(ipath)) {
		const LocationInfo *targetLoc = nullptr;
		uint32_t match = 0;

		auto defaultInterface = sprt::filesystem::getDefaultInterface();

		FileCategory resultCategory = FileCategory::Custom;
		sprt::filepath::reconstructPath([&](StringView rpath) {
			// cache access result for the default interface, becouse path is absolute
			bool accessResult = true;
			if (access != Access::None) {
				accessResult = defaultInterface->_access(sprt::filesystem::getCurrentLocation(),
									   rpath, access)
						== Status::Ok;
			}
			for (auto &it : res->paths) {
				if (it.path > match && hasFlag(it.locationFlags, LocationFlags::Locateable)
						&& rpath.starts_with(it.path) && rpath.at(it.path.size()) == '/'
						&& (access == Access::None
								|| (it.interface == defaultInterface && accessResult)
								|| it.interface->_access(it, rpath, access) == Status::Ok)) {
					targetLoc = &it;
					match = it.path.size();
				}
			}

			if (targetLoc) {
				if (cb) {
					auto subPath = rpath.sub(targetLoc->path.size());
					subPath.skipChars<StringView::Chars<'/'>>();

					sprt::filepath::merge([&](StringView path) {
						sprt::StreamTraits<char>::merge([&](StringView prefixed) {
							cb(ReverseLookupInfo{
								category,
								path,
								prefixed,
								targetLoc,
							});
						}, res->prefix, path);
					}, targetLoc->path, rpath);
				}
				resultCategory = category;
			}
		}, ipath);
		return resultCategory;
	} else {
		FileCategory resultCategory = FileCategory::Custom;
		sprt::filepath::reconstructPath([&](StringView rpath) {
			for (auto &it : res->paths) {
				if (access == Access::None
						|| it.interface->_access(it, rpath, access) == Status::Ok) {
					if (cb) {
						sprt::filepath::merge([&](StringView path) {
							sprt::StreamTraits<char>::merge([&](StringView prefixed) {
								cb(ReverseLookupInfo{
									category,
									path,
									prefixed,
									&it,
								});
							}, res->prefix, path);
						}, it.path, rpath);
					}
					resultCategory = category;
					break;
				}
			}
		}, ipath);
		return resultCategory;
	}
	return FileCategory::Custom;
}

bool enumeratePrefixedPath(StringView ipath, FileFlags flags, Access a,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	if (!ipath.starts_with("%")) {
		return false;
	}

	auto cat = sprt::filesystem::getResourceCategoryByPrefix(ipath);
	if (cat != FileCategory::Max) {
		auto res = sprt::filesystem::getLookupInfo(cat);

		ipath += res->prefix.size();
		ipath.skipChars<StringView::Chars<'/'>>();

		// enumerate target dirs
		if (ipath.empty()) {
			if (a == Access::None) {
				enumeratePaths(cat, flags, cb);
			} else {
				return false;
			}
		}

		if (filepath::isAboveRoot(ipath)) {
			return false;
		}

		bool ret = false;
		sprt::filepath::reconstructPath([&](StringView reconstructed) {
			enumeratePaths(cat, reconstructed, flags, a, cb);
			ret = true;
		}, ipath);
		return ret;
	}
	return false;
}

} // namespace stappler::filesystem


namespace STAPPLER_VERSIONIZED stappler {

std::ostream &operator<<(std::ostream &stream, const FileInfo &fileInfo) {
	stream << "FileInfo{\"" << fileInfo.path << "\"," << fileInfo.category << "}";
	return stream;
}

} // namespace STAPPLER_VERSIONIZED stappler
