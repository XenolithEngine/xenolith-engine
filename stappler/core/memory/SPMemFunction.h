/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_MEMORY_SPMEMFUNCTION_H_
#define STAPPLER_CORE_MEMORY_SPMEMFUNCTION_H_

#include "SPCore.h"
#include <sprt/runtime/mem/function.h>

namespace STAPPLER_VERSIONIZED stappler::memory {

using sprt::memory::function;
using sprt::callback;
using sprt::static_function;

template <typename T>
inline auto makeCallback(T &&t) ->
		typename std::enable_if<!std::is_function<T>::value && !std::is_bind_expression<T>::value,
				typename sprt::callback_traits<decltype(&T::operator())>::type>::type {
	using Type = typename sprt::callback_traits<decltype(&T::operator())>::type;

	return Type(std::forward<T>(t));
}

template <typename Sig>
inline auto makeCallback(const std::function<Sig> &fn) {
	return callback<Sig>(fn);
}

template <typename Sig>
inline auto makeCallback(const memory::function<Sig> &fn) {
	return callback<Sig>(fn);
}

} // namespace stappler::memory

#endif /* STAPPLER_CORE_MEMORY_SPMEMFUNCTION_H_ */
