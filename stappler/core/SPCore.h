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

#include "detail/SPPlatformInit.h"

#include <sprt/runtime/enum.h>
#include <sprt/runtime/hash.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/io_traits.h>
#include <sprt/runtime/utils/notnull.h>
#include <sprt/runtime/utils/ptr.h>
#include <sprt/runtime/utils/time.h>
#include <sprt/runtime/detail/value_wrapper.h>

#include <sprt/cxx/utility>
#include <sprt/cxx/algorithm>
#include <sprt/cxx/initializer_list>

#include <sprt/cxx/string>
#include <sprt/cxx/vector>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/unordered_set>

#include <assert.h> // IWYU pragma: keep


/** SP_DEFINE_ENUM_AS_MASK is utility to make a bitwise-mask from typed enum
 * It defines a set of overloaded operators, that allow some bitwise operations
 * on this enum class
 *
 * Type should be unsigned, and SDK code style suggests to make it sized (uint32_t, uint64_t)
 */
#define SP_DEFINE_ENUM_AS_MASK(Type) SPRT_DEFINE_ENUM_AS_MASK(Type)

/** SP_DEFINE_ENUM_AS_INCREMENTABLE adds operator++/operator-- for enumerations */
#define SP_DEFINE_ENUM_AS_INCREMENTABLE(Type, First, Last) SPRT_DEFINE_ENUM_AS_INCREMENTABLE(Type, First, Last)

namespace STAPPLER_VERSIONIZED sp = STAPPLER_VERSIONIZED_NAMESPACE;

namespace STAPPLER_VERSIONIZED stappler {

using sprt::move;
using sprt::move_unsafe;

using sprt::forward;
using sprt::min;
using sprt::max;

using sprt::uint8_t;
using sprt::uint16_t;
using sprt::uint32_t;
using sprt::uint64_t;
using sprt::uintptr_t;
using sprt::int8_t;
using sprt::int16_t;
using sprt::int32_t;
using sprt::int64_t;
using sprt::intptr_t;
using sprt::size_t;
using sprt::ssize_t;
using sprt::time_t;
using sprt::ptrdiff_t;

using sprt::nullptr_t;

using sprt::toInt;
using sprt::each;
using sprt::flags;

using sprt::progress;

using sprt::StringToNumber;

using sprt::HasMultiplication;
using sprt::ValueWrapper;

using sprt::Ptr;
using sprt::NotNull;

using sprt::Status;

namespace math = sprt::math;

namespace chars = sprt::chars;

using sprt::time::Time;
using sprt::time::TimeInterval;

using sprt::RefAlloc;
using sprt::Ref;
using sprt::Rc;
using sprt::SharedRef;
using sprt::SharedRefMode;

using sprt::BytesReader;
using sprt::StringViewBase;
using sprt::StringView;
using sprt::StringViewUtf8;
using sprt::WideStringView;
using sprt::BytesViewTemplate;
using sprt::BytesView;
using sprt::BytesViewNetwork;
using sprt::BytesViewHost;
using sprt::SpanView;

using sprt::StringComparator;
using sprt::StringCaseComparator;
using sprt::StringUnicodeComparator;
using sprt::StringUnicodeCaseComparator;

using sprt::CharGroupId;

using sprt::time::operator""_sec;
using sprt::time::operator""_msec;
using sprt::time::operator""_mksec;

template <typename T>
using Callback = sprt::callback<T>;

using CallbackStream = Callback<void(StringView)>;

template <typename A, typename B>
using Pair = sprt::pair<A, B>;

inline constexpr uint32_t SP_MAKE_API_VERSION(uint32_t variant, uint32_t major, uint32_t minor,
		uint32_t patch) {
	return (uint32_t(variant) << 29) | (uint32_t(major) << 22) | (uint32_t(minor) << 12)
			| uint32_t(patch & 0b1111'1111'1111);
}

// used for naming/hashing (like "MyTag"_tag)
constexpr sprt::uint32_t operator""_hash(const char *str, sprt::size_t len) {
	return sprt::hash32(str, sprt::uint32_t(len));
}
constexpr sprt::uint32_t operator""_tag(const char *str, sprt::size_t len) {
	return sprt::hash32(str, sprt::uint32_t(len));
}

constexpr sprt::uint64_t operator""_hash64(const char *str, sprt::size_t len) {
	return sprt::hash64(str, len);
}
constexpr sprt::uint64_t operator""_tag64(const char *str, sprt::size_t len) {
	return sprt::hash64(str, len);
}

constexpr long double operator""_to_rad(long double val) {
	return val * sprt::numbers::Pi<long double> / 180.0;
}
constexpr long double operator""_to_rad(unsigned long long int val) {
	return val * sprt::numbers::Pi<long double> / 180.0;
}

template <typename T = float>
inline constexpr auto nan() -> T {
	return sprt::NaN<T>;
}

template <typename T = float>
inline constexpr auto epsilon() -> T {
	return sprt::Epsilon<T>;
}

template <class T>
inline constexpr T maxOf() {
	return sprt::Max<T>;
}

template <class T>
inline constexpr T minOf() {
	return sprt::Min<T>;
}

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

using sprt::pair;

template <typename T>
using InitializerList = sprt::initializer_list<T>;

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
