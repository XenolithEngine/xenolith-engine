/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

// <system_error> conformance for the sprt-backed STL vs the host libstdc++.
//
// NOTE: the *numeric* value of an errc (== the platform errno) and the *text* of
// error_category::message() are platform-specific, so this test never prints them.
// It exercises only portable, platform-independent facts: errc<->error_code/
// error_condition equivalence, category identity, the enum-trait tags, and the
// system_error exception's code(). Those must be identical on glibc, the sprt Linux
// build and the sprt Windows build.

#include <stdio.h>

#include <system_error>
#include <type_traits>

namespace sprt::test {

namespace {

// enum-trait tags (compile time)
static_assert(std::is_error_condition_enum<std::errc>::value);
static_assert(!std::is_error_code_enum<std::errc>::value);
static_assert(std::is_error_condition_enum_v<std::errc>);

void probe(const char *label, std::errc e) {
	std::error_code ec = std::make_error_code(e);
	std::error_condition cond = std::make_error_condition(e);
	printf("%s: bool=%d self=%d cross=%d gencat=%d cond_self=%d defcond=%d\n", label,
			(int) (bool) ec, // nonzero
			(int) (ec == e), // error_code == errc
			(int) (ec == cond), // error_code == error_condition (equivalent)
			(int) (ec.category() == std::generic_category()), // make_error_code -> generic
			(int) (cond == e), // error_condition == errc
			(int) (ec.default_error_condition() == e)); // maps back to the same errc
}

} // namespace

void performSystemErrorTest() {
	// default-constructed error_code: no error, system category
	std::error_code def;
	printf("default: bool=%d value0=%d syscat=%d name=%s\n", (int) (bool) def,
			(int) (def.value() == 0), (int) (def.category() == std::system_category()),
			def.category().name());

	// category names are stable spellings across implementations
	printf("cat: generic=%s system=%s eq=%d ne=%d\n", std::generic_category().name(),
			std::system_category().name(),
			(int) (std::generic_category() == std::generic_category()),
			(int) (std::generic_category() != std::system_category()));

	// a spread of errc values through make_error_code / make_error_condition
	probe("einval", std::errc::invalid_argument);
	probe("enoent", std::errc::no_such_file_or_directory);
	probe("enomem", std::errc::not_enough_memory);
	probe("erange", std::errc::result_out_of_range);
	probe("eacces", std::errc::permission_denied);

	// distinct errc values do not compare equal
	std::error_code a = std::make_error_code(std::errc::invalid_argument);
	std::error_code b = std::make_error_code(std::errc::io_error);
	printf("distinct: eq=%d ne=%d neErrc=%d\n", (int) (a == b), (int) (a != b),
			(int) (a != std::errc::io_error));

	// errc is an error_CONDITION enum (not a code enum): it assigns to
	// error_condition. Exercise assignment-from-enum and clear().
	std::error_condition asg;
	asg = std::errc::permission_denied;
	printf("assign: is=%d ", (int) (asg == std::errc::permission_denied));
	asg.clear();
	printf("cleared=%d gencat=%d\n", (int) (!asg),
			(int) (asg.category() == std::generic_category()));

	// system_error derives from runtime_error and carries its code(). Constructed (not
	// thrown — this suite builds with -fno-exceptions); RTTI is available. The what()
	// *text* is platform-specific (strerror), so only its non-nullness is checked.
	static_assert(std::is_base_of_v<std::runtime_error, std::system_error>);
	static_assert(std::is_base_of_v<std::exception, std::system_error>);
	std::system_error se(std::make_error_code(std::errc::invalid_argument), "ctx");
	printf("exc: code=%d isRuntime=%d whatNonNull=%d\n",
			(int) (se.code() == std::errc::invalid_argument),
			(int) (dynamic_cast<const std::runtime_error *>(&se) != nullptr),
			(int) (se.what() != nullptr));

	// a plain std::runtime_error round-trips its message through __libcpp_refstring
	std::runtime_error re("boom");
	std::runtime_error re2(re); // copy shares the refcounted message
	printf("runtime_error: what=%s copy=%s\n", re.what(), re2.what());

	// error_condition constructed implicitly from errc
	std::error_condition ic = std::errc::not_a_directory;
	printf("implicit_cond: is=%d bool=%d\n", (int) (ic == std::errc::not_a_directory),
			(int) (bool) ic);
}

} // namespace sprt::test
