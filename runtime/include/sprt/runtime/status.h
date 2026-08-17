/**
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_STATUS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_STATUS_H_

#include <sprt/runtime/enum.h>
#include <sprt/runtime/string.h>
#include <sprt/runtime/callback.h>
#include <sprt/runtime/math.h>
#include <sprt/cxx/__functional/invoke.h>
#include <sprt/cxx/__algorithm/minmax.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/runtime/detail/errno_canonical.h>

namespace sprt::status {

constexpr int STATUS_ERRNO_OFFSET = 0xFFFF;
constexpr int STATUS_GENERIC_OFFSET = 0x1'FFFF;
constexpr int STATUS_GAPI_OFFSET = 0x2'FFFF;

// WinAPI error space
constexpr int STATUS_WINAPI_OFFSET = 0x100'FFFF;

// IDN/UTS-46 error space. Appended after WinAPI so the existing section bounds keep
// their values; only isWinApi()'s upper bound moves from END to IDN.
constexpr int STATUS_IDN_OFFSET = 0x200'FFFF;
constexpr int STATUS_END_OFFSET = 0x201'FFFF;

constexpr inline int ERRNO_ERROR_NUMBER(int __errno) { return -STATUS_ERRNO_OFFSET - __errno; }

constexpr inline int GENERIC_ERROR_NUMBER(int __errno) { return -STATUS_GENERIC_OFFSET - __errno; }

constexpr inline int GAPI_ERROR_NUMBER(int __errno) { return -STATUS_GAPI_OFFSET - __errno; }

constexpr inline int WINAPI_ERROR_NUMBER(int __errno) { return -STATUS_WINAPI_OFFSET - __errno; }

constexpr inline int IDN_ERROR_NUMBER(int __idn) { return -STATUS_IDN_OFFSET - __idn; }

/* Canonical errno numbers used by the Status enumerators below.
 *
 * These are literals on purpose: writing ERRNO_ERROR_NUMBER(EAGAIN) would bake the *native*
 * errno of the platform being compiled for into the enumerator, and EAGAIN is 11 on Linux,
 * 35 on Darwin and 11-but-not-EWOULDBLOCK on Windows - the same error would then be a
 * different Status number per OS. The canonical numbering is the Linux/asm-generic one;
 * detail/errno_canonical.h converts native <-> canonical at the boundary.
 *
 * The static_asserts below pin these to the platform list on the platforms that define the
 * canonical numbering, so the table cannot silently drift.
 */
constexpr int32_t CANON_EPERM = 1;
constexpr int32_t CANON_ENOENT = 2;
constexpr int32_t CANON_ESRCH = 3;
constexpr int32_t CANON_EINTR = 4;
constexpr int32_t CANON_E2BIG = 7;
constexpr int32_t CANON_EAGAIN = 11;
constexpr int32_t CANON_ENOMEM = 12;
constexpr int32_t CANON_EBUSY = 16;
constexpr int32_t CANON_EEXIST = 17;
constexpr int32_t CANON_EXDEV = 18;
constexpr int32_t CANON_EINVAL = 22;
constexpr int32_t CANON_ENOSPC = 28;
constexpr int32_t CANON_EDEADLK = 35;
constexpr int32_t CANON_ENOSYS = 38;
constexpr int32_t CANON_ETIME = 62;
constexpr int32_t CANON_ENOTSUP = 95;
constexpr int32_t CANON_ENOBUFS = 105;
constexpr int32_t CANON_ETIMEDOUT = 110;
constexpr int32_t CANON_EALREADY = 114;
constexpr int32_t CANON_EINPROGRESS = 115;
constexpr int32_t CANON_ECANCELED = 125;
constexpr int32_t CANON_EOWNERDEAD = 130;
constexpr int32_t CANON_ENOTRECOVERABLE = 131;

#if !SPRT_APPLE && !SPRT_WINDOWS && !SPRT_HOSTED_RTOS
// Linux, Android and wasm carry the canonical numbering natively. The RTOS
// targets are excluded: NuttX carries most asm-generic values but splits
// ENOTSUP/EOPNOTSUPP and lacks ETIME, and Embox numbers errno its own way
// (cross/embox_sprt/errno.h), so the canonical-equality pin holds on neither.
static_assert(CANON_EPERM == EPERM && CANON_ENOENT == ENOENT && CANON_ESRCH == ESRCH
				&& CANON_EINTR == EINTR && CANON_E2BIG == E2BIG && CANON_EAGAIN == EAGAIN
				&& CANON_ENOMEM == ENOMEM && CANON_EBUSY == EBUSY && CANON_EEXIST == EEXIST
				&& CANON_EXDEV == EXDEV && CANON_EINVAL == EINVAL && CANON_ENOSPC == ENOSPC
				&& CANON_EDEADLK == EDEADLK && CANON_ENOSYS == ENOSYS && CANON_ETIME == ETIME
				&& CANON_ENOTSUP == ENOTSUP && CANON_ENOBUFS == ENOBUFS
				&& CANON_ETIMEDOUT == ETIMEDOUT && CANON_EALREADY == EALREADY
				&& CANON_EINPROGRESS == EINPROGRESS && CANON_ECANCELED == ECANCELED
				&& CANON_EOWNERDEAD == EOWNERDEAD && CANON_ENOTRECOVERABLE == ENOTRECOVERABLE,
		"Canonical errno table drifted from the platform errno list");
#endif

// The named errno statuses must round-trip through the platform tables on every platform.
static_assert(errnoToCanonical(canonicalToErrno(CANON_EAGAIN)) == CANON_EAGAIN
				&& errnoToCanonical(canonicalToErrno(CANON_EDEADLK)) == CANON_EDEADLK
				&& errnoToCanonical(canonicalToErrno(CANON_ENOTSUP)) == CANON_ENOTSUP
				&& errnoToCanonical(canonicalToErrno(CANON_ETIMEDOUT)) == CANON_ETIMEDOUT
				&& errnoToCanonical(canonicalToErrno(CANON_EOWNERDEAD)) == CANON_EOWNERDEAD,
		"errno canonicalization is not round-trippable");

// clang-format off
enum class Status : int32_t {
	// general return values
	Ok = 0,
	Declined = -1, // For refusal without an error
	Done = -2,
	Suspended = -3,
	Pending = -4,
	Timeout = -5,
	Propagate = -6, // Ask for next possible event processor

	// Vulkan support codes
	EventSet = -7, // VK_EVENT_SET
	EventReset = -8, // VK_EVENT_RESET
	Incomplete = -9, // VK_INCOMPLETE
	Suboptimal = -10, // VK_SUBOPTIMAL_KHR
	ThreadIdle = -11, // VK_THREAD_IDLE_KHR
	ThreadDone = -12, // VK_THREAD_DONE_KHR
	OperationDeferred = -13, // VK_OPERATION_DEFERRED_KHR
	OperationNotDeferred = -14, // VK_OPERATION_NOT_DEFERRED_KHR

	// general errors
	// This errors matched their errno codes, but can occurs in any subsystem
	ErrorNumber =				status::ERRNO_ERROR_NUMBER(0),
	ErrorNotPermitted =			status::ERRNO_ERROR_NUMBER(status::CANON_EPERM), // EPERM, VK_ERROR_NOT_PERMITTED_KHR
	ErrorNotFound =				status::ERRNO_ERROR_NUMBER(status::CANON_ENOENT), // ENOENT
	ErrorNoSuchProcess  =		status::ERRNO_ERROR_NUMBER(status::CANON_ESRCH), // ESRCH
	ErrorInterrupted  =			status::ERRNO_ERROR_NUMBER(status::CANON_EINTR), // EINTR
	ErrorTooManyObjects =		status::ERRNO_ERROR_NUMBER(status::CANON_E2BIG), // E2BIG, VK_ERROR_TOO_MANY_OBJECTS
	ErrorAgain =				status::ERRNO_ERROR_NUMBER(status::CANON_EAGAIN), // EAGAIN
	ErrorOutOfHostMemory =		status::ERRNO_ERROR_NUMBER(status::CANON_ENOMEM), // ENOMEM, VK_ERROR_OUT_OF_HOST_MEMORY
	ErrorBusy =					status::ERRNO_ERROR_NUMBER(status::CANON_EBUSY), // EBUSY
	ErrorFileExists =			status::ERRNO_ERROR_NUMBER(status::CANON_EEXIST), // EEXIST
	ErrorIncompatibleDevice =	status::ERRNO_ERROR_NUMBER(status::CANON_EXDEV), // EXDEV, VK_ERROR_INCOMPATIBLE_DRIVER
	ErrorInvalidArguemnt =		status::ERRNO_ERROR_NUMBER(status::CANON_EINVAL), // EINVAL, VK_ERROR_INITIALIZATION_FAILED
	ErrorOutOfDeviceMemory =	status::ERRNO_ERROR_NUMBER(status::CANON_ENOSPC), // ENOSPC, VK_ERROR_OUT_OF_DEVICE_MEMORY
	ErrorDeadLock =				status::ERRNO_ERROR_NUMBER(status::CANON_EDEADLK), // EDEADLK
	ErrorNotImplemented =		status::ERRNO_ERROR_NUMBER(status::CANON_ENOSYS), // ENOSYS
	ErrorTimerExpired =			status::ERRNO_ERROR_NUMBER(status::CANON_ETIME), // ETIME; when it's not an error - return Suspended
	ErrorNotSupported =			status::ERRNO_ERROR_NUMBER(status::CANON_ENOTSUP), // ENOTSUP, VK_ERROR_FORMAT_NOT_SUPPORTED
	ErrorBufferOverflow =		status::ERRNO_ERROR_NUMBER(status::CANON_ENOBUFS), // ENOBUFS
	ErrorTimeout =				status::ERRNO_ERROR_NUMBER(status::CANON_ETIMEDOUT), // ETIMEDOUT
	ErrorAlreadyPerformed =		status::ERRNO_ERROR_NUMBER(status::CANON_EALREADY), // EALREADY
	ErrorInProgress =			status::ERRNO_ERROR_NUMBER(status::CANON_EINPROGRESS), // EINPROGRESS
	ErrorCancelled =			status::ERRNO_ERROR_NUMBER(status::CANON_ECANCELED), // ECANCELED, VK_ERROR_OUT_OF_DATE_KHR
	ErrorNotRecoverable =		status::ERRNO_ERROR_NUMBER(status::CANON_ENOTRECOVERABLE), // ENOTRECOVERABLE
	ErrorDeviceLost =			status::ERRNO_ERROR_NUMBER(status::CANON_EOWNERDEAD), // EOWNERDEAD, VK_ERROR_DEVICE_LOST

	// Generic errors, can occurs in any subsystem
	ErrorMemoryMapFailed =		status::GENERIC_ERROR_NUMBER(1), // VK_ERROR_MEMORY_MAP_FAILED

	// Graphic-API specific errors
	ErrorLayerNotPresent =			status::GAPI_ERROR_NUMBER(1),
	ErrorExtensionNotPresent =		status::GAPI_ERROR_NUMBER(2),
	ErrorFeatureNotPresent =		status::GAPI_ERROR_NUMBER(3),
	ErrorFragmentedPool =			status::GAPI_ERROR_NUMBER(4),
	ErrorOutOfPoolMemory =			status::GAPI_ERROR_NUMBER(5),
	ErrorInvalidExternalHandle =	status::GAPI_ERROR_NUMBER(6),
	ErrorFragmentation =			status::GAPI_ERROR_NUMBER(7),
	ErrorInvalidCaptureAddress =	status::GAPI_ERROR_NUMBER(8),
	ErrorPipelineCompileRequired =	status::GAPI_ERROR_NUMBER(9),
	ErrorSurfaceLost =				status::GAPI_ERROR_NUMBER(10),
	ErrorNativeWindowInUse =		status::GAPI_ERROR_NUMBER(11),
	ErrorIncompatibleDisplay =		status::GAPI_ERROR_NUMBER(12),
	ErrorValidationFailed =			status::GAPI_ERROR_NUMBER(13),
	ErrorInvalidShader =			status::GAPI_ERROR_NUMBER(14),
	ErrorInvalidDrmFormat =			status::GAPI_ERROR_NUMBER(15),
	ErrorFullscreenLost =			status::GAPI_ERROR_NUMBER(16),

	// IDN (UTS-46) errors: one enumerator per rule the standard can reject a name by.
	//
	// UTS-46 naturally produces a SET of violated rules, while a Status is a single value,
	// so sprt::idn collapses the set with a fixed priority order (idn::statusFromErrors()
	// in utils/idn.h documents and implements it). The order below IS that priority, most
	// specific first - keep the two in sync, a silent reorder changes what callers see.
	ErrorIdnPunycode =				status::IDN_ERROR_NUMBER(1),
	ErrorIdnInvalidAceLabel =		status::IDN_ERROR_NUMBER(2),
	ErrorIdnLabelHasDot =			status::IDN_ERROR_NUMBER(3),
	ErrorIdnEmptyLabel =			status::IDN_ERROR_NUMBER(4),
	ErrorIdnDisallowed =			status::IDN_ERROR_NUMBER(5),
	ErrorIdnBidi =					status::IDN_ERROR_NUMBER(6),
	ErrorIdnContextJ =				status::IDN_ERROR_NUMBER(7),
	ErrorIdnContextOPunctuation =	status::IDN_ERROR_NUMBER(8),
	ErrorIdnContextODigits =		status::IDN_ERROR_NUMBER(9),
	ErrorIdnLeadingCombiningMark =	status::IDN_ERROR_NUMBER(10),
	ErrorIdnLeadingHyphen =			status::IDN_ERROR_NUMBER(11),
	ErrorIdnTrailingHyphen =		status::IDN_ERROR_NUMBER(12),
	ErrorIdnHyphen34 =				status::IDN_ERROR_NUMBER(13),
	ErrorIdnLabelTooLong =			status::IDN_ERROR_NUMBER(14),
	ErrorIdnDomainNameTooLong =		status::IDN_ERROR_NUMBER(15),

	ErrorUnknown = ErrorNumber,
};
// clang-format on

static constexpr bool isSuccessful(Status st) {
	switch (st) {
	case Status::Ok:
	case Status::Done:
	case Status::Suspended: return true; break;
	default: break;
	}
	return false;
}

constexpr inline int isApplicationDefined(Status st) { return toInt(st) < 0; }

constexpr inline int isOperational(Status st) {
	return toInt(st) <= 0 && toInt(st) > STATUS_ERRNO_OFFSET;
}

constexpr inline int isErrno(Status st) {
	return toInt(st) <= -STATUS_ERRNO_OFFSET && toInt(st) > -STATUS_GENERIC_OFFSET;
}

constexpr inline int isGeneric(Status st) {
	return toInt(st) <= -STATUS_GENERIC_OFFSET && toInt(st) > -STATUS_GAPI_OFFSET;
}

constexpr inline int isGApi(Status st) {
	return toInt(st) <= -STATUS_GAPI_OFFSET && toInt(st) > -STATUS_WINAPI_OFFSET;
}

constexpr inline int isWinApi(Status st) {
	return toInt(st) <= -STATUS_WINAPI_OFFSET && toInt(st) > -STATUS_IDN_OFFSET;
}

constexpr inline int isIdn(Status st) {
	return toInt(st) <= -STATUS_IDN_OFFSET && toInt(st) > -STATUS_END_OFFSET;
}

// The errno carried by a Status is the canonical (portable) number - see
// detail/errno_canonical.h. Translate it back into what this platform's libc uses.
// Returns 0 when the status is not an errno status, or when the canonical code has no
// counterpart on this platform.
constexpr inline int toErrno(Status st) {
	return isErrno(st) ? canonicalToErrno(-toInt(st) - STATUS_ERRNO_OFFSET) : 0;
}

// The raw canonical code, without translating it back to the local libc.
constexpr inline int toCanonicalErrno(Status st) {
	return isErrno(st) ? -toInt(st) - STATUS_ERRNO_OFFSET : 0;
}

constexpr inline int toGeneric(Status st) {
	return isGeneric(st) ? -toInt(st) - STATUS_GENERIC_OFFSET : 0;
}

constexpr inline int toGApi(Status st) { return isGApi(st) ? -toInt(st) - STATUS_GAPI_OFFSET : 0; }

constexpr inline int toWinApi(Status st) {
	return isWinApi(st) ? -toInt(st) - STATUS_WINAPI_OFFSET : 0;
}

constexpr inline int toIdn(Status st) { return isIdn(st) ? -toInt(st) - STATUS_IDN_OFFSET : 0; }

// Takes a NATIVE errno of the platform being compiled for and canonicalizes it, so the same
// error yields the same Status everywhere (see detail/errno_canonical.h).
constexpr inline Status errnoToStatus(int _errno) {
	return Status(-STATUS_ERRNO_OFFSET - errnoToCanonical(_errno));
}

// For codes that are already canonical (e.g. received from another platform).
constexpr inline Status canonicalErrnoToStatus(int _errno) {
	return Status(-STATUS_ERRNO_OFFSET - _errno);
}

constexpr inline Status lastErrorToStatus(int _GetLastErrorResult) {
	return Status(-STATUS_WINAPI_OFFSET - _GetLastErrorResult);
}

/*
	Defined in stringview.h

SPRT_API StringView getStatusName(Status status);

SPRT_API void getStatusDescription(Status st, const callback<void(StringView)> &cb);
*/

} // namespace sprt::status

namespace sprt {

using status::Status;

/** Result is a helper class for functions, that returns some result
 * or fails and returns nothing. It defines several mechanisms to handle
 * error state:
 * - get with default value in case of failure (`get`)
 * - grab value into object, provided by reference, if value is valid (`grab`)
 * - call a callback with value, if it's valid (`unwrap`)
 */
template <typename T>
struct Result {
	Status status = Status::ErrorUnknown;
	T result;

	static Result<T> error() { return Result(); }
	static Result<T> error(Status st) { return Result{st}; }

	Result(T &&t, Status s = Status::Ok) noexcept : status(s), result(move(t)) { }
	Result(const T &t, Status s = Status::Ok) noexcept : status(s), result(t) { }

	Result() noexcept = default;
	Result(const Result &) noexcept = default;
	Result(Result &&) noexcept = default;
	Result &operator=(const Result &) noexcept = default;
	Result &operator=(Result &&) noexcept = default;

	bool valid() const { return isSuccessful(status); }

	explicit operator bool() const { return valid(); }

	template <typename Callback>
	bool unwrap(const Callback &cb) const {
		static_assert(is_invocable_v<Callback, const T &>, "Invalid callback type");
		if (isSuccessful(status)) {
			cb(result);
			return true;
		}
		return false;
	}

	bool grab(T &value) {
		if (isSuccessful(status)) {
			value = move(result);
			return true;
		}
		return false;
	}

	const T &get() const { return result; }
	const T &get(const T &def) const { return (isSuccessful(status)) ? result : def; }
};


// Type, that use negative Status values on failure, or positive int values on success
template <typename T = int32_t>
struct StatusValue {
	static_assert(sizeof(T) == sizeof(Status) && (is_integral_v<T> or is_enum_v<T>));

	static T max() {
		return T(sprt::min(uint32_t(Max<T>), uint32_t(Max<underlying_type_t<Status>>)));
	}

	union {
		Status status = Status::Ok;
		T value;
	};

	StatusValue(Status s) : status(s) { }
	StatusValue(const T &v) : value(v) {
		sprt_passert(value >= 0 && uint32_t(value) <= uint32_t(Max<underlying_type_t<Status>>),
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

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_STATUS_H_
