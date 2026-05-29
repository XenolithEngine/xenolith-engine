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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_TYPES_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_TYPES_H_

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/stream.h>
#include <sprt/runtime/utils/time.h>
#include <sprt/cxx/string>
#include <sprt/cxx/vector>
#include <sprt/cxx/set>
#include <sprt/cxx/map>
#include <sprt/cxx/unordered_set>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/function>

namespace sprt::dispatch {

using time::Time;
using time::TimeInterval;

using String = __malloc_string;

using StringStream = __malloc_stringstream;

using WideString = __malloc_u16string;

template <typename Type>
using Vector = __malloc_vector<Type>;

template <typename Type>
using Set = __malloc_set<Type>;

template <typename Key, typename Value>
using Map = __malloc_map<Key, Value>;

template <typename Type>
using HashSet = __malloc_unordered_set<Type>;

template <typename Key, typename Value>
using HashMap = __malloc_unordered_map<Key, Value>;

template <typename Type>
using Function = __malloc_function<Type>;

template <typename Type>
using Callback = callback<Type>;

using CallbackStream = callback<void(StringView)>;

using Bytes = __malloc_bytes;

template <typename... Args>
static inline String toString(Args &&...args) {
	return StreamTraits<char>::toString<String>(sprt::forward<Args>(args)...);
}

} // namespace sprt::dispatch

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_TYPES_H_
