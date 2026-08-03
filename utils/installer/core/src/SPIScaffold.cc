/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "SPIScaffold.h"
#include "SPITriple.h"
#include "SPIManifest.h"
#include "SPIInstall.h"
#include "SPIEngineSource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// A path as make itself must read it: forward slashes (the build REQUIRES `/`, a backslash is an
// escape on Windows), and every space escaped as `\ ` so the assignment stays ONE word. The escape
// is what a human would write by hand, and the lexer turns it back into the engine's internal
// placeholder — a raw space would make `$(STAPPLER_ROOT)/make/universal.mk` split into two words.
String toMakePath(StringView p) {
	String out;
	out.reserve(p.size());
	for (auto c : p) {
		if (c == ' ') {
			out.push_back('\\');
			out.push_back(' ');
		} else {
			out.push_back(c == '\\' ? '/' : c);
		}
	}
	return out;
}

bool writeTextFile(StringView path, StringView data) {
	return filesystem::write(FileInfo(path),
			BytesView(reinterpret_cast<const uint8_t *>(data.data()), data.size()));
}

// Vendored minimal scene templates (the engine's own code, kept here so the starting project is
// intentionally minimal — a single centered label). Mirrors crates/core/templates/src.
constexpr StringView kSceneH =
		"#ifndef XENOLITH_PROJECT_EXAMPLESCENE_H_\n"
		"#define XENOLITH_PROJECT_EXAMPLESCENE_H_\n\n"
		"#include \"XL2dScene.h\"\n"
		"#include \"XL2dLabel.h\"\n\n"
		"namespace STAPPLER_VERSIONIZED stappler::xenolith::app {\n\n"
		"// A minimal scene: an empty window with a single centered label.\n"
		"class ExampleScene : public basic2d::Scene2d {\n"
		"public:\n"
		"\tvirtual ~ExampleScene() = default;\n\n"
		"\tvirtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,\n"
		"\t\t\tconst core::FrameConstraints &) override;\n"
		"\tvirtual void handleContentSizeDirty() override;\n\n"
		"protected:\n"
		"\tusing Scene::init;\n"
		"\tbasic2d::Label *_label = nullptr;\n"
		"};\n\n"
		"} // namespace stappler::xenolith::app\n\n"
		"#endif /* XENOLITH_PROJECT_EXAMPLESCENE_H_ */\n";

constexpr StringView kSceneCpp =
		"#include \"XLCommon.h\"\n"
		"#include \"XL2dSceneContent.h\"\n"
		"#include \"XLEntryPoint.h\"\n"
		"#include \"ExampleScene.h\"\n\n"
		"namespace STAPPLER_VERSIONIZED stappler::xenolith::app {\n\n"
		"bool ExampleScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,\n"
		"\t\tconst core::FrameConstraints &constraints) {\n"
		"\tusing namespace basic2d;\n\n"
		"\tif (!Scene2d::init(app, window, constraints)) {\n"
		"\t\treturn false;\n"
		"\t}\n\n"
		"\tauto content = Rc<SceneContent2d>::create();\n\n"
		"\t// The whole scene: one centered label.\n"
		"\t_label = content->addChild(Rc<Label>::create(\"Hello from Xenolith!\"), ZOrder(1));\n"
		"\t_label->setAnchorPoint(Anchor::Middle);\n"
		"\t_label->setFontSize(32);\n\n"
		"\tsetContent(content);\n"
		"\treturn true;\n"
		"}\n\n"
		"void ExampleScene::handleContentSizeDirty() {\n"
		"\tScene2d::handleContentSizeDirty();\n"
		"\tauto cs = getContentSize();\n"
		"\tif (_label) {\n"
		"\t\t_label->setPosition(Vec2(cs.width / 2.0f, cs.height / 2.0f));\n"
		"\t}\n"
		"}\n\n"
		"// Registers ExampleScene as the application's primary scene class.\n"
		"DEFINE_PRIMARY_SCENE_CLASS(ExampleScene)\n\n"
		"} // namespace stappler::xenolith::app\n";

// Makefile template. LOCAL_MODULES_PATHS are RELATIVE to the engine root (the build prepends
// $(GLOBAL_ROOT)/), so they must NOT carry an absolute prefix.
String renderMakefile(StringView engineRoot, StringView exe) {
	return toString("STAPPLER_ROOT ?= ", toMakePath(engineRoot), "\n",
			"LOCAL_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))\n\n",
			"LOCAL_EXECUTABLE := ", exe, "\n\n",
			"LOCAL_MODULES_PATHS = \\\n"
			"\tstappler/stappler-modules.mk \\\n"
			"\txenolith/xenolith-modules.mk\n\n"
			"LOCAL_MODULES := \\\n"
			"\truntime \\\n"
			"\txenolith_application \\\n"
			"\txenolith_application_main \\\n"
			"\txenolith_renderer_simpleui \\\n"
			"\txenolith_backend_vk \\\n"
			"\txenolith_resources_assets\n\n"
			"LOCAL_SRCS_DIRS := src\n"
			"LOCAL_INCLUDES_OBJS := src\n\n"
			"include $(STAPPLER_ROOT)/make/universal.mk\n");
}

} // namespace

bool isValidProjectName(StringView name) {
	if (name.empty()) {
		return false;
	}
	for (auto c : name) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
					|| c == '-')) {
			return false;
		}
	}
	return true;
}

String sanitizeProjectName(StringView name) {
	String out;
	for (auto c : name) {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
				|| c == '-') {
			out.push_back(c);
		} else {
			out.push_back('_');
		}
	}
	if (out.empty()) {
		return toString("app");
	}
	return out;
}

ScaffoldResult scaffoldProject(StringView name, StringView location, const Layout &layout,
		StringView engineOverride) {
	ScaffoldResult result;

	if (!isValidProjectName(name)) {
		result.setError(Status::ErrorInvalidArguemnt,
				"project name must use only letters, digits, '-' or '_' (no spaces)");
		return result;
	}

	// A space in the location is fine — the project directory reaches make through
	// Makefile::setRootPath / CURDIR, both of which encode it (PathSpacePlaceholder). Only the
	// project NAME stays space-free: it is used verbatim as the executable identifier.
	auto host = resolveHost(getNativeArch(), getNativeOs());
	if (host.native.empty()) {
		result.setError(Status::ErrorNotSupported, "no SDK host for ", getNativeArch(), "-",
				getNativeOs());
		return result;
	}

	bool engineOk = false;
	auto engineRoot = resolveEngineRoot(layout, engineOverride, &engineOk);
	if (!engineOk) {
		result.setError(Status::ErrorNotFound, "engine not found at ", engineRoot,
				" — run `engine-install` or pass `--engine <path>`");
		return result;
	}

	// Heal stale toolchain links so the engine build finds the store.
	linkToolchainsIntoEnginePath(layout, StringView(engineRoot));

	auto hostBin = mergePath(getComponentDir(layout, Kind::Host, host.native), "bin");
	if (!filesystem::exists(FileInfo(StringView(hostBin)))) {
		result.setError(Status::ErrorNotFound, "host toolchain '", host.native,
				"' not installed — run `install` first");
		return result;
	}

	auto path = mergePath(location, name);
	result.path = path;

	auto src = mergePath(path, "src");
	filesystem::mkdir_recursive(FileInfo(StringView(src)));

	if (!writeTextFile(mergePath(src, "ExampleScene.h"), kSceneH)
			|| !writeTextFile(mergePath(src, "ExampleScene.cpp"), kSceneCpp)) {
		result.setError(Status::ErrorUnknown, "failed to write scene sources");
		return result;
	}

	// Carry the engine's formatting config so format-on-save matches upstream.
	auto clangFormat = mergePath(engineRoot, ".clang-format");
	if (filesystem::exists(FileInfo(StringView(clangFormat)))) {
		filesystem::copy(FileInfo(StringView(clangFormat)),
				FileInfo(StringView(mergePath(path, ".clang-format"))));
	}

	// Never clobber an existing Makefile (user-owned).
	auto makefile = mergePath(path, "Makefile");
	if (!filesystem::exists(FileInfo(StringView(makefile)))) {
		auto content =
				renderMakefile(StringView(engineRoot), StringView(sanitizeProjectName(name)));
		if (!writeTextFile(makefile, content)) {
			result.setError(Status::ErrorUnknown, "failed to write Makefile");
			return result;
		}
	}
	return result;
}

} // namespace stappler::xenolith::installer
