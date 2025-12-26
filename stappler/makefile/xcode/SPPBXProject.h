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

#ifndef CORE_MAKEFILE_XCODE_SPPBXPROJECT_H_
#define CORE_MAKEFILE_XCODE_SPPBXPROJECT_H_

#include "SPPBXBuildPhase.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

struct PBXProject final : PBXObject {
	static const PBXProject *create(XCodeExport &, const Callback<void(PBXProject *)> &);

	static void write(const Callback<void(StringView)> &, const PBXProject &);

	Map<String, Value> attributes;
	const XCConfigurationList *buildConfigurationList = nullptr;
	String compatibilityVersion;
	String developmentRegion;
	int32_t hasScannedForEncodings = 0;
	Vector<String> knownRegions;
	Vector<const XCLocalSwiftPackageReference *> localPackages;
	const PBXGroup *mainGroup = nullptr;
	String name;
	int32_t preferredProjectObjectVersion;
	const PBXGroup *productsGroup = nullptr;
	String projectDirPath;
	String projectRoot;
	Vector<Map<String, const PBXFileElement *>> projects;
	Vector<const XCRemoteSwiftPackageReference *> remotePackages;
	Map<const PBXTarget *, Map<String, Value>> targetAttributes;
	Vector<const PBXTarget *> targets;

	PBXProject(const XCodeExport &r) : PBXObject(r, ISA::PBXProject) { }
};

} // namespace stappler::makefile::xcode

#endif /* CORE_MAKEFILE_XCODE_SPPBXPROJECT_H_ */
