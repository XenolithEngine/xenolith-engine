/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TYPES_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TYPES_H_

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/mem/string.h>
#include <sprt/runtime/mem/vector.h>
#include <sprt/runtime/mem/set.h>
#include <sprt/runtime/mem/map.h>
#include <sprt/runtime/mem/function.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/stream.h>

namespace sprt::window {

using String = memory::dynstring;

template <typename Type>
using Vector = memory::dynvector<Type>;

template <typename Type>
using Set = memory::dynset<Type>;

template <typename Key, typename Value>
using Map = memory::dynmap<Key, Value>;

template <typename Type>
using Function = memory::dynfunction<Type>;

template <typename Type>
using Callback = callback<Type>;

using CallbackStream = callback<void(StringView)>;

using Bytes = memory::dynbytes;

template <typename... Args>
static inline String toString(Args &&...args) {
	return StreamTraits<char>::toString(sprt::forward<Args>(args)...);
}


} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TYPES_H_
