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

#include "SPCommon.h"

#if linux
#include "linux/SPAbiLinuxElf.cc"
#include "linux/SPAbiLinuxLoader.cc"
#endif

namespace STAPPLER_VERSIONIZED stappler::abi {

static auto ERROR_NOT_FOUND = "stappler-abi: abi::open: module not found";
static auto ERROR_INVALID_MODULE = "stappler-abi: abi::sym: invalid module provided";
static auto ERROR_SYM_NOT_OUND = "stappler-abi: abi::sym: symbol not found";
static auto ERROR_NO_FOREIGN_LOADER = "stappler-abi: abi::sym: foreign loader is not available";

static bool s_hasLoader = false;

void initialize(int argc, const char *procArgv[]) {
	if (startLinuxLoader(argc, procArgv)) {
		s_hasLoader = true;
	}
}

void *open(StringView name, DsoFlags flags, const char **err) {
	if (s_hasLoader) {
		auto obj = openForeign(name, flags, err);
		if (obj) {
			return obj;
		}
	}

#ifdef STAPPLER_ABI_REQUIRED
	auto abiName = mem_std::toString("__abi__:", name);

	auto v = SharedModule::openModule(abiName.data());
	if (!v) {
		*err = ERROR_NOT_FOUND;
	}
	*err = nullptr;
	return const_cast<SharedModule *>(v);
#else
	return nullptr;
#endif
}

void close(DsoFlags flags, void *handle) {
	auto v = reinterpret_cast<SharedVirtualObject *>(handle);
	switch (v->_typeId) {
	case SharedModule::TypeId:
		// do nothing
		break;
	case ForeignDso::TypeId:
		if (s_hasLoader) {
			closeForeign(flags, static_cast<ForeignDso *>(v));
		}
		break;
	default: break;
	}
}

void *sym(void *handle, StringView name, DsoSymFlags flags, const char **err) {
	if (!handle) {
		*err = ERROR_INVALID_MODULE;
		return nullptr;
	}

	auto v = reinterpret_cast<SharedVirtualObject *>(handle);
	switch (v->_typeId) {
	case SharedModule::TypeId: {
		auto mod = static_cast<SharedModule *>(v);
		auto sym = mod->acquireSymbol(name.data());
		if (!sym) {
			*err = ERROR_SYM_NOT_OUND;
			return nullptr;
		}

		return const_cast<void *>(sym);
		break;
	}
	case ForeignDso::TypeId:
		if (s_hasLoader) {
			return symForeign(static_cast<ForeignDso *>(v), name, flags, err);
		} else {
			*err = ERROR_NO_FOREIGN_LOADER;
		}
		break;
	default: *err = ERROR_INVALID_MODULE; break;
	}
	return nullptr;
}

void *createThread(void (*cb)(thread::Thread *), thread::Thread *thread, uint32_t flags) {
	return reinterpret_cast<void *>(
			startForeignThread(reinterpret_cast<void *(*)(void *)>(cb), thread, flags));
}

void *joinThread(void *thread) { return joinForeignThread(reinterpret_cast<pthread_t>(thread)); }

void detachThread(void *thread) { detachForeignThread(reinterpret_cast<pthread_t>(thread)); }

static SharedSymbol s_abiSharedSymbols[] = {
	SharedSymbol("createThread", &createThread),
	SharedSymbol("joinThread", &joinThread),
	SharedSymbol("detachThread", &detachThread),
};

SP_USED static SharedModule s_abiSharedModule(buildconfig::MODULE_STAPPLER_ABI_NAME,
		s_abiSharedSymbols, sizeof(s_abiSharedSymbols) / sizeof(SharedSymbol));

} // namespace stappler::abi
