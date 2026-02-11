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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_NEW_H_
#define CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_NEW_H_

#include <sprt/runtime/int.h>

namespace std {

enum class align_val_t : sprt::size_t {
};

struct nothrow_t {
	explicit nothrow_t() = default;
};

extern nothrow_t const nothrow;

} // namespace std


[[nodiscard]]
__declspec(allocator) void *__cdecl operator new(size_t _Size);

[[nodiscard]]
__declspec(allocator) void *__cdecl operator new(size_t _Size, ::std::nothrow_t const &) noexcept;

[[nodiscard]]
__declspec(allocator) void *__cdecl operator new[](size_t _Size);

[[nodiscard]]
__declspec(allocator) void *__cdecl operator new[](size_t _Size, ::std::nothrow_t const &) noexcept;

void __cdecl operator delete(void *_Block) noexcept;

void __cdecl operator delete(void *_Block, ::std::nothrow_t const &) noexcept;

void __cdecl operator delete[](void *_Block) noexcept;

void __cdecl operator delete[](void *_Block, ::std::nothrow_t const &) noexcept;

void __cdecl operator delete(void *_Block, size_t _Size) noexcept;

void __cdecl operator delete[](void *_Block, size_t _Size) noexcept;

[[nodiscard]]
__declspec(allocator) void *__cdecl operator new(size_t _Size, ::std::align_val_t _Al);

[[nodiscard]]
__declspec(allocator) void *__cdecl operator new(size_t _Size, ::std::align_val_t _Al,
		::std::nothrow_t const &) noexcept;


[[nodiscard]]
__declspec(allocator) void *__cdecl operator new[](size_t _Size, ::std::align_val_t _Al);

[[nodiscard]]
__declspec(allocator) void *__cdecl operator new[](size_t _Size, ::std::align_val_t _Al,
		::std::nothrow_t const &) noexcept;

void __cdecl operator delete(void *_Block, ::std::align_val_t _Al) noexcept;

void __cdecl operator delete(void *_Block, ::std::align_val_t _Al,
		::std::nothrow_t const &) noexcept;

void __cdecl operator delete[](void *_Block, ::std::align_val_t _Al) noexcept;

void __cdecl operator delete[](void *_Block, ::std::align_val_t _Al,
		::std::nothrow_t const &) noexcept;

void __cdecl operator delete(void *_Block, size_t _Size, ::std::align_val_t _Al) noexcept;

void __cdecl operator delete[](void *_Block, size_t _Size, ::std::align_val_t _Al) noexcept;

[[nodiscard]]
constexpr inline void *__cdecl operator new(size_t _Size, void *_Where) noexcept {
	(void)_Size;
	return _Where;
}

inline void __cdecl operator delete(void *, void *) noexcept { return; }

[[nodiscard]]
inline void *__cdecl operator new[](size_t _Size, void *_Where) noexcept {
	(void)_Size;
	return _Where;
}

inline void __cdecl operator delete[](void *, void *) noexcept { }

#endif // CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_NEW_H_
