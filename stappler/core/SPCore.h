/**
 Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_SPCORE_H_
#define STAPPLER_CORE_SPCORE_H_

/* Stappler Core header: common includes and functions
 * Enables SDK-specific syntactic sugar and functions, should be first included header
 * (already true it you include any of SDK's functional headers)
 *
 * To enable precompiled headers, use SPCommon.h as a first include in translation unit instead
 */

#include "stappler-buildconfig.h" // IWYU pragma: keep

namespace stappler::buildconfig {

// appconfig stores values from project configuration, default values is:
// - bundle id
// - default application name
// - application version
constexpr auto MODULE_APPCONFIG_NAME = "appconfig";

// libstappler can use this module name to interact with running application itself
// this module should define application-specific symbols for runtime intialization
// (like, default scene intialization for Xenolith or default ServerComponent)
constexpr auto MODULE_APPCOMMON_NAME = "appcommon";

} // namespace stappler::buildconfig

// Link sprt with STL
#define __SPRT_USE_STL 1

#include "detail/SPPlatformInit.h"
#include <sprt/runtime/notnull.h>
#include <assert.h>

// From C++ standard library:
#include <type_traits> // IWYU pragma: keep
#include <typeindex> // IWYU pragma: keep
#include <iterator> // IWYU pragma: keep
#include <limits> // IWYU pragma: keep
#include <utility> // IWYU pragma: keep
#include <iterator> // IWYU pragma: keep
#include <algorithm> // IWYU pragma: keep
#include <tuple> // IWYU pragma: keep
#include <cmath> // IWYU pragma: keep
#include <locale> // IWYU pragma: keep

#include <tuple> // IWYU pragma: keep
#include <string> // IWYU pragma: keep
#include <vector> // IWYU pragma: keep
#include <functional> // IWYU pragma: keep
#include <sstream> // IWYU pragma: keep
#include <fstream> // IWYU pragma: keep
#include <map> // IWYU pragma: keep
#include <set> // IWYU pragma: keep
#include <unordered_map> // IWYU pragma: keep
#include <unordered_set> // IWYU pragma: keep
#include <bitset> // IWYU pragma: keep
#include <forward_list> // IWYU pragma: keep
#include <array> // IWYU pragma: keep
#include <deque> // IWYU pragma: keep
#include <bit> // IWYU pragma: keep
#include <bitset> // IWYU pragma: keep

#include <istream> // IWYU pragma: keep
#include <ostream> // IWYU pragma: keep
#include <iostream> // IWYU pragma: keep
#include <iomanip> // IWYU pragma: keep
#include <mutex> // IWYU pragma: keep
#include <shared_mutex> // IWYU pragma: keep
#include <atomic> // IWYU pragma: keep
#include <future> // IWYU pragma: keep
#include <thread> // IWYU pragma: keep
#include <condition_variable> // IWYU pragma: keep
#include <initializer_list> // IWYU pragma: keep
#include <optional> // IWYU pragma: keep
#include <variant> // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep
#include <compare> // IWYU pragma: keep

#include "detail/SPHash.h" // IWYU pragma: keep
#include "detail/SPMath.h" // IWYU pragma: keep
#include "detail/SPValueWrapper.h" // IWYU pragma: keep
#include "detail/SPEnum.h" // IWYU pragma: keep
#include "detail/SPPtr.h" // IWYU pragma: keep

#include "detail/SPPlatformCleanup.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED sp = STAPPLER_VERSIONIZED_NAMESPACE;

namespace STAPPLER_VERSIONIZED stappler {

inline constexpr uint32_t SP_MAKE_API_VERSION(uint32_t variant, uint32_t major, uint32_t minor,
		uint32_t patch) {
	return (uint32_t(variant) << 29) | (uint32_t(major) << 22) | (uint32_t(minor) << 12)
			| uint32_t(patch & 0b1111'1111'1111);
}

using sprt::move;
using sprt::move_unsafe;

template <typename T>
using NotNull = sprt::NotNull<T>;

// Stappler requires only C++17, backport for std::bit_cast
// Previously referenced as as `reinterpretValue`;
template <class To, class From>
std::enable_if_t< sizeof(To) == sizeof(From)
				&& std::is_trivially_copyable_v<From> && std::is_trivially_copyable_v<To>,
		To>
bit_cast(const From &src) noexcept {
	static_assert(std::is_trivially_constructible_v<To>,
        "This implementation additionally requires "
        "destination type to be trivially constructible");

	To dst;
	::memcpy(&dst, &src, sizeof(To));
	return dst;
}

using std::forward;
using std::min;
using std::max;

using nullptr_t = std::nullptr_t;

/*
 *   User Defined literals
 *
 *   Functions:
 *   - _len / _length     - string literal length
 *   - _GiB / _MiB / _KiB - binary size numbers
 *   - _c8 / _c16         - convert integer literal to character
 */

// string length (useful for comparation: memcmp(str, "Test", "Test"_len) )
constexpr size_t operator""_length(const char *str, size_t len) { return len; }
constexpr size_t operator""_length(const char16_t *str, size_t len) { return len; }
constexpr size_t operator""_len(const char *str, size_t len) { return len; }
constexpr size_t operator""_len(const char16_t *str, size_t len) { return len; }

constexpr unsigned long long int operator""_GiB(unsigned long long int val) {
	return val * 1'024 * 1'024 * 1'024;
}
constexpr unsigned long long int operator""_MiB(unsigned long long int val) {
	return val * 1'024 * 1'024;
}
constexpr unsigned long long int operator""_KiB(unsigned long long int val) { return val * 1'024; }

constexpr char16_t operator""_c16(unsigned long long int val) { return (char16_t)val; }
constexpr char operator""_c8(unsigned long long int val) { return (char)val; }

template <typename... Args>
inline constexpr auto pair(Args &&...args) -> decltype(std::make_pair(sp::forward<Args>(args)...)) {
	return std::make_pair(sp::forward<Args>(args)...);
}

template <typename T, typename V>
using Pair = std::pair<T, V>;

template <typename T>
using InitializerList = std::initializer_list<T>;

/** Functions for enum flags */
template <typename T>
bool hasFlag(T mask, T flag) {
	return (mask & flag) != T(0);
}

template <typename T>
bool hasFlagAll(T mask, T flag) {
	return (mask & flag) == T(flag);
}

/*
 * 		Invoker/CallTest macro
 * Tests when some method of class C is defined
 */

#define InvokerCallTest_MakeCallTest(Name, Success, Failure) \
	private: \
		template <typename C> static Success CallTest_ ## Name( typeof(&C::Name) ); \
		template <typename C> static Failure CallTest_ ## Name(...); \
	public: \
		static constexpr bool Name = sizeof(CallTest_ ## Name<T>(0)) == sizeof(success);


/*
 * Initialization API
 *
 * call `initialize` when main application thread is started
 * call `terminate`` when main thread is stopped
 *
 * initialize returns false when application should not try to run,
 * and set appropriate resultCode to return from application's main
 *
 * argc and argv should be original vaules from 'main' or (0, nullptr)
 *
 * if initialize return true - app can be run as usual
 *
 * or use perform_main from SPMemory.h when possible
*/

SP_PUBLIC bool initialize(int argc, const char *argv[], int &resultCode);
SP_PUBLIC void terminate();

// `init` will be called in FIFO order, `term` - in reverse (LIFO) order
// if `initialize` was already called, `init` will be called in place
SP_PUBLIC bool addInitializer(void *ptr, NotNull<void(void *)> init, NotNull<void(void *)> term);

/*
	'perform_main' is intended to be called at the entry point into program execution.
	It correctly initializes and deinitializes all stappler systems,
*/
template <typename Callback>
inline int perform_main(int argc, const char *argv[], const Callback &cb) {
	int resultCode = 0;
	if (initialize(argc, argv, resultCode)) {
		auto ret = cb();

		terminate();
		return ret;
	} else {
		return resultCode;
	}
}

/*
 * SDK Version API
 */

SP_PUBLIC const char *getStapplerVersionString();

SP_PUBLIC uint32_t getStapplerVersionIndex();

SP_PUBLIC uint32_t getStapplerVersionVariant();

// API version number
SP_PUBLIC uint32_t getStapplerVersionApi();

// Build revision version number
SP_PUBLIC uint32_t getStapplerVersionRev();

// Build number
SP_PUBLIC uint32_t getStapplerVersionBuild();

/*
 * Appconfig API
 * 
 * Appconfig uses SharedModule appconfig, that should be created by build system when building the application
 *
 * Use `getVersionDescription<Interface>(getAppconfigVersionIndex())` for a version string
 */

// Returns NULL when appconfig is not defined
SP_PUBLIC const char *getAppconfigBundleName();

// Returns NULL when appconfig is not defined
SP_PUBLIC const char *getAppconfigAppName();

SP_PUBLIC uint32_t getAppconfigVersionIndex();

SP_PUBLIC uint32_t getAppconfigVersionVariant();

SP_PUBLIC uint32_t getAppconfigVersionApi();

SP_PUBLIC uint32_t getAppconfigVersionRev();

SP_PUBLIC uint32_t getAppconfigVersionBuild();

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_SPCORE_H_ */
