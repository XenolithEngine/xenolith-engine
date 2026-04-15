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

#include <sprt/runtime/stream.h>
#include <sprt/cxx/type_traits>

namespace sprt {

void test_trait(const char *name, bool result) {
	sprt::cout << name << ": " << (result ? "true" : "false") << "\n";
}

void performTypeTraitsTests() {
	enum class TestEnum {
		A,
		B
	};

	// Type properties
	test_trait("is_void", sprt::is_void<void>::value);
	test_trait("is_null_pointer", sprt::is_null_pointer<sprt::nullptr_t>::value);
	test_trait("is_integral", sprt::is_integral<int>::value);
	test_trait("is_floating_point", sprt::is_floating_point<float>::value);
	test_trait("is_array", sprt::is_array<int[]>::value);
	test_trait("is_pointer", sprt::is_pointer<int *>::value);
	test_trait("is_lvalue_reference", sprt::is_lvalue_reference<int &>::value);
	test_trait("is_rvalue_reference", sprt::is_rvalue_reference<int &&>::value);
	test_trait("is_member_object_pointer", sprt::is_member_object_pointer<int *>::value);
	test_trait("is_member_function_pointer", sprt::is_member_function_pointer<void(int)>::value);
	test_trait("is_enum",
			sprt::is_enum<sprt::underlying_type<TestEnum>::type>::value); // Need to define enum
	test_trait("is_union", sprt::is_union<int>::value);
	test_trait("is_class", sprt::is_class<int>::value);
	test_trait("is_function", sprt::is_function<void()>::value);

	// Type relationships
	test_trait("is_same", sprt::is_same<int, int>::value);
	test_trait("is_base_of", sprt::is_base_of<int, int>::value); // Always false (not proper usage)
	test_trait("is_convertible", sprt::is_convertible<int, int>::value);

	// Type modifications
	test_trait("remove_reference", sprt::is_same<int, sprt::remove_reference<int &>::type>::value);
	test_trait("remove_pointer", sprt::is_same<int, sprt::remove_pointer<int *>::type>::value);
	test_trait("remove_extent", sprt::is_same<int, sprt::remove_extent<int[5]>::type>::value);
	test_trait("remove_all_extents",
			sprt::is_same<int, sprt::remove_all_extents<int[3][4]>::type>::value);
	test_trait("add_pointer", sprt::is_same<int *, sprt::add_pointer<int>::type>::value);
	test_trait("remove_cv", sprt::is_same<int, sprt::remove_cv<const volatile int>::type>::value);
	test_trait("add_cv", sprt::is_same<const volatile int, sprt::add_cv<int>::type>::value);
	test_trait("add_lvalue_reference",
			sprt::is_same<int &, sprt::add_lvalue_reference<int>::type>::value);
	test_trait("add_rvalue_reference",
			sprt::is_same<int &&, sprt::add_rvalue_reference<int>::type>::value);

	// Type categories
	test_trait("is_arithmetic", sprt::is_arithmetic<int>::value);
	test_trait("is_fundamental", sprt::is_fundamental<int>::value);
	test_trait("is_compound", sprt::is_compound<int>::value);
	test_trait("is_reference", sprt::is_reference<int &>::value);
	test_trait("is_pointer", sprt::is_pointer<int *>::value);
	test_trait("is_object", sprt::is_object<int>::value);
	test_trait("is_scalar", sprt::is_scalar<int>::value);

	// Type properties (C++17)
	test_trait("is_aggregate", sprt::is_aggregate<int>::value);
	test_trait("is_trivially_copyable", sprt::is_trivially_copyable<int>::value);
	test_trait("is_standard_layout", sprt::is_standard_layout<int>::value);
	test_trait("is_trivial", sprt::is_trivial<int>::value);
	test_trait("is_trivially_constructible", sprt::is_trivially_constructible<int>::value);
	test_trait("is_trivially_assignable", sprt::is_trivially_assignable<int &, int>::value);
	test_trait("is_trivially_destructible", sprt::is_trivially_destructible<int>::value);

	// Type properties (C++20)
	test_trait("is_nothrow_assignable", sprt::is_nothrow_assignable<int &, int>::value);
	test_trait("is_nothrow_constructible", sprt::is_nothrow_constructible<int>::value);
	test_trait("is_nothrow_default_constructible",
			sprt::is_nothrow_default_constructible<int>::value);
}

} // namespace sprt
