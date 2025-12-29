/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_CORE_UTILS_SPSHAREDMODULE_H_
#define CORE_CORE_UTILS_SPSHAREDMODULE_H_

#include "SPCore.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler {

enum class SharedModuleFlags : uint32_t {
	None = 0,

	// This flag allows to define multiple SharedModules with the same name
	// All symbols from modules will be available, but lookup will be slower,
	// then from non-extensible module
	// All SharedModules, that shares same name, should define this flag
	Extensible = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(SharedModuleFlags)

// Used by stappler-abi to detect actual shared object type
// Stappler API only defines SharedModule with _typeId = 1,
// but ABI can define other types, that accessable with
// abi::open in runtime
struct SharedVirtualObject {
	uintptr_t _typeId = 0;
};

struct SP_PUBLIC SharedSymbol final {
	const char *name = nullptr;
	const void *ptr = nullptr;
	const std::type_info *type = nullptr;

	template <typename T>
	SharedSymbol(const char *n, T *t)
	: name(n), ptr(reinterpret_cast<const void *>(t)), type(&typeid(std::remove_cv_t<T>)) { }

	SharedSymbol(const char *n, const void *p) : name(n), ptr(p) { }
};

class SP_PUBLIC SharedModule final : public SharedVirtualObject {
public:
	static constexpr uintptr_t TypeId = 1;
	static constexpr uint32_t VersionLatest = maxOf<uint32_t>();

	// Enumarate all defined module names
	static void enumerateModules(void *userdata, void (*)(void *userdata, const char *name));

	static const SharedModule *openModule(const char *module, uint32_t version);

	static const void *acquireSymbol(const char *module, uint32_t version, const char *symbol,
			const sprt::source_location & = __SPRT_LOCATION);
	static const void *acquireSymbol(const char *module, uint32_t version, const char *symbol,
			const std::type_info &, const sprt::source_location & = __SPRT_LOCATION);

	template <typename T = const void *>
	static auto acquireTypedSymbol(const char *module, uint32_t version, const char *symbol,
			const sprt::source_location &loc = __SPRT_LOCATION) {
		return reinterpret_cast<T>(const_cast<void *>(acquireSymbol(module, version, symbol,
				typeid(std::remove_pointer_t<typename std::remove_cv<T>::type>), loc)));
	}

	static bool enumerateSymbols(const char *module, uint32_t version, void *userdata,
			void (*)(void *userdata, const char *name, const void *symbol));


	static const SharedModule *openModule(const char *module);

	static const void *acquireSymbol(const char *module, const char *symbol,
			const sprt::source_location & = __SPRT_LOCATION);
	static const void *acquireSymbol(const char *module, const char *symbol, const std::type_info &,
			const sprt::source_location & = __SPRT_LOCATION);

	template <typename T = const void *>
	static auto acquireTypedSymbol(const char *module, const char *symbol,
			const sprt::source_location &loc = __SPRT_LOCATION) {
		return reinterpret_cast<T>(const_cast<void *>(acquireSymbol(module, symbol,
				typeid(std::remove_pointer_t<typename std::remove_cv<T>::type>), loc)));
	}

	static bool enumerateSymbols(const char *module, void *userdata,
			void (*)(void *userdata, const char *name, const void *symbol));

	SharedModule(const char *, SharedSymbol *, size_t count,
			SharedModuleFlags = SharedModuleFlags::None);
	~SharedModule();

	uint32_t getVersion() const { return _version; }

	const void *acquireSymbol(const char *symbol,
			const sprt::source_location & = __SPRT_LOCATION) const;
	const void *acquireSymbol(const char *symbol, const std::type_info &,
			const sprt::source_location & = __SPRT_LOCATION) const;

	template <typename T = const void *>
	auto acquireTypedSymbol(const char *symbol,
			const sprt::source_location &loc = __SPRT_LOCATION) const {
		return reinterpret_cast<T>(const_cast<void *>(acquireSymbol(symbol,
				typeid(std::remove_pointer_t<typename std::remove_cv<T>::type>), loc)));
	}

private:
	friend struct SharedModuleManager;

	const char *_name = nullptr;
	SharedSymbol *_symbols = nullptr;
	size_t _symbolsCount = 0;
	SharedModuleFlags _flags = SharedModuleFlags::None;
	SharedModule *_next = nullptr;
	uint32_t _version = 0;
};

// shortcut for single-symbol extension
class SP_PUBLIC SharedExtension final {
public:
	template <typename T>
	SharedExtension(const char *moduleName, const char *symbolName, T *s)
	: _symbol(symbolName, s), _module(moduleName, &_symbol, 1, SharedModuleFlags::Extensible) { }

protected:
	SharedSymbol _symbol;
	SharedModule _module;
};

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* CORE_CORE_UTILS_SPSHAREDMODULE_H_ */
