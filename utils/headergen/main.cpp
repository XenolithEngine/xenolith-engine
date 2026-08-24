/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPFilesystem.h"
#include "SPBitmap.h"
#include "SPVectorImage.h"
#include "SPData.h"
// The Vulkan registry generator is not ported to this tree - see the Makefile. `material` is, and
// it is the one the icons need.
//#include "RegistryData.h"

static constexpr auto HELP_STRING(
		R"HelpString(headergen <options> registry|icons|material
Options:
    -v (--verbose)
    -h (--help))HelpString");

namespace STAPPLER_VERSIONIZED stappler::headergen {

using namespace mem_std;

static int parseOptionSwitch(Value &ret, char c, const char *str) {
	if (c == 'h') {
		ret.setBool(true, "help");
	} else if (c == 'v') {
		ret.setBool(true, "verbose");
	}
	return 1;
}

static int parseOptionString(Value &ret, const StringView &str, int argc, const char *argv[]) {
	if (str == "help") {
		ret.setBool(true, "help");
	} else if (str == "verbose") {
		ret.setBool(true, "verbose");
	}
	return 1;
}

struct IconData {
	String name;
	String title;
	Bytes data;
	size_t nbytes;
	size_t ncompressed;
};

static IconData &exportIcon(Map<String, IconData> &icons, StringView name, vg::VectorImage &image) {
	auto paths = image.getPaths();
	if (paths.size() > 1) {
		for (auto &it : paths) {
			if (it.second->getStyle() == vg::DrawFlags::None) {
				image.removePath(it.second);
			}
		}
	}
	paths.clear();

	paths = image.getPaths();

	auto it = paths.begin();
	auto path = it->second->getPath();

	++it;

	while (it != paths.end()) {
		path->addPath(*it->second->getPath());
		++it;
	}

	auto data = path->encode();

	auto nameStr = name.str<Interface>();
	auto titleStr = name.str<Interface>();
	titleStr[0] = ::toupper(titleStr[0]);

	Bytes compressed;
	//auto compressed = data::compress<Interface>(data.data(), data.size(), data::EncodeFormat::Compression::LZ4HCCompression, true);

	if (!compressed.empty()) {
		auto iit = icons.emplace(nameStr,
								IconData{nameStr, titleStr, compressed, data.size(),
									compressed.size()})
						   .first;
		return iit->second;
	} else {
		auto iit = icons.emplace(nameStr,
								IconData{nameStr, titleStr, data, data.size(), compressed.size()})
						   .first;
		return iit->second;
	}
}

auto LICENSE_STRING =
		R"Text(/**
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

// Generated with headergen
)Text";

static void makeMaterialIconSource(FileInfo path, const Map<String, IconData> &icons) {
	StringStream sourceFile;

	sourceFile << LICENSE_STRING <<
			R"Text(
///@ SP_EXCLUDE

#include "XLCommon.h"
#include "XL2dIcons.h"

#include "XL2dIconImage.cc"

// clang-format off

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

)Text";

	for (auto &it : icons) {
		auto text = base16::encode<Interface>(CoderSource(it.second.data));
		auto d = text.c_str();
		bool first = false;
		sourceFile << "static const uint8_t s_icon_" << it.first << "[] = { ";
		for (size_t i = 0; i < text.size() / 2; ++i) {
			if (!first) {
				first = true;
			} else {
				sourceFile << ",";
			}
			sourceFile << "0x" << d[i * 2] << d[i * 2 + 1];
		}
		sourceFile << "};\n";
	}

	sourceFile <<
			R"Text(
StringView getIconName(IconName name) {
	switch (name) {
	case IconName::None: return "Nnne"; break;
	case IconName::Empty: return "Empty"; break;
	case IconName::Stappler_CursorIcon: return "Stappler_CursorIcon"; break;
	case IconName::Stappler_SelectioinStartIcon: return "Stappler_SelectioinStartIcon"; break;
	case IconName::Stappler_SelectioinEndIcon: return "Stappler_SelectioinEndIcon"; break;
	case IconName::Dynamic_Loader: return "Dynamic_Loader"; break;
	case IconName::Dynamic_Nav: return "Dynamic_Nav"; break;
	case IconName::Dynamic_DownloadProgress: return "Dynamic_DownloadProgress"; break;
)Text";

	for (auto &it : icons) {
		sourceFile << "\tcase IconName::" << it.second.title << ": return \"" << it.second.title
				   << "\"; break;\n";
	}

	sourceFile <<
			R"Text(	default: break;
	}
	return StringView();
}

bool getIconData(IconName name, const Callback<void(BytesView)> &cb) {
	switch (name) {
	case IconName::None: break;
	case IconName::Empty: break;
	case IconName::Stappler_CursorIcon: break;
	case IconName::Stappler_SelectioinStartIcon: break;
	case IconName::Stappler_SelectioinEndIcon: break;
	case IconName::Dynamic_Loader: break;
	case IconName::Dynamic_Nav: break;
	case IconName::Dynamic_DownloadProgress: break;
)Text";

	for (auto &it : icons) {
		sourceFile << "\tcase IconName::" << it.second.title << ":";
		sourceFile << "cb(BytesView(s_icon_" << it.first << ", " << it.second.nbytes << "));";
		sourceFile << " return true; break;\n";
	}

	sourceFile <<
			R"Text(	default: break;
	}
	return false;
}

}
)Text";

	filesystem::write(path, sourceFile.str());
}

static void makeMaterialIconHeader(FileInfo path, const Map<String, IconData> &icons) {
	StringStream headerFile;

	headerFile << LICENSE_STRING <<
			R"Text(
#ifndef XENOLITH_RENDERER_BASIC2D_ICONS_XL2DICONS_H_
#define XENOLITH_RENDERER_BASIC2D_ICONS_XL2DICONS_H_

#include "XLCommon.h"
#include "SPVectorImage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

enum class IconName : uint16_t {
	None = 0,
	Empty,

	Stappler_CursorIcon,
	Stappler_SelectioinStartIcon,
	Stappler_SelectioinEndIcon,

	Dynamic_Loader,
	Dynamic_Nav,
	Dynamic_DownloadProgress,

)Text";
	for (auto &it : icons) { headerFile << "\t" << it.second.title << ",\n"; }

	headerFile <<
			R"Text(	Max
};

SP_PUBLIC StringView getIconName(IconName);
SP_PUBLIC bool getIconData(IconName, const Callback<void(BytesView)> &);

SP_PUBLIC void drawIcon(vg::VectorImage &, IconName, float progress);

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_ICONS_XL2DICONS_H_ */
)Text";

	filesystem::write(path, headerFile.str());
}

static int exportMaterialIcons(const FileInfo path) {
	size_t i = 0;
	Map<String, IconData> icons;

	filesystem::ftw(path, [&](const FileInfo &subpath, FileType type) {
		if (type == FileType::File) {
			auto name = filepath::name(filepath::root(subpath));
			//sprt::cout << name << "\n";
			if (name == "materialicons" || name == "materialiconsoutlined") {
				if (filepath::fullExtension(subpath) == "svg"
						&& filepath::name(subpath) == "24px") {
					StringStream iconName;
					bool empty = true;
					auto p = filepath::root(subpath.path);
					if (p.starts_with(path.path)) {
						p += path.path.size();
						if (p.is('/')) {
							++p;
						}
					}
					// Arguments swapped in this layout: the callback comes first.
					filepath::split([&](StringView substr) {
						if (substr == "materialicons") {
							iconName << "_solid";
						} else if (substr == "materialiconsoutlined") {
							iconName << "_outline";
						} else {
							if (!empty) {
								iconName << "_";
							} else {
								empty = false;
							}
							iconName << substr;
						}
					}, p);

					vg::VectorImage image;
					if (image.init(FileInfo(subpath))) {
						auto &ic = exportIcon(icons, iconName.str(), image);

						sprt::cout << "[" << i << "] " << ic.title << " - " << subpath << " "
								   << ic.nbytes << " - " << ic.ncompressed << "\n";
						++i;
					} else {
						sprt::cout << "Fail to open: " << subpath << "\n";
					}
				}
			} else if (name != "materialiconssharp" && name != "materialiconsround"
					&& name != "materialiconstwotone") {
				sprt::cout << name << " " << subpath << "\n";
			}
		}
		return true;
	});

	size_t full = 0;
	size_t compressed = 0;

	for (auto &it : icons) {
		full += it.second.nbytes;
		if (it.second.ncompressed) {
			compressed += it.second.ncompressed;
		} else {
			compressed += it.second.nbytes;
		}
	}

	sprt::cout << full << " " << compressed << "\n";

	auto headerPath = FileInfo("gen/XL2dIcons.h");
	auto sourcePath = FileInfo("gen/XL2dIcons.cpp");
	filesystem::mkdir(FileInfo(filepath::root(headerPath.path)));
	filesystem::remove(headerPath);
	filesystem::remove(sourcePath);

	makeMaterialIconHeader(headerPath, icons);
	makeMaterialIconSource(sourcePath, icons);

	return 0;
}

SP_EXTERN_C int main(int argc, const char **argv) {
	Value opts;
	Vector<String> args;
	data::parseCommandLineOptions<Interface, Value>(opts, argc, argv, [&](Value &, StringView arg) {
		args.emplace_back(arg.str<Interface>());
	}, &parseOptionSwitch, &parseOptionString);
	if (opts.getBool("help")) {
		sprt::cout << HELP_STRING << "\n";
		return 0;
	}

	if (opts.getBool("verbose")) {
		sprt::cout << " Current work dir: " << filesystem::currentDir<Interface>() << "\n";
		sprt::cout << " Options: " << data::EncodeFormat::Pretty << opts << "\n";
	}

	return perform_main(argc, argv, [&]() -> int {
		if (args.size() > 1 && args.at(1) == "material" && args.size() > 2) {
			auto path = args.at(2);
			return exportMaterialIcons(FileInfo(path));
		}

		/* `registry` and `icons` are NOT available in this tree.

		They are not disabled to save effort on this errand - they do not compile here. Both write
		through `std::ofstream` and read their own stringstreams by `data()/size()`, and this tree
		has neither; porting them means rewriting their output layer, which is a different job from
		moving a build file. `material` touches none of it.

		Said out loud rather than left as a missing branch, because a generator that silently does
		nothing is worse than one that says it cannot. */
		sprt::cout << "headergen material <path-to-material-design-icons>\n"
					  "  registry, icons: not ported to this tree yet\n";
		return 1;
		return 0;
	});
}

} // namespace stappler::headergen
