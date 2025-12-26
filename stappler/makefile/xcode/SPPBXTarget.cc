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

#include "SPPBXTarget.h"
#include "SPFilepath.h"
#include "SPPBXFile.h"
#include "SPXCodeProject.h"
#include "xcode/SPPBXObject.h"

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

const XCBuildConfiguration *XCBuildConfiguration::create(XCodeExport &xctx,
		const Callback<void(XCBuildConfiguration *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) XCBuildConfiguration(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void XCBuildConfiguration::write(const Callback<void(StringView)> &cb,
		const XCBuildConfiguration &conf) {
	cb << '\t' << getStringId(conf.id) << " = {\n";

	cb << Line{"isa", "XCBuildConfiguration"};

	if (conf.baseConfiguration) {
		cb << Line{"baseConfigurationReference", ObjectRef{conf.baseConfiguration}};
	}

	cb << ValueMap{"buildSettings", conf.buildSettings};
	cb << Line{"name", StringValue{conf.name}};
	cb << "\t};\n";
}

const XCConfigurationList *XCConfigurationList::create(XCodeExport &xctx,
		const Callback<void(XCConfigurationList *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) XCConfigurationList(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void XCConfigurationList::write(const Callback<void(StringView)> &cb,
		const XCConfigurationList &conf) {
	cb << '\t' << getStringId(conf.id) << " = {\n";
	cb << Line{"isa", "XCConfigurationList"};
	cb << RefArray{"buildConfigurations", conf.buildConfigurations};
	cb << Line{"defaultConfigurationIsVisible", conf.defaultConfigurationIsVisible ? '1' : '0'};
	cb << Line{"defaultConfigurationName", StringValue{conf.defaultConfiguration->name}};
	cb << "\t};\n";
}

const PBXNativeTarget *PBXNativeTarget::create(XCodeExport &xctx,
		const Callback<void(PBXNativeTarget *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXNativeTarget(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXNativeTarget::write(const Callback<void(StringView)> &cb, const PBXNativeTarget &target) {
	cb << '\t' << getStringId(target.id) << " = {\n";

	cb << Line{"isa", "PBXNativeTarget"};

	if (target.buildConfigurationList) {
		cb << Line{"buildConfigurationList", ObjectRef{target.buildConfigurationList}};
	}

	cb << RefArray{"buildPhases", target.buildPhases};
	cb << RefArray{"buildRules", target.buildRules};
	cb << RefArray{"dependencies", target.dependencies};

	if (!target.name.empty()) {
		cb << Line{"name", StringValue{target.name}};
	}

	cb << RefArray{"packageProductDependencies", target.packageProductDependencies};

	if (target.product) {
		if (!target.product->name.empty()) {
			cb << Line{"productName", StringValue{filepath::name(target.product->name)}};
		} else if (!target.product->path.empty()) {
			cb << Line{"productName", StringValue{filepath::name(target.product->path)}};
		}
		cb << Line{"productReference", ObjectRef{target.product}};
	}

	cb << Line{"productType", target.productType};
	cb << "\t};\n";
}

} // namespace stappler::makefile::xcode
