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

#include "SPPBXProject.h"
#include "SPPBXTarget.h"
#include "SPPBXFile.h"
#include "SPXCodeProject.h"
#include "xcode/SPPBXObject.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

const PBXProject *PBXProject::create(XCodeExport &xctx, const Callback<void(PBXProject *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXProject(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXProject::write(const Callback<void(StringView)> &cb, const PBXProject &proj) {
	cb << '\t' << getStringId(proj.id) << " /* Project object */ = {\n";
	cb << Line{"isa", "PBXProject"};

	if (proj.buildConfigurationList) {
		cb << Line{"buildConfigurationList", ObjectRef{proj.buildConfigurationList}};
	}

	if (!proj.developmentRegion.empty()) {
		cb << Line{"developmentRegion", StringValue{proj.developmentRegion}};
	}

	cb << Line{"hasScannedForEncodings", proj.hasScannedForEncodings};

	if (!proj.knownRegions.empty()) {
		cb << Array{"knownRegions", proj.knownRegions};
	}

	if (proj.mainGroup) {
		cb << Line{"mainGroup", ObjectRef{proj.mainGroup}};
	}

	if (proj.preferredProjectObjectVersion) {
		cb << Line{"preferredProjectObjectVersion", proj.preferredProjectObjectVersion};
	}

	if (proj.productsGroup) {
		cb << Line{"productRefGroup", ObjectRef{proj.productsGroup}};
	}

	cb << Line{"projectDirPath", StringValue{proj.projectDirPath}};
	cb << Line{"projectRoot", StringValue{proj.projectRoot}};

	if (!proj.targets.empty()) {
		cb << RefArray{"targets", proj.targets};
	}
	cb << "\t};\n";
}

} // namespace stappler::makefile::xcode
