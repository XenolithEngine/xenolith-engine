/**
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

#include "SPPBXBuildPhase.h"
#include "SPXCodeProject.h"
#include "xcode/SPPBXObject.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

const PBXCopyFilesBuildPhase *PBXCopyFilesBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXCopyFilesBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXCopyFilesBuildPhase(xctx);
	obj->buildPhase = BuildPhase::copyFiles;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXCopyFilesBuildPhase::write(const Callback<void(StringView)> &cb,
		const PBXCopyFilesBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";

	cb << Line{"isa", "PBXCopyFilesBuildPhase"};

	cb << "\t};\n";
}

const PBXFrameworksBuildPhase *PBXFrameworksBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXFrameworksBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXFrameworksBuildPhase(xctx);
	obj->buildPhase = BuildPhase::frameworks;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXFrameworksBuildPhase::write(const Callback<void(StringView)> &cb,
		const PBXFrameworksBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";
	cb << Line{"isa", "PBXFrameworksBuildPhase"};
	cb << Line{"buildActionMask", phase.buildActionMask};
	cb << RefArray{"files", phase.files};
	cb << Line{"runOnlyForDeploymentPostprocessing", phase.runOnlyForDeploymentPostprocessing};
	cb << "\t};\n";
}


const PBXHeadersBuildPhase *PBXHeadersBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXHeadersBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXHeadersBuildPhase(xctx);
	obj->buildPhase = BuildPhase::headers;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXHeadersBuildPhase::write(const Callback<void(StringView)> &cb,
		const PBXHeadersBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";
	cb << Line{"isa", "PBXHeadersBuildPhase"};
	cb << Line{"buildActionMask", phase.buildActionMask};
	cb << RefArray{"files", phase.files};
	cb << Line{"runOnlyForDeploymentPostprocessing", phase.runOnlyForDeploymentPostprocessing};
	cb << "\t};\n";
}

const PBXResourcesBuildPhase *PBXResourcesBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXResourcesBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXResourcesBuildPhase(xctx);
	obj->buildPhase = BuildPhase::resources;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXResourcesBuildPhase::write(const Callback<void(StringView)> &cb,
		const PBXResourcesBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";
	cb << Line{"isa", "PBXResourcesBuildPhase"};
	cb << Line{"buildActionMask", phase.buildActionMask};
	cb << RefArray{"files", phase.files};
	cb << Line{"runOnlyForDeploymentPostprocessing", phase.runOnlyForDeploymentPostprocessing};
	cb << "\t};\n";
}

const PBXRezBuildPhase *PBXRezBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXRezBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXRezBuildPhase(xctx);
	obj->buildPhase = BuildPhase::carbonResources;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXRezBuildPhase::write(const Callback<void(StringView)> &cb, const PBXRezBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";

	cb << Line{"isa", "PBXRezBuildPhase"};

	cb << "\t};\n";
}

const PBXShellScriptBuildPhase *PBXShellScriptBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXShellScriptBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXShellScriptBuildPhase(xctx);
	obj->buildPhase = BuildPhase::runScript;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXShellScriptBuildPhase::write(const Callback<void(StringView)> &cb,
		const PBXShellScriptBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";

	cb << Line{"isa", "PBXShellScriptBuildPhase"};

	cb << "\t};\n";
}

const PBXSourcesBuildPhase *PBXSourcesBuildPhase::create(XCodeExport &xctx,
		const Callback<void(PBXSourcesBuildPhase *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXSourcesBuildPhase(xctx);
	obj->buildPhase = BuildPhase::sources;

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXSourcesBuildPhase::write(const Callback<void(StringView)> &cb,
		const PBXSourcesBuildPhase &phase) {
	cb << '\t' << getStringId(phase.id) << " = {\n";
	cb << Line{"isa", "PBXSourcesBuildPhase"};
	cb << Line{"buildActionMask", phase.buildActionMask};
	cb << RefArray{"files", phase.files};
	cb << Line{"runOnlyForDeploymentPostprocessing", phase.runOnlyForDeploymentPostprocessing};
	cb << "\t};\n";
}

} // namespace stappler::makefile::xcode
