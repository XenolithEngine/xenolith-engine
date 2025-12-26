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

#include "SPCore.h"
#include "SPPlatform.h"
#include "SPPBXProject.h"
#include "SPPBXObject.h"
#include "SPPBXTarget.h"
#include "SPPBXFile.h"
#include "SPXCodeProject.h"
#include <unistd.h>

namespace STAPPLER_VERSIONIZED stappler::makefile::xcode {

struct Globalidentifier {
	int8_t user; // encoded username
	int8_t pid; // encoded current pid #
	int16_t _random; // encoded random value
	int32_t _time; // encoded time value
	int8_t zero; // zero
	int8_t host_shift; // encoded, shifted hostid
	int8_t host_h; // high byte of lower bytes of hostid
	int8_t host_l; // low byte of lower bytes of hostid
} __attribute__((packed));

static uint64_t packedValueForChar = 0x1f1f'1f1f'1f1f'1f1f;
static int8_t hasInitialized = false;
static int32_t lasttime = 0;
static int16_t firstseq = 0;
static struct Globalidentifier gid = {};

static const char *getCurrentUsername() { return getlogin(); };

// convenience method for ROL
static int16_t rotl16(int16_t value, unsigned int count) {
	const unsigned int mask = (CHAR_BIT * sizeof(value) - 1);
	count &= mask;
	return (value << count) | (value >> ((-count) & mask));
}

// the original function (same name) can be found in DevToolsSupport.framework
static int8_t *TSGenerateUniqueGlobalID(int8_t *buff) {

	/* Note: the "gid" variable is a reference to the global identifier struct */

	int8_t *buffer = buff;
	if (hasInitialized == 0) {
		hasInitialized = 1;

		pid_t current_pid = getpid();
		gid.pid = (current_pid & 0xff);

		const char *username = getCurrentUsername();
		uint64_t index = 0;
		char letter = username[index];
		int32_t counter = 0;
		int8_t output = 0;

		// perfom the encoding on the username
		do {
			int8_t value = 0x1f;
			letter = username[index];
			if (letter >= 0) {
				value = (int8_t)((letter & 0xff) + packedValueForChar);
			}
			if (counter != 0) {
				value = (value & 0xff) << counter >> 0x8 | (value & 0xff) << counter;
			}
			counter = (counter + 0x5) & 0x7;
			output = output ^ value;
			index++;
		} while (letter != '\0');
		// store the encoded byte
		gid.user = output;

		int32_t host_id = 0;
		int32_t temp = static_cast<int32_t>(
				gethostid()); // this is used even though it is deprecated on OS X
		if (temp != -1) {
			host_id = temp;
		}
		// generate the random seed
		auto time_seed = platform::nanoclock();
		srandom(static_cast<unsigned>((current_pid & 0xff) << 0x10 | host_id)
				^ static_cast<unsigned>(time_seed));
		if (host_id == 0) {
			host_id = static_cast<int32_t>(random());
		}

		gid.zero = 0;
		gid.host_shift = (host_id >> 0x10) & 0xff; // low byte after shift
		gid.host_h = (host_id & 0xff00) >> 0x8; // high byte
		gid.host_l = (host_id & 0xff); // low byte
		gid._random = random();
	}

	// increment the random value
	int16_t random_value = gid._random;
	random_value += 1;
	gid._random = random_value;

	// encode the time value and check to make sure we don't have conflicts with a previous add
	// (eg when two adds happen in close enough timeframe)
	int64_t time_val = platform::nanoclock();
	int32_t encoded_time = 0;
	if (time_val > lasttime) {
		firstseq = random_value;
		lasttime = static_cast<int32_t>(time_val);
	} else {
		if (firstseq == random_value) {
			lasttime += 1;
		}
	}
	encoded_time = lasttime;
	// now swap byte ordering
	gid._time = byteorder::bswap32(encoded_time);

	// rotate the random value for output
	int16_t random_rol = rotl16(random_value, 0x8);
	gid._random = random_rol;

	// copy to passed buffer, this will always be 12 bytes
	memcpy(buffer, &gid, sizeof(Globalidentifier));

	// roll the rolled value again for storing
	gid._random = rotl16(random_rol, 0x8);

	// return passed buffer
	return buffer;
}

Id PBXObject::generateId() {
	Id ret;
	TSGenerateUniqueGlobalID((int8_t *)ret.data());
	return ret;
}

StringId PBXObject::getStringId(Id id) {
	StringId ret;
	base16::encode(ret.data(), ret.size(), CoderSource(id), true);
	return ret;
}

void PBXObject::write(const Callback<void(StringView)> &cb) const {
	switch (isa) {
	case ISA::PBXProject: PBXProject::write(cb, static_cast<const PBXProject &>(*this)); break;
	case ISA::XCBuildConfiguration:
		XCBuildConfiguration::write(cb, static_cast<const XCBuildConfiguration &>(*this));
		break;
	case ISA::PBXFileReference:
		PBXFileReference::write(cb, static_cast<const PBXFileReference &>(*this));
		break;
	case ISA::XCConfigurationList:
		XCConfigurationList::write(cb, static_cast<const XCConfigurationList &>(*this));
		break;
	case ISA::PBXGroup: PBXGroup::write(cb, static_cast<const PBXGroup &>(*this)); break;
	case ISA::PBXNativeTarget:
		PBXNativeTarget::write(cb, static_cast<const PBXNativeTarget &>(*this));
		break;
	case ISA::PBXCopyFilesBuildPhase:
		PBXCopyFilesBuildPhase::write(cb, static_cast<const PBXCopyFilesBuildPhase &>(*this));
		break;
	case ISA::PBXFrameworksBuildPhase:
		PBXFrameworksBuildPhase::write(cb, static_cast<const PBXFrameworksBuildPhase &>(*this));
		break;
	case ISA::PBXHeadersBuildPhase:
		PBXHeadersBuildPhase::write(cb, static_cast<const PBXHeadersBuildPhase &>(*this));
		break;
	case ISA::PBXResourcesBuildPhase:
		PBXResourcesBuildPhase::write(cb, static_cast<const PBXResourcesBuildPhase &>(*this));
		break;
	case ISA::PBXRezBuildPhase:
		PBXRezBuildPhase::write(cb, static_cast<const PBXRezBuildPhase &>(*this));
		break;
	case ISA::PBXShellScriptBuildPhase:
		PBXShellScriptBuildPhase::write(cb, static_cast<const PBXShellScriptBuildPhase &>(*this));
		break;
	case ISA::PBXSourcesBuildPhase:
		PBXSourcesBuildPhase::write(cb, static_cast<const PBXSourcesBuildPhase &>(*this));
		break;
	case ISA::PBXFileSystemSynchronizedRootGroup:
		PBXFileSystemSynchronizedRootGroup::write(cb,
				static_cast<const PBXFileSystemSynchronizedRootGroup &>(*this));
		break;
	case ISA::PBXFileSystemSynchronizedBuildFileExceptionSet:
		PBXFileSystemSynchronizedBuildFileExceptionSet::write(cb,
				static_cast<const PBXFileSystemSynchronizedBuildFileExceptionSet &>(*this));
		break;
	case ISA::PBXBuildFile:
		PBXBuildFile::write(cb, static_cast<const PBXBuildFile &>(*this));
		break;
	default: abort(); break;
	}
}

const PBXBuildFile *PBXBuildFile::create(XCodeExport &xctx,
		const Callback<void(PBXBuildFile *)> &cb) {
	memory::context ctx(xctx.pool);

	auto obj = new (xctx.pool) PBXBuildFile(xctx);

	cb(obj);

	xctx.objects.emplace_back(obj);
	return obj;
}

void PBXBuildFile::write(const CallbackStream &cb, const PBXBuildFile &file) {
	cb << '\t' << getStringId(file.id) << " = {isa = PBXBuildFile;";

	if (file.file) {
		cb << " fileRef = " << getStringId(file.file->id) << ";";
	}

	cb << " };\n";
}

const CallbackStream &operator<<(const CallbackStream &cb, const StringValue &str) {
	if (str.value.empty()) {
		cb << "\"\"";
		return cb;
	}

	auto tmp = str.value;
	tmp.skipChars<StringView::Alphanumeric>();
	if (tmp.is('.')) {
		++tmp;
		tmp.skipChars<StringView::Alphanumeric>();
	}

	if (!tmp.empty()) {
		cb << '"' << str.value << '"';
	} else {
		cb << str.value;
	}
	return cb;
}

const CallbackStream &operator<<(const CallbackStream &cb, const DataValue &val) {
	switch (val.value.getType()) {
	case Value::Type::EMPTY:
	case Value::Type::NONE:
	case Value::Type::BYTESTRING:
	case Value::Type::DICTIONARY: cb << "\"\""; break;
	case Value::Type::INTEGER: cb << val.value.asInteger(); break;
	case Value::Type::DOUBLE: cb << val.value.asDouble(); break;
	case Value::Type::BOOLEAN: cb << (val.value.asBool() ? "YES" : "NO"); break;
	case Value::Type::CHARSTRING: cb << StringValue{val.value.getString()}; break;
	case Value::Type::ARRAY:
		cb << "(\n";
		for (auto &iit : val.value.asArray()) {
			for (uint32_t i = 0; i <= val.indent; ++i) { cb << '\t'; }
			cb << DataValue{iit, val.indent + 1};
			cb << ",\n";
		}
		for (uint32_t i = 0; i < val.indent; ++i) { cb << '\t'; }
		cb << ")";
		break;
	}
	return cb;
}

const CallbackStream &operator<<(const CallbackStream &cb, const ObjectRef &obj) {
	cb << PBXObject::getStringId(obj.object->id);
	return cb;
}

const CallbackStream &operator<<(const CallbackStream &cb, const PBXProductType &productType) {
	cb << "\"";
	switch (productType) {
	case PBXProductType::none: break;
	case PBXProductType::application: cb << "com.apple.product-type.application"; break;
	case PBXProductType::framework: cb << "com.apple.product-type.framework"; break;
	case PBXProductType::staticFramework: cb << "com.apple.product-type.framework.static"; break;
	case PBXProductType::xcFramework: cb << "com.apple.product-type.xcframework"; break;
	case PBXProductType::dynamicLibrary: cb << "com.apple.product-type.library.dynamic"; break;
	case PBXProductType::staticLibrary: cb << "com.apple.product-type.library.static"; break;
	case PBXProductType::bundle: cb << "com.apple.product-type.bundle"; break;
	case PBXProductType::unitTestBundle: cb << "com.apple.product-type.bundle.unit-test"; break;
	case PBXProductType::uiTestBundle: cb << "com.apple.product-type.bundle.ui-testing"; break;
	case PBXProductType::appExtension: cb << "com.apple.product-type.app-extension"; break;
	case PBXProductType::extensionKitExtension:
		cb << "com.apple.product-type.extensionkit-extension";
		break;
	case PBXProductType::commandLineTool: cb << "com.apple.product-type.tool"; break;
	case PBXProductType::watchApp: cb << "com.apple.product-type.application.watchapp"; break;
	case PBXProductType::watch2App: cb << "com.apple.product-type.application.watchapp2"; break;
	case PBXProductType::watch2AppContainer:
		cb << "com.apple.product-type.application.watchapp2-container";
		break;
	case PBXProductType::watchExtension: cb << "com.apple.product-type.watchkit-extension"; break;
	case PBXProductType::watch2Extension: cb << "com.apple.product-type.watchkit2-extension"; break;
	case PBXProductType::tvExtension: cb << "com.apple.product-type.tv-app-extension"; break;
	case PBXProductType::messagesApplication:
		cb << "com.apple.product-type.application.messages";
		break;
	case PBXProductType::messagesExtension:
		cb << "com.apple.product-type.app-extension.messages";
		break;
	case PBXProductType::stickerPack:
		cb << "com.apple.product-type.app-extension.messages-sticker-pack";
		break;
	case PBXProductType::xpcService: cb << "com.apple.product-type.xpc-service"; break;
	case PBXProductType::ocUnitTestBundle: cb << "com.apple.product-type.bundle.ocunit-test"; break;
	case PBXProductType::xcodeExtension: cb << "com.apple.product-type.xcode-extension"; break;
	case PBXProductType::instrumentsPackage:
		cb << "com.apple.product-type.instruments-package";
		break;
	case PBXProductType::intentsServiceExtension:
		cb << "com.apple.product-type.app-extension.intents-service";
		break;
	case PBXProductType::onDemandInstallCapableApplication:
		cb << "com.apple.product-type.application.on-demand-install-capable";
		break;
	case PBXProductType::metalLibrary: cb << "com.apple.product-type.metal-library"; break;
	case PBXProductType::driverExtension: cb << "com.apple.product-type.driver-extension"; break;
	case PBXProductType::systemExtension: cb << "com.apple.product-type.system-extension"; break;
	}
	cb << "\"";
	return cb;
}

const CallbackStream &operator<<(const CallbackStream &cb, const PBXSourceTree &sourceTree) {
	switch (sourceTree.type) {
	case PBXSourceTree::absolute: cb << "\"<absolute>\""; break;
	case PBXSourceTree::buildProductsDir: cb << "BUILT_PRODUCTS_DIR"; break;
	case PBXSourceTree::custom: cb << StringValue{sourceTree._custom}; break;
	case PBXSourceTree::developerDir: cb << "DEVELOPER_DIR"; break;
	case PBXSourceTree::group: cb << "\"<group>\""; break;
	case PBXSourceTree::sdkRoot: cb << "SDKROOT"; break;
	case PBXSourceTree::sourceRoot: cb << "SOURCE_ROOT"; break;
	case PBXSourceTree::none: cb << "\"\""; break;
	}
	return cb;
}

} // namespace stappler::makefile::xcode
