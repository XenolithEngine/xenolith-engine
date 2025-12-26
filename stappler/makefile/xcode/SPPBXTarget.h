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

#ifndef CORE_MAKEFILE_XCODE_SPPBXTARGET_H_
#define CORE_MAKEFILE_XCODE_SPPBXTARGET_H_

#include "SPPBXObject.h"
#include "SPPBXBuildPhase.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

struct XCBuildConfiguration final : PBXObject {
	static const XCBuildConfiguration *create(XCodeExport &,
			const Callback<void(XCBuildConfiguration *)> &);

	static void write(const Callback<void(StringView)> &, const XCBuildConfiguration &);

	const PBXFileReference *baseConfiguration = nullptr;
	Map<String, Value> buildSettings;
	String name;

	XCBuildConfiguration(const XCodeExport &r) : PBXObject(r, ISA::XCBuildConfiguration) { }
};

struct XCConfigurationList final : PBXObject {
	static const XCConfigurationList *create(XCodeExport &,
			const Callback<void(XCConfigurationList *)> &);

	static void write(const Callback<void(StringView)> &, const XCConfigurationList &);

	Vector<const XCBuildConfiguration *> buildConfigurations;
	bool defaultConfigurationIsVisible = false;
	const XCBuildConfiguration *defaultConfiguration = nullptr;

	XCConfigurationList(const XCodeExport &r) : PBXObject(r, ISA::XCConfigurationList) { }
};

struct PBXBuildRule final : PBXObject {
	String compilerSpec;
	String dependencyFile;
	String filePatterns;
	String fileType;
	Vector<String> inputFiles;
	bool isEditable;
	String name;
	Vector<String> outputFiles;
	Vector<String> outputFilesCompilerFlags;
	bool runOncePerArchitecture;
	String script;
};

struct PBXTargetDependency final : PBXObject {
	String name;
	String platformFilter;
	Vector<String> platformFilters;
	XCSwiftPackageProductDependency *product;
	PBXTarget *target;
	PBXContainerItemProxy *targetProxy;
};

struct PBXTarget : PBXContainerItem {
	const XCConfigurationList *buildConfigurationList = nullptr;
	Vector<const PBXBuildPhase *> buildPhases;
	Vector<const PBXBuildRule *> buildRules;
	Vector<const PBXTargetDependency *> dependencies;
	Vector<const PBXFileSystemSynchronizedRootGroup *> fileSystemSynchronizedGroups;
	String name;
	Vector<const XCSwiftPackageProductDependency *> packageProductDependencies;
	const PBXFileReference *product = nullptr;
	PBXProductType productType = PBXProductType::none;

	PBXTarget(const XCodeExport &r, ISA i) : PBXContainerItem(r, i) { }
};

struct PBXAggregateTarget final : PBXTarget {
	static PBXAggregateTarget *create();
};

struct PBXLegacyTarget final : PBXTarget {
	static PBXLegacyTarget *create();

	String buildArgumentsString;
	String buildToolPath;
	String buildWorkingDirectory;
	bool passBuildSettingsInEnvironment;
};

struct PBXNativeTarget final : PBXTarget {
	static const PBXNativeTarget *create(XCodeExport &, const Callback<void(PBXNativeTarget *)> &);

	static void write(const Callback<void(StringView)> &, const PBXNativeTarget &);

	String productInstallPath;

	PBXNativeTarget(const XCodeExport &r) : PBXTarget(r, ISA::PBXNativeTarget) { }
};

} // namespace stappler::makefile::xcode

#endif /* CORE_MAKEFILE_XCODE_SPPBXTARGET_H_ */
