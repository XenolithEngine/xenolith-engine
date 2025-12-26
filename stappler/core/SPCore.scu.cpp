/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPCommon.h" // IWYU pragma: keep

#include "SPIO.cc"
#include "detail/SPMemPoolAllocator.cc"
#include "detail/SPMemPoolApr.cc"
#include "detail/SPMemPoolHash.cc"
#include "detail/SPMemPoolInterface.cc"
#include "detail/SPMemPoolPool.cc"
#include "detail/SPMemPoolUtils.cc"
#include "detail/SPMemAlloc.cc"
#include "detail/SPMemRbtree.cc"
#include "detail/SPMemUserData.cc"
#include "SPMemUuid.cc"
#include "SPBase64.cc"
#include "SPCharGroup.cc"
#include "SPSha2.cc"
#include "SPGost3411-2012.cc"
#include "SPString.cc"
#include "SPUnicode.cc"
#include "SPHtmlParser.cc"
#include "SPLog.cc"
#include "SPRef.cc"
#include "SPTime.cc"
#include "SPDso.cc"
#include "SPSharedModule.cc"

#include "platform/SPCoreRuntime.cc"

#include "platform/SPCore-win32.cc"
#include "platform/SPCore-darwin.cc"

#include "SPUrl.cc"
#include "SPValid.cc"
#include "SPCommandLineParser.cc"
#include "SPStatus.cc"
#include "SPLocaleInfo.cc"

#include "SPIdn.cc"
#include "SPIdnTld.cc"

#include "SPMetastring.h"
#include "SPRuntimePlatform.h"

#if LINUX
#ifdef MODULE_STAPPLER_ABI
#include "linux/SPAbiLinuxElf.h"
#endif
#endif

#include <list>

#define STAPPLER_VERSION_VARIANT 0

namespace STAPPLER_VERSIONIZED stappler {

struct Initializer {
	void *userdata;
	void (*init)(void *);
	void (*term)(void *);
};

struct InitializerManager {
	static InitializerManager &get();

	InitializerManager &operator=(const InitializerManager &) = delete;

	std::mutex mutex;
	std::list<Initializer> list;
	bool initialized = false;
	memory::pool_t *pool = nullptr;
};

InitializerManager &InitializerManager::get() {
	static InitializerManager im;
	return im;
}

bool initialize(int argc, const char *argv[], int &resultCode) {
#if LINUX
#ifdef MODULE_STAPPLER_ABI
	abi::initialize(argc, argv);
	platform::s_instance = platform::i18n::getInstance();
#endif
#endif

	memory::pool::initialize();

	auto pool = memory::pool::create(memory::app_root_pool);

	memory::pool::push(pool);

	if (!sprt::initialize(resultCode)) {
		memory::pool::pop(pool, nullptr);
		return false;
	}

	auto &m = InitializerManager::get();
	std::unique_lock lock(m.mutex);
	m.initialized = true;
	m.pool = pool;

	for (auto &it : m.list) { it.init(it.userdata); }
	return true;
}

void terminate() {
	auto &m = InitializerManager::get();
	std::unique_lock lock(m.mutex);

	// terminate in reverse oder
	for (auto it = m.list.rbegin(); it != m.list.rend(); ++it) { it->term(it->userdata); }

	m.list.clear();

	sprt::terminate();

	memory::pool::pop(m.pool, nullptr);
	memory::pool::terminate();
}

bool addInitializer(void *ptr, NotNull<void(void *)> init, NotNull<void(void *)> term) {
	auto &m = InitializerManager::get();
	std::unique_lock lock(m.mutex);
	if (m.initialized) {
		init.get()(ptr);
	}
	m.list.emplace_back(Initializer{ptr, init, term});
	return !m.initialized;
}

const char *getStapplerVersionString() {
	static auto versionString = metastring::merge(
			metastring::numeric<size_t(STAPPLER_VERSION_VARIANT)>(), metastring::metastring<'.'>(),
			metastring::numeric<size_t(buildconfig::STAPPLER_VERSION_API)>(),
			metastring::metastring<'.'>(),
			metastring::numeric<size_t(buildconfig::STAPPLER_VERSION_REV)>(),
			metastring::metastring<'.'>(),
			metastring::numeric<size_t(buildconfig::STAPPLER_VERSION_BUILD)>(),
			metastring::metastring<char(0)>())
										.to_array();

	return versionString.data();
}

uint32_t getStapplerVersionIndex() {
	return SP_MAKE_API_VERSION(STAPPLER_VERSION_VARIANT, buildconfig::STAPPLER_VERSION_API,
			buildconfig::STAPPLER_VERSION_REV, buildconfig::STAPPLER_VERSION_BUILD);
}

uint32_t getStapplerVersionVariant() { return STAPPLER_VERSION_VARIANT; }

uint32_t getStapplerVersionApi() { return buildconfig::STAPPLER_VERSION_API; }

uint32_t getStapplerVersionRev() { return buildconfig::STAPPLER_VERSION_REV; }

uint32_t getStapplerVersionBuild() { return buildconfig::STAPPLER_VERSION_BUILD; }


SP_PUBLIC const char *getAppconfigBundleName() {
	return SharedModule::acquireTypedSymbol<const char *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_BUNDLE_NAME");
}

const char *getAppconfigAppName() {
	return SharedModule::acquireTypedSymbol<const char *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_APP_NAME");
}

uint32_t getAppconfigVersionIndex() {
	return SP_MAKE_API_VERSION(getAppconfigVersionVariant(), getAppconfigVersionApi(),
			getAppconfigVersionRev(), getAppconfigVersionBuild());
}

uint32_t getAppconfigVersionVariant() {
	auto num = SharedModule::acquireTypedSymbol<const int *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_VERSION_VARIANT");
	if (num) {
		return uint32_t(*num);
	}
	return 0;
}

uint32_t getAppconfigVersionApi() {
	auto num = SharedModule::acquireTypedSymbol<const int *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_VERSION_API");
	if (num) {
		return uint32_t(*num);
	}
	return 0;
}

uint32_t getAppconfigVersionRev() {
	auto num = SharedModule::acquireTypedSymbol<const int *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_VERSION_REV");
	if (num) {
		return uint32_t(*num);
	}
	return 0;
}

uint32_t getAppconfigVersionBuild() {
	auto num = SharedModule::acquireTypedSymbol<const int *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_VERSION_BUILD");
	if (num) {
		return uint32_t(*num);
	}
	return 0;
}

} // namespace STAPPLER_VERSIONIZED stappler
