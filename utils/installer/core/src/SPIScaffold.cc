#include "SPIScaffold.h"
#include "SPITriple.h"
#include "SPIManifest.h"
#include "SPIInstall.h"
#include "SPIEngineSource.h"

#include "SPFilesystem.h"
#include "SPFilepath.h"

#include <cstdio>
#include <cstring>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// A path in forward-slash form. GNU make REQUIRES `/` (a backslash is an escape on Windows); a
// no-op where the path already has none.
String make_path(StringView p) {
	String out;
	out.reserve(p.size());
	for (auto c : p) {
		out.push_back(c == '\\' ? '/' : c);
	}
	return out;
}

bool write_file(StringView path, StringView data) {
	return filesystem::write(FileInfo(StringView(path)),
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
String render_makefile(StringView engineRoot, StringView exe) {
	StringView tmpl =
			"STAPPLER_ROOT ?= {{STAPPLER_ROOT}}\n"
			"LOCAL_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))\n\n"
			"LOCAL_EXECUTABLE := {{EXE}}\n\n"
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
			"include $(STAPPLER_ROOT)/make/universal.mk\n";
	String root = make_path(engineRoot);
	String out;
	out.reserve(tmpl.size() + root.size() + exe.size());
	const char *t = tmpl.data();
	for (size_t i = 0; i < tmpl.size();) {
		if (i + 17 <= tmpl.size() && std::strncmp(t + i, "{{STAPPLER_ROOT}}", 17) == 0) {
			out.append(root.data(), root.size());
			i += 17;
		} else if (i + 7 <= tmpl.size() && std::strncmp(t + i, "{{EXE}}", 7) == 0) {
			out.append(exe.data(), exe.size());
			i += 7;
		} else {
			out.push_back(tmpl[i++]);
		}
	}
	return out;
}

} // namespace

bool is_valid_project_name(StringView name) {
	if (name.empty()) {
		return false;
	}
	for (auto c : name) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
					|| c == '_' || c == '-')) {
			return false;
		}
	}
	return true;
}

String sanitize_project_name(StringView name) {
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

ScaffoldResult scaffold_project(StringView name, StringView location, const Layout &layout,
		const String *engineOverride) {
	ScaffoldResult result;

	if (!is_valid_project_name(name)) {
		result.status = Status::ErrorUnknown;
		result.error = toString("project name must use only letters, digits, '-' or '_' (no spaces)");
		return result;
	}
	bool locSpace = false;
	for (auto c : location) {
		if (c == ' ' || c == '\t') {
			locSpace = true;
			break;
		}
	}
	if (locSpace) {
		result.status = Status::ErrorUnknown;
		result.error = toString("project location must not contain spaces (GNU make breaks on them)");
		return result;
	}

	auto host = resolve_host(native_arch(), native_os());
	if (host.native.empty()) {
		result.status = Status::ErrorUnknown;
		char buf[96];
		snprintf(buf, sizeof(buf), "no SDK host for %s-%s", toString(native_arch()).c_str(),
				toString(native_os()).c_str());
		result.error = toString(buf);
		return result;
	}

	bool engineOk = false;
	String engineRoot = resolve_engine_root(layout, engineOverride, &engineOk);
	if (!engineOk) {
		result.status = Status::ErrorUnknown;
		result.error = toString("engine not found at ") + engineRoot
				+ toString(" — run `engine-install` or pass `--engine <path>`");
		return result;
	}

	// Heal stale toolchain links so the engine build finds the store.
	link_toolchains_into_engine_path(layout, StringView(engineRoot));

	String hostBin = component_dir(layout, Kind::Host, host.native) + "/bin";
	if (!filesystem::exists(FileInfo(StringView(hostBin)))) {
		result.status = Status::ErrorUnknown;
		result.error = toString("host toolchain '") + host.native
				+ toString("' not installed — run `install` first");
		return result;
	}

	String path = toString(location);
	if (!path.empty() && path.back() != '/') {
		path += "/";
	}
	path += toString(name);
	result.path = path;

	String src = path + "/src";
	filesystem::mkdir_recursive(FileInfo(StringView(src)));

	if (!write_file(src + "/ExampleScene.h", kSceneH)
			|| !write_file(src + "/ExampleScene.cpp", kSceneCpp)) {
		result.status = Status::ErrorUnknown;
		result.error = toString("failed to write scene sources");
		return result;
	}

	// Carry the engine's formatting config so format-on-save matches upstream.
	String clangFormat = toString(engineRoot) + "/.clang-format";
	if (filesystem::exists(FileInfo(StringView(clangFormat)))) {
		filesystem::copy(FileInfo(StringView(clangFormat)), FileInfo(path + "/.clang-format"));
	}

	// Never clobber an existing Makefile (user-owned).
	String makefile = path + "/Makefile";
	if (!filesystem::exists(FileInfo(StringView(makefile)))) {
		String exe = sanitize_project_name(name);
		if (!write_file(makefile, StringView(render_makefile(StringView(engineRoot), StringView(exe))))) {
			result.status = Status::ErrorUnknown;
			result.error = toString("failed to write Makefile");
			return result;
		}
	}
	return result;
}

} // namespace stappler::xenolith::installer
