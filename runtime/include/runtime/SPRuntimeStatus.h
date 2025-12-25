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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMESTATUS_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMESTATUS_H_

#include "SPRuntimeInt.h"
#include "SPRuntimeString.h"
#include "SPRuntimeCallback.h"

namespace sprt::status {

constexpr int STATUS_ERRNO_OFFSET = 0xFFFF;
constexpr int STATUS_GENERIC_OFFSET = 0x1'FFFF;
constexpr int STATUS_GAPI_OFFSET = 0x2'FFFF;

// WinAPI error space
constexpr int STATUS_WINAPI_OFFSET = 0x100'FFFF;
constexpr int STATUS_END_OFFSET = 0x200'FFFF;

constexpr inline int ERRNO_ERROR_NUMBER(int __errno) { return -STATUS_ERRNO_OFFSET - __errno; }

constexpr inline int GENERIC_ERROR_NUMBER(int __errno) { return -STATUS_GENERIC_OFFSET - __errno; }

constexpr inline int GAPI_ERROR_NUMBER(int __errno) { return -STATUS_GAPI_OFFSET - __errno; }

constexpr inline int WINAPI_ERROR_NUMBER(int __errno) { return -STATUS_WINAPI_OFFSET - __errno; }

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
	ErrorNotPermitted =			status::ERRNO_ERROR_NUMBER(1), // EPERM, VK_ERROR_NOT_PERMITTED_KHR
	ErrorNotFound =				status::ERRNO_ERROR_NUMBER(2), // ENOENT
	ErrorNoSuchProcess  =		status::ERRNO_ERROR_NUMBER(3), // ESRCH
	ErrorInterrupted  =			status::ERRNO_ERROR_NUMBER(4), // ESRCH
	ErrorTooManyObjects =		status::ERRNO_ERROR_NUMBER(7), // E2BIG, VK_ERROR_TOO_MANY_OBJECTS
	ErrorAgain =				status::ERRNO_ERROR_NUMBER(11), // EAGAIN
	ErrorOutOfHostMemory =		status::ERRNO_ERROR_NUMBER(12), // ENOMEM, VK_ERROR_OUT_OF_HOST_MEMORY
	ErrorBusy =					status::ERRNO_ERROR_NUMBER(16), // EBUSY
	ErrorFileExists =			status::ERRNO_ERROR_NUMBER(17), // EEXIST
	ErrorIncompatibleDevice =	status::ERRNO_ERROR_NUMBER(18), // EXDEV, VK_ERROR_INCOMPATIBLE_DRIVER
	ErrorInvalidArguemnt =		status::ERRNO_ERROR_NUMBER(22), // EINVAL, VK_ERROR_INITIALIZATION_FAILED
	ErrorOutOfDeviceMemory =	status::ERRNO_ERROR_NUMBER(28), // ENOSPC, VK_ERROR_OUT_OF_DEVICE_MEMORY
	ErrorNotImplemented =		status::ERRNO_ERROR_NUMBER(38), // ENOSYS
	ErrorTimerExpired =			status::ERRNO_ERROR_NUMBER(62), // ETIME; when it's not an error - return Suspended
	ErrorNotSupported =			status::ERRNO_ERROR_NUMBER(95), // ENOTSUP, VK_ERROR_FORMAT_NOT_SUPPORTED
	ErrorBufferOverflow =		status::ERRNO_ERROR_NUMBER(105), // ENOBUFS
	ErrorAlreadyPerformed =		status::ERRNO_ERROR_NUMBER(114), // EALREADY
	ErrorInProgress =			status::ERRNO_ERROR_NUMBER(115), // EINPROGRESS
	ErrorCancelled =			status::ERRNO_ERROR_NUMBER(125), // ECANCELED, VK_ERROR_OUT_OF_DATE_KHR
	ErrorDeviceLost =			status::ERRNO_ERROR_NUMBER(130), // EOWNERDEAD, VK_ERROR_DEVICE_LOST

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
	return toInt(st) <= -STATUS_WINAPI_OFFSET && toInt(st) > -STATUS_END_OFFSET;
}

constexpr inline int toErrno(Status st) {
	return isErrno(st) ? -toInt(st) - STATUS_ERRNO_OFFSET : 0;
}

constexpr inline int toGeneric(Status st) {
	return isGeneric(st) ? -toInt(st) - STATUS_GENERIC_OFFSET : 0;
}

constexpr inline int toGApi(Status st) { return isGApi(st) ? -toInt(st) - STATUS_GAPI_OFFSET : 0; }

constexpr inline int toWinApi(Status st) {
	return isWinApi(st) ? -toInt(st) - STATUS_WINAPI_OFFSET : 0;
}

constexpr inline Status errnoToStatus(int _errno) { return Status(-STATUS_ERRNO_OFFSET - _errno); }

constexpr inline Status lastErrorToStatus(int _GetLastErrorResult) {
	return Status(-STATUS_WINAPI_OFFSET - _GetLastErrorResult);
}

SPRT_API StringView getStatusName(Status status);

SPRT_API void getStatusDescription(Status st, const callback<void(StringView)> &cb);

} // namespace sprt::status

namespace sprt {

using status::Status;

}

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMESTATUS_H_
