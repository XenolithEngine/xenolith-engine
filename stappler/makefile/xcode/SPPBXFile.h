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

#ifndef CORE_MAKEFILE_XCODE_SPPBXFILE_H_
#define CORE_MAKEFILE_XCODE_SPPBXFILE_H_

#include "SPPBXObject.h"
#include "SPStringView.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

struct PBXFileSystemSynchronizedBuildFileExceptionSet : PBXObject {
	static const PBXFileSystemSynchronizedBuildFileExceptionSet *create(XCodeExport &,
			const Callback<void(PBXFileSystemSynchronizedBuildFileExceptionSet *)> &);

	static void write(const CallbackStream &, const PBXFileSystemSynchronizedBuildFileExceptionSet &);

	Map<String, String> additionalCompilerFlagsByRelativePath;
	Map<String, String> attributesByRelativePath;
	Vector<String> membershipExceptions;
	Vector<String> privateHeaders;
	Vector<String> publicHeaders;
	const PBXTarget *target = nullptr;

	PBXFileSystemSynchronizedBuildFileExceptionSet(const XCodeExport &r)
	: PBXObject(r, ISA::PBXFileSystemSynchronizedBuildFileExceptionSet) { }
};

struct PBXContainerItemProxy : PBXObject {
	struct ContainerPortal {
		enum {
			FileReference,
			Project,
			UnknownObject
		} type;

		union {
			PBXFileReference *file;
			PBXProject *project;
			PBXObject *object;
		};
	};

	enum class ProxyType {
		NativeTarget,
		Other,
		Reference
	};

	struct RemoteGlobalID {
		enum {
			StringType,
			ObjectType
		} type;

		PBXObject *object;
		String string;
	};

	ContainerPortal containerPortal;
	ProxyType proxyType;
	RemoteGlobalID remoteGlobalID;
};

struct PBXFileElement : PBXContainerItem {
	bool includeInIndex = false;
	uint32_t indentWidth = 0;
	String name;
	PBXFileElement *parent;
	String path;
	PBXSourceTree sourceTree;
	uint32_t tabWidth = 0;
	bool usesTabs = false;
	bool wrapsLines = false;

	PBXFileElement(const XCodeExport &r, ISA i) : PBXContainerItem(r, i) { }
};

struct PBXFileReference final : PBXFileElement {
	static const PBXFileReference *create(XCodeExport &,
			const Callback<void(PBXFileReference *)> &);

	static void write(const CallbackStream &, const PBXFileReference &);

	String explicitFileType;
	uint32_t fileEncoding = 0;
	String languageSpecificationIdentifier;
	String lastKnownFileType;
	uint32_t lineEnding = 0;
	String plistStructureDefinitionIdentifier;
	String xcLanguageSpecificationIdentifier;

	PBXFileReference(const XCodeExport &r) : PBXFileElement(r, ISA::PBXFileReference) { }
};

struct PBXFileSystemSynchronizedRootGroup final : PBXFileElement {
	static const PBXFileSystemSynchronizedRootGroup *create(XCodeExport &,
			const Callback<void(PBXFileSystemSynchronizedRootGroup *)> &);

	static void write(const CallbackStream &, const PBXFileSystemSynchronizedRootGroup &);

	mutable Vector<const PBXFileSystemSynchronizedBuildFileExceptionSet *> exceptions;
	Map<String, String> explicitFileTypes;
	Vector<String> explicitFolders;

	PBXFileSystemSynchronizedRootGroup(const XCodeExport &r)
	: PBXFileElement(r, ISA::PBXFileSystemSynchronizedRootGroup) { }
};

struct PBXReferenceProxy final : PBXFileElement {
	String fileType;
	PBXContainerItemProxy *remote;
};

struct PBXGroup : PBXFileElement {
	static const PBXGroup *create(XCodeExport &, const Callback<void(PBXGroup *)> &);

	static void write(const CallbackStream &, const PBXGroup &);

	Vector<const PBXFileElement *> children;

	PBXGroup(const XCodeExport &r) : PBXFileElement(r, ISA::PBXGroup) { }
};

struct PBXVariantGroup final : PBXGroup {
	static PBXVariantGroup *create();
};

struct XCVersionGroup final : PBXGroup {
	static XCVersionGroup *create();

	PBXFileReference *currentVersion;
	String versionGroupType;
};

} // namespace stappler::makefile::xcode

#endif /* CORE_MAKEFILE_XCODE_SPPBXFILE_H_ */
