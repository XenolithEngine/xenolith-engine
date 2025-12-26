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

#ifndef CORE_MAKEFILE_XCODE_SPPBXOBJECT_H_
#define CORE_MAKEFILE_XCODE_SPPBXOBJECT_H_

#include "SPCommon.h"
#include "SPMakefile.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

using namespace mem_pool;

enum class ISA {
	PBXObject,
	PBXBuildFile,
	PBXContainerItem,
	PBXFileElement,
	PBXAggregateTarget,
	PBXContainerItemProxy,
	PBXFileReference,
	PBXFileSystemSynchronizedBuildFileExceptionSet,
	PBXFileSystemSynchronizedRootGroup,
	PBXGroup,
	PBXNativeTarget,
	PBXProject,
	PBXCopyFilesBuildPhase,
	PBXFrameworksBuildPhase,
	PBXHeadersBuildPhase,
	PBXResourcesBuildPhase,
	PBXSourcesBuildPhase,
	PBXRezBuildPhase,
	PBXShellScriptBuildPhase,
	PBXTargetDependency,
	XCBuildConfiguration,
	XCConfigurationList,
	XCSwiftPackageProductDependency
};

using Id = std::array<uint8_t, 12>;
using StringId = std::array<char, 24>;

enum class BuildPhase {
	carbonResources,
	copyFiles,
	frameworks,
	headers,
	resources,
	runScript,
	sources,
};

struct PBXSourceTree {
	enum Type {
		absolute,
		buildProductsDir,
		custom,
		developerDir,
		group,
		none,
		sdkRoot,
		sourceRoot,
	} type = none;

	String _custom;
};

enum class PBXProductType {
	appExtension,
	application,
	bundle,
	commandLineTool,
	driverExtension,
	dynamicLibrary,
	extensionKitExtension,
	framework,
	instrumentsPackage,
	intentsServiceExtension,
	messagesApplication,
	messagesExtension,
	metalLibrary,
	none,
	ocUnitTestBundle,
	onDemandInstallCapableApplication,
	staticFramework,
	staticLibrary,
	stickerPack,
	systemExtension,
	tvExtension,
	uiTestBundle,
	unitTestBundle,
	watch2App,
	watch2AppContainer,
	watch2Extension,
	watchApp,
	watchExtension,
	xcFramework,
	xcodeExtension,
	xpcService
};

struct XCodeExport;

struct PBXAggregateTarget;
struct PBXContainerItemProxy;
struct PBXFileElement;
struct PBXFileReference;
struct PBXFileSystemSynchronizedBuildFileExceptionSet;
struct PBXFileSystemSynchronizedRootGroup;
struct PBXFrameworksBuildPhase;
struct PBXGroup;
struct PBXHeadersBuildPhase;
struct PBXTarget;
struct PBXNativeTarget;
struct PBXProject;
struct PBXResourcesBuildPhase;
struct PBXSourcesBuildPhase;
struct PBXTargetDependency;
struct XCBuildConfiguration;
struct XCConfigurationList;
struct XCSwiftPackageProductDependency;

struct PBXObject : AllocBase {
	static Id generateId();
	static StringId getStringId(Id);

	const XCodeExport *root = nullptr;
	ISA isa;
	Id id;

	void write(const Callback<void(StringView)> &) const;

	PBXObject(const XCodeExport &r, ISA i) : root(&r), isa(i), id(PBXObject::generateId()) { }
};

struct PBXContainerItem : PBXObject {
	PBXContainerItem(const XCodeExport &r, ISA i) : PBXObject(r, i) { }
};

struct XCLocalSwiftPackageReference : PBXContainerItem {
	String name;
	String relativePath;
};

struct XCRemoteSwiftPackageReference : PBXContainerItem {
	struct VersionRequirement {
		enum {
			Branch,
			Exact,
			Range,
			Revision,
			UpToNextMajorVersion,
			UpToNextMinorVersion,
		} type;
		String value1;
		String value2;
	};

	String name;
	String repositoryURL;
	VersionRequirement versionRequirement;
};

struct XCSwiftPackageProductDependency : PBXContainerItem {
	XCRemoteSwiftPackageReference *package;
	String productName;
};

struct PBXBuildFile final : PBXObject {
	static const PBXBuildFile *create(XCodeExport &, const Callback<void(PBXBuildFile *)> &);

	static void write(const CallbackStream &, const PBXBuildFile &);

	const PBXFileElement *file = nullptr;
	String platformFilter;
	Vector<String> platformFilters;
	const XCSwiftPackageProductDependency *product = nullptr;
	Map<String, Value> settings;

	PBXBuildFile(const XCodeExport &r) : PBXObject(r, ISA::PBXBuildFile) { }
};

struct StringValue {
	StringView value;
};

struct DataValue {
	const Value &value;
	uint32_t indent = 3;
};

struct ObjectRef {
	const PBXObject *object;
};

template <typename Value>
struct Line {
	StringView name;
	const Value &value;
	uint32_t indent = 2;
};

template <typename Value>
struct Array {
	StringView name;
	const Vector<Value> &value;
	uint32_t indent = 2;
};

struct StringArray {
	StringView name;
	const Vector<String> &value;
	uint32_t indent = 2;
};

template <typename Value>
struct RefArray {
	StringView name;
	const Vector<Value> &value;
	uint32_t indent = 2;
};

struct ValueMap {
	StringView name;
	const Map<String, Value> &value;
	uint32_t indent = 2;
};

struct StringMap {
	StringView name;
	const Map<String, String> &value;
	uint32_t indent = 2;
};

const CallbackStream &operator<<(const CallbackStream &, const StringValue &);
const CallbackStream &operator<<(const CallbackStream &, const DataValue &);
const CallbackStream &operator<<(const CallbackStream &, const ObjectRef &);
const CallbackStream &operator<<(const CallbackStream &, const PBXProductType &);
const CallbackStream &operator<<(const CallbackStream &, const PBXSourceTree &);

template <typename Value>
inline const CallbackStream &operator<<(const CallbackStream &cb, const Line<Value> &val) {
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << val.name << " = " << val.value << ";\n";
	return cb;
}

template <typename Value>
inline const CallbackStream &operator<<(const CallbackStream &cb, const Array<Value> &val) {
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << val.name << " = (\n";
	for (auto &it : val.value) {
		for (uint32_t i = 0; i <= val.indent; ++i) { cb << '\t'; }
		cb << it << ",\n";
	}
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << ");\n";
	return cb;
}

inline const CallbackStream &operator<<(const CallbackStream &cb, const StringArray &val) {
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << val.name << " = (\n";
	for (auto &it : val.value) {
		for (uint32_t i = 0; i <= val.indent; ++i) { cb << '\t'; }
		cb << StringValue{it} << ",\n";
	}
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << ");\n";
	return cb;
}

template <typename Value>
inline const CallbackStream &operator<<(const CallbackStream &cb, const RefArray<Value> &val) {
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << val.name << " = (\n";
	for (auto &it : val.value) {
		for (uint32_t i = 0; i <= val.indent; ++i) { cb << '\t'; }
		cb << ObjectRef{it} << ",\n";
	}
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << ");\n";
	return cb;
}

inline const CallbackStream &operator<<(const CallbackStream &cb, const ValueMap &val) {
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << val.name << " = {\n";
	for (auto &it : val.value) {
		for (uint32_t i = 0; i <= val.indent; ++i) { cb << '\t'; }
		cb << it.first << " = " << DataValue{it.second, val.indent + 1} << ";\n";
	}
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << "};\n";
	return cb;
}

inline const CallbackStream &operator<<(const CallbackStream &cb, const StringMap &val) {
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << val.name << " = {\n";
	for (auto &it : val.value) {
		for (uint32_t i = 0; i <= val.indent; ++i) { cb << '\t'; }
		cb << it.first << " = " << StringValue{it.second} << ";\n";
	}
	for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
	cb << "};\n";
	return cb;
}

} // namespace stappler::makefile::xcode

#endif /* CORE_MAKEFILE_XCODE_SPPBXOBJECT_H_ */
