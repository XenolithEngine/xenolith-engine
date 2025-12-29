/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_CORE_UTILS_SPSTATUS_H_
#define CORE_CORE_UTILS_SPSTATUS_H_

#include "SPCore.h"
#include <sprt/runtime/status.h>
#include <type_traits>

namespace STAPPLER_VERSIONIZED stappler {

using sprt::Status;

// Type, that use negative Status values on failure, or positive int values on success
template <typename T = int32_t>
struct StatusValue {
	static_assert(sizeof(T) == sizeof(Status) && (std::is_integral_v<T> or std::is_enum_v<T>));

	static T max() {
		return T(std::min(uint32_t(maxOf<T>()), uint32_t(maxOf<std::underlying_type_t<Status>>())));
	}

	union {
		Status status = Status::Ok;
		T value;
	};

	StatusValue(Status s) : status(s) { }
	StatusValue(const T &v) : value(v) {
		sprt_passert(value >= 0
						&& uint32_t(value) <= uint32_t(maxOf<std::underlying_type_t<Status>>()),
				"Value should be in positive range of int32_t");
	}

	Status getStatus() const {
		if (toInt(status) <= 0) {
			return status;
		} else {
			return Status::Ok;
		}
	}

	T getValue() const {
		if (toInt(status) <= 0) {
			return T(0);
		} else {
			return value;
		}
	}

	operator Status() const { return getStatus(); }

	operator T() const { return getValue(); }

	explicit operator bool() const { return toInt(status) >= 0; }
};

SP_PUBLIC std::ostream &operator<<(std::ostream &, Status);

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* CORE_CORE_UTILS_SPSTATUS_H_ */
