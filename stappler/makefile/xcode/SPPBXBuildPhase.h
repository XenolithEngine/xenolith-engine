/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_MAKEFILE_XCODE_SPPBXBUILDPHASE_H_
#define CORE_MAKEFILE_XCODE_SPPBXBUILDPHASE_H_

#include "SPPBXObject.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

struct PBXBuildPhase : PBXContainerItem {
	uint32_t buildActionMask = 2'147'483'647;
	BuildPhase buildPhase = BuildPhase::sources;
	Vector<const PBXBuildFile *> files;
	Vector<String> inputFileListPaths;
	Vector<String> outputFileListPaths;
	bool runOnlyForDeploymentPostprocessing = false;

	PBXBuildPhase(const XCodeExport &r, ISA i) : PBXContainerItem(r, i) { }
};

struct PBXCopyFilesBuildPhase final : PBXBuildPhase {
	static const PBXCopyFilesBuildPhase *create(XCodeExport &,
			const Callback<void(PBXCopyFilesBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXCopyFilesBuildPhase &);

	enum class SubFolder {
		AbsolutePath,
		Executables,
		Frameworks,
		JavaResources,
		Other,
		Plugins,
		ProductsDirectory,
		Resources,
		SharedFrameworks,
		SharedSupport,
		Wrapper
	};

	String dstPath;
	SubFolder dstSubfolderSpec;
	String name;

	PBXCopyFilesBuildPhase(const XCodeExport &r) : PBXBuildPhase(r, ISA::PBXCopyFilesBuildPhase) { }
};

struct PBXFrameworksBuildPhase final : PBXBuildPhase {
	static const PBXFrameworksBuildPhase *create(XCodeExport &,
			const Callback<void(PBXFrameworksBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXFrameworksBuildPhase &);

	PBXFrameworksBuildPhase(const XCodeExport &r)
	: PBXBuildPhase(r, ISA::PBXFrameworksBuildPhase) { }
};

struct PBXHeadersBuildPhase final : PBXBuildPhase {
	static const PBXHeadersBuildPhase *create(XCodeExport &,
			const Callback<void(PBXHeadersBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXHeadersBuildPhase &);

	PBXHeadersBuildPhase(const XCodeExport &r) : PBXBuildPhase(r, ISA::PBXHeadersBuildPhase) { }
};

struct PBXResourcesBuildPhase final : PBXBuildPhase {
	static const PBXResourcesBuildPhase *create(XCodeExport &,
			const Callback<void(PBXResourcesBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXResourcesBuildPhase &);

	PBXResourcesBuildPhase(const XCodeExport &r) : PBXBuildPhase(r, ISA::PBXResourcesBuildPhase) { }
};

struct PBXRezBuildPhase final : PBXBuildPhase {
	static const PBXRezBuildPhase *create(XCodeExport &,
			const Callback<void(PBXRezBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXRezBuildPhase &);

	PBXRezBuildPhase(const XCodeExport &r) : PBXBuildPhase(r, ISA::PBXRezBuildPhase) { }
};

struct PBXShellScriptBuildPhase final : PBXBuildPhase {
	static const PBXShellScriptBuildPhase *create(XCodeExport &,
			const Callback<void(PBXShellScriptBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXShellScriptBuildPhase &);

	bool alwaysOutOfDate = true;
	String dependencyFile;
	Vector<String> inputPaths;
	String name;
	Vector<String> outputPaths;
	String shellPath;
	String shellScript;
	bool showEnvVarsInLog = false;

	PBXShellScriptBuildPhase(const XCodeExport &r)
	: PBXBuildPhase(r, ISA::PBXShellScriptBuildPhase) { }
};

struct PBXSourcesBuildPhase final : PBXBuildPhase {
	static const PBXSourcesBuildPhase *create(XCodeExport &,
			const Callback<void(PBXSourcesBuildPhase *)> &);

	static void write(const Callback<void(StringView)> &, const PBXSourcesBuildPhase &);

	PBXSourcesBuildPhase(const XCodeExport &r) : PBXBuildPhase(r, ISA::PBXSourcesBuildPhase) { }
};

} // namespace stappler::makefile::xcode

#endif /* CORE_MAKEFILE_XCODE_SPPBXBUILDPHASE_H_ */
