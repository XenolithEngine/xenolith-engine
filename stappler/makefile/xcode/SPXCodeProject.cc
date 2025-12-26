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

#include "SPXCodeProject.h"
#include "xcode/SPPBXObject.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

XCodeExport::XCodeExport() { pool = memory::pool::create(memory::pool::acquire()); }

void XCodeExport::write(const Callback<void(StringView)> &cb) {
	memory::context ctx(pool);

	auto exportList = [&](SpanView<ISA> isaList, StringView sectionName) {
		cb << "/* Begin " << sectionName << " section */\n";

		for (auto &obj : objects) {
			if (std::find_if(isaList.begin(), isaList.end(), [&](ISA isa) {
				return obj->isa == isa;
			}) != isaList.end()) {
				obj->write(cb);
			}
		}

		cb << "/* End " << sectionName << " section */\n";
	};

	cb << "// !$*UTF8*$!\n";
	cb << "{\n";
	cb << "\tarchiveVersion = " << archiveVersion << ";\n";
	cb << "\tclasses = {\n";
	cb << "\t};\n";
	cb << "\tobjectVersion = " << objectVersion << ";\n";
	cb << "\tobjects = {\n";

	exportList(Vector<ISA>{ISA::PBXAggregateTarget}, "PBXAggregateTarget");
	exportList(Vector<ISA>{ISA::PBXBuildFile}, "PBXBuildFile");
	exportList(Vector<ISA>{ISA::PBXContainerItemProxy}, "PBXContainerItemProxy");
	exportList(Vector<ISA>{ISA::PBXFileReference}, "PBXFileReference");
	exportList(Vector<ISA>{ISA::PBXFileSystemSynchronizedBuildFileExceptionSet},
			"PBXFileSystemSynchronizedBuildFileExceptionSet");
	exportList(Vector<ISA>{ISA::PBXFileSystemSynchronizedRootGroup},
			"PBXFileSystemSynchronizedRootGroup");
	exportList(Vector<ISA>{ISA::PBXFrameworksBuildPhase}, "PBXFrameworksBuildPhase");
	exportList(Vector<ISA>{ISA::PBXGroup}, "PBXGroup");
	exportList(Vector<ISA>{ISA::PBXHeadersBuildPhase}, "PBXHeadersBuildPhase");
	exportList(Vector<ISA>{ISA::PBXNativeTarget}, "PBXNativeTarget");
	exportList(Vector<ISA>{ISA::PBXProject}, "PBXProject");
	exportList(Vector<ISA>{ISA::PBXResourcesBuildPhase}, "PBXResourcesBuildPhase");
	exportList(Vector<ISA>{ISA::PBXSourcesBuildPhase}, "PBXSourcesBuildPhase");
	exportList(Vector<ISA>{ISA::PBXTargetDependency}, "PBXTargetDependency");
	exportList(Vector<ISA>{ISA::XCBuildConfiguration}, "XCBuildConfiguration");
	exportList(Vector<ISA>{ISA::XCConfigurationList}, "XCConfigurationList");

	cb << "\t};\n";
	if (root) {
		cb << Line{"rootObject", ObjectRef{root}, 1};
	}
	cb << "}\n";
}

} // namespace stappler::makefile::xcode
