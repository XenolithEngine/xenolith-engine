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

#include "SPPBXFile.h"
#include "SPXCodeProject.h"
#include "xcode/SPPBXObject.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

const PBXFileSystemSynchronizedBuildFileExceptionSet *
PBXFileSystemSynchronizedBuildFileExceptionSet::create(XCodeExport &xctx,
		const Callback<void(PBXFileSystemSynchronizedBuildFileExceptionSet *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXFileSystemSynchronizedBuildFileExceptionSet(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXFileSystemSynchronizedBuildFileExceptionSet::write(const CallbackStream &cb,
		const PBXFileSystemSynchronizedBuildFileExceptionSet &set) {
	cb << '\t' << getStringId(set.id) << " = {\n";
	cb << Line{"isa", "PBXFileSystemSynchronizedBuildFileExceptionSet"};
	cb << StringArray{"membershipExceptions", set.membershipExceptions};
	cb << Line{"target", ObjectRef{set.target}};
	cb << "\t};\n";
}

const PBXFileReference *PBXFileReference::create(XCodeExport &xctx,
		const Callback<void(PBXFileReference *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXFileReference(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXFileReference::write(const Callback<void(StringView)> &cb, const PBXFileReference &ref) {
	cb << '\t' << getStringId(ref.id) << " = {isa = PBXFileReference;";

	if (!ref.explicitFileType.empty()) {
		cb << " explicitFileType = " << StringValue{ref.explicitFileType} << ";";
	}

	if (!ref.lastKnownFileType.empty()) {
		cb << " lastKnownFileType = " << StringValue{ref.lastKnownFileType} << ";";
	}

	if (!ref.includeInIndex) {
		cb << " includeInIndex = " << ref.includeInIndex << ";";
	}

	if (!ref.name.empty()) {
		cb << " name = " << StringValue{ref.name} << ";";
	}

	if (!ref.path.empty()) {
		cb << " path = " << StringValue{ref.path} << ";";
	}

	cb << " sourceTree = " << ref.sourceTree << ";";
	cb << " };\n";
}

const PBXFileSystemSynchronizedRootGroup *PBXFileSystemSynchronizedRootGroup::create(
		XCodeExport &xctx, const Callback<void(PBXFileSystemSynchronizedRootGroup *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXFileSystemSynchronizedRootGroup(xctx);

	obj->sourceTree = PBXSourceTree{PBXSourceTree::absolute};

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXFileSystemSynchronizedRootGroup::write(const CallbackStream &cb,
		const PBXFileSystemSynchronizedRootGroup &group) {
	cb << '\t' << getStringId(group.id) << " = {\n";

	cb << Line{"isa", "PBXFileSystemSynchronizedRootGroup"};

	cb << RefArray{"exceptions", group.exceptions};

	cb << StringMap{"explicitFileTypes", group.explicitFileTypes};
	cb << StringArray{"explicitFolders", group.explicitFolders};

	if (!group.name.empty()) {
		cb << Line{"name", StringValue{group.name}};
	}

	if (!group.path.empty()) {
		cb << Line{"path", StringValue{group.path}};
	}

	cb << Line{"sourceTree", group.sourceTree};
	cb << "\t};\n";
}

const PBXGroup *PBXGroup::create(XCodeExport &xctx, const Callback<void(PBXGroup *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXGroup(xctx);

	obj->sourceTree = PBXSourceTree{PBXSourceTree::group};

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXGroup::write(const CallbackStream &cb, const PBXGroup &group) {
	cb << '\t' << getStringId(group.id) << " = {\n";

	cb << Line{"isa", "PBXGroup"};

	cb << RefArray{"children", group.children};

	if (!group.name.empty()) {
		cb << Line{"name", StringValue{group.name}};
	}

	cb << Line{"sourceTree", group.sourceTree};
	cb << "\t};\n";
}


} // namespace stappler::makefile::xcode
