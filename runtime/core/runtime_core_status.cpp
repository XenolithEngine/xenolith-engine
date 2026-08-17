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

#define __SPRT_BUILD 1

#include <sprt/runtime/status.h>
#include <sprt/runtime/stream.h>

#define STATUS_DESC_BUFFER_SIZE 512

#if SPRT_WINDOWS
#include <sprt/wrappers/windows/message_api.h>

#endif

namespace sprt::status {

StringView getStatusName(Status status) {
	switch (status) {
	case Status::Ok: return "Status::Ok"; break;
	case Status::Declined: return "Status::Declined"; break;
	case Status::Done: return "Status::Done"; break;
	case Status::Suspended: return "Status::Suspended"; break;
	case Status::Pending: return "Status::Pending"; break;
	case Status::Timeout: return "Status::Timeout"; break;
	case Status::Propagate: return "Status::Propagate"; break;
	case Status::EventSet: return "Status::EventSet"; break;
	case Status::EventReset: return "Status::EventReset"; break;
	case Status::Incomplete: return "Status::Incomplete"; break;
	case Status::Suboptimal: return "Status::Suboptimal"; break;
	case Status::ThreadIdle: return "Status::ThreadIdle"; break;
	case Status::ThreadDone: return "Status::ThreadDone"; break;
	case Status::OperationDeferred: return "Status::OperationDeferred"; break;
	case Status::OperationNotDeferred: return "Status::OperationNotDeferred"; break;

	// errors
	case Status::ErrorUnknown: return "Status::ErrorUnknown"; break;
	case Status::ErrorNotPermitted: return "Status::ErrorNotPermitted"; break;
	case Status::ErrorNotFound: return "Status::ErrorNotFound"; break;
	case Status::ErrorNoSuchProcess: return "Status::ErrorNoSuchProcess"; break;
	case Status::ErrorInterrupted: return "Status::ErrorInterrupted"; break;
	case Status::ErrorTooManyObjects: return "Status::ErrorTooManyObjects"; break;
	case Status::ErrorAgain: return "Status::ErrorAgain"; break;
	case Status::ErrorOutOfHostMemory: return "Status::ErrorOutOfHostMemory"; break;
	case Status::ErrorBusy: return "Status::ErrorBusy"; break;
	case Status::ErrorFileExists: return "Status::ErrorFileExists"; break;
	case Status::ErrorIncompatibleDevice: return "Status::ErrorIncompatibleDevice"; break;
	case Status::ErrorInvalidArguemnt: return "Status::ErrorInvalidArguemnt"; break;
	case Status::ErrorOutOfDeviceMemory: return "Status::ErrorOutOfDeviceMemory"; break;
	case Status::ErrorNotImplemented: return "Status::ErrorNotImplemented"; break;
	case Status::ErrorTimerExpired: return "Status::ErrorTimerExpired"; break;
	case Status::ErrorNotSupported: return "Status::ErrorNotSupported"; break;
	case Status::ErrorBufferOverflow: return "Status::ErrorBufferOverflow"; break;
	case Status::ErrorAlreadyPerformed: return "Status::ErrorAlreadyPerformed"; break;
	case Status::ErrorInProgress: return "Status::ErrorInProgress"; break;
	case Status::ErrorCancelled: return "Status::ErrorCancelled"; break;
	case Status::ErrorDeviceLost: return "Status::ErrorDeviceLost"; break;
	case Status::ErrorTimeout: return "Status::ErrorTimeout"; break;
	case Status::ErrorNotRecoverable: return "Status::ErrorNotRecoverable"; break;
	case Status::ErrorDeadLock: return "Status::ErrorDeadLock"; break;

	// Generic errors, can occurs in any subsystem
	case Status::ErrorMemoryMapFailed: return "Status::ErrorMemoryMapFailed"; break;

	// Graphic-API specific errors
	case Status::ErrorLayerNotPresent: return "Status::ErrorLayerNotPresent"; break;
	case Status::ErrorExtensionNotPresent: return "Status::ErrorExtensionNotPresent"; break;
	case Status::ErrorFeatureNotPresent: return "Status::ErrorFeatureNotPresent"; break;
	case Status::ErrorFragmentedPool: return "Status::ErrorFragmentedPool"; break;
	case Status::ErrorOutOfPoolMemory: return "Status::ErrorOutOfPoolMemory"; break;
	case Status::ErrorInvalidExternalHandle: return "Status::ErrorInvalidExternalHandle"; break;
	case Status::ErrorFragmentation: return "Status::ErrorFragmentation"; break;
	case Status::ErrorInvalidCaptureAddress: return "Status::ErrorInvalidCaptureAddress"; break;
	case Status::ErrorPipelineCompileRequired: return "Status::ErrorPipelineCompileRequired"; break;
	case Status::ErrorSurfaceLost: return "Status::ErrorSurfaceLost"; break;
	case Status::ErrorNativeWindowInUse: return "Status::ErrorNativeWindowInUse"; break;
	case Status::ErrorIncompatibleDisplay: return "Status::ErrorIncompatibleDisplay"; break;
	case Status::ErrorValidationFailed: return "Status::ErrorValidationFailed"; break;
	case Status::ErrorInvalidShader: return "Status::ErrorInvalidShader"; break;
	case Status::ErrorInvalidDrmFormat: return "Status::ErrorInvalidDrmFormat"; break;
	case Status::ErrorFullscreenLost: return "Status::ErrorFullscreenLost"; break;

	case Status::ErrorIdnPunycode: return "Status::ErrorIdnPunycode"; break;
	case Status::ErrorIdnInvalidAceLabel: return "Status::ErrorIdnInvalidAceLabel"; break;
	case Status::ErrorIdnLabelHasDot: return "Status::ErrorIdnLabelHasDot"; break;
	case Status::ErrorIdnEmptyLabel: return "Status::ErrorIdnEmptyLabel"; break;
	case Status::ErrorIdnDisallowed: return "Status::ErrorIdnDisallowed"; break;
	case Status::ErrorIdnBidi: return "Status::ErrorIdnBidi"; break;
	case Status::ErrorIdnContextJ: return "Status::ErrorIdnContextJ"; break;
	case Status::ErrorIdnContextOPunctuation: return "Status::ErrorIdnContextOPunctuation"; break;
	case Status::ErrorIdnContextODigits: return "Status::ErrorIdnContextODigits"; break;
	case Status::ErrorIdnLeadingCombiningMark: return "Status::ErrorIdnLeadingCombiningMark"; break;
	case Status::ErrorIdnLeadingHyphen: return "Status::ErrorIdnLeadingHyphen"; break;
	case Status::ErrorIdnTrailingHyphen: return "Status::ErrorIdnTrailingHyphen"; break;
	case Status::ErrorIdnHyphen34: return "Status::ErrorIdnHyphen34"; break;
	case Status::ErrorIdnLabelTooLong: return "Status::ErrorIdnLabelTooLong"; break;
	case Status::ErrorIdnDomainNameTooLong: return "Status::ErrorIdnDomainNameTooLong"; break;
	}
	return StringView();
}

static StringView getInternalDescription(Status st) {
	switch (st) {
	case Status::Ok: return "Ok"; break;
	case Status::Done: return "Operation completed successfully"; break;
	case Status::Declined: return "Operation was declined without an error"; break;
	case Status::Suspended: return "Operation was suspended without an error"; break;
	case Status::Pending: return "Execution in process"; break;
	case Status::Timeout: return "Operation declined with timeout"; break;
	case Status::Propagate: return "Operation should be propagated"; break;
	case Status::ErrorUnknown: return "Unknown error"; break;
	case Status::ErrorNotPermitted: return "Operation not permitted"; break;
	case Status::ErrorTooManyObjects: return "Too many objects for the command"; break;
	case Status::ErrorOutOfHostMemory: return "Cannot allocate memory on host device"; break;
	case Status::ErrorIncompatibleDevice: return "Incompatible device for linking"; break;
	case Status::ErrorInvalidArguemnt: return "Invalid arguments, fail to execute command"; break;
	case Status::ErrorOutOfDeviceMemory: return "No space left on target device"; break;
	case Status::ErrorNotSupported: return "Operation not supported for this arguments"; break;
	case Status::ErrorCancelled:
		return "Operation cancelled (device is not compatible with it any more)";
		break;
	case Status::ErrorDeviceLost: return "Device is not accessible any more"; break;
	case Status::ErrorBufferOverflow: return "No buffer space available"; break;

	case Status::ErrorMemoryMapFailed: return "Fail to map memory for the object"; break;

	// Graphic-API specific errors
	case Status::ErrorLayerNotPresent:
		return "A requested layer is not present or could not be loaded";
		break;
	case Status::ErrorExtensionNotPresent: return "A requested extension is not supported"; break;
	case Status::ErrorFeatureNotPresent: return "A requested feature is not supported"; break;
	case Status::ErrorFragmentedPool:
		return "A pool allocation has failed due to fragmentation of the pool’s memory";
		break;
	case Status::ErrorOutOfPoolMemory: return "A pool memory allocation has failed"; break;
	case Status::ErrorInvalidExternalHandle:
		return "An external handle is not a valid handle of the specified type";
		break;
	case Status::ErrorFragmentation:
		return "A descriptor pool creation has failed due to fragmentation";
		break;
	case Status::ErrorInvalidCaptureAddress:
		return "A buffer creation or memory allocation failed because the requested address is not "
			   "available";
		break;
	case Status::ErrorPipelineCompileRequired: return "Status::ErrorPipelineCompileRequired"; break;
	case Status::ErrorSurfaceLost: return "A surface is no longer available"; break;
	case Status::ErrorNativeWindowInUse:
		return "The requested window is already in use in a manner which prevents it from being "
			   "used again";
		break;
	case Status::ErrorIncompatibleDisplay:
		return "The display is incompatible in a way that prevents sharing an image";
		break;
	case Status::ErrorValidationFailed:
		return " A command failed because invalid usage was detected by the implementation or a "
			   "validation-layer";
		break;
	case Status::ErrorInvalidShader: return "One or more shaders failed to compile or link"; break;
	case Status::ErrorInvalidDrmFormat: return "Status::ErrorInvalidDrmFormat"; break;
	case Status::ErrorFullscreenLost:
		return "Swapchain did not have exclusive full-screen access any more";
		break;

	// IDN (UTS-46). The wording names the rule that rejected the name, since a caller
	// showing this to a user has no other way to tell the cases apart.
	case Status::ErrorIdnPunycode: return "A Punycode label is not decodable"; break;
	case Status::ErrorIdnInvalidAceLabel:
		return "An ACE (xn--) label does not round-trip or is otherwise invalid";
		break;
	case Status::ErrorIdnLabelHasDot: return "A label contains a dot after processing"; break;
	case Status::ErrorIdnEmptyLabel: return "A domain name label is empty"; break;
	case Status::ErrorIdnDisallowed: return "A character is disallowed for domain names"; break;
	case Status::ErrorIdnBidi: return "The name violates the IDNA Bidi Rule (RFC 5893)"; break;
	case Status::ErrorIdnContextJ:
		return "A joiner (ZWJ/ZWNJ) is used outside its permitted context";
		break;
	case Status::ErrorIdnContextOPunctuation:
		return "A contextual punctuation character is used outside its permitted context";
		break;
	case Status::ErrorIdnContextODigits:
		return "Arabic-Indic and extended Arabic-Indic digits are mixed in one label";
		break;
	case Status::ErrorIdnLeadingCombiningMark:
		return "A label starts with a combining mark";
		break;
	case Status::ErrorIdnLeadingHyphen: return "A label starts with a hyphen"; break;
	case Status::ErrorIdnTrailingHyphen: return "A label ends with a hyphen"; break;
	case Status::ErrorIdnHyphen34:
		return "A label has hyphens in both the third and the fourth position";
		break;
	case Status::ErrorIdnLabelTooLong: return "A label is longer than 63 bytes"; break;
	case Status::ErrorIdnDomainNameTooLong:
		return "The domain name is longer than 255 bytes";
		break;

	default: break;
	}

	return StringView();
}

void getStatusDescription(Status st, const callback<void(StringView)> &cb) {
	char strerrBuffer[STATUS_DESC_BUFFER_SIZE] = {0};

	char *strTarget = strerrBuffer;

	// Use callback-stream output
	auto fn = [&](StringView out) {
		if (strTarget + out.size() < &strerrBuffer[STATUS_DESC_BUFFER_SIZE]) {
			::__sprt_memcpy(strTarget, out.data(), out.size());
			strTarget += out.size();
		}
	};

	auto outCb = callback<void(StringView)>(fn);

	auto name = getStatusName(st);

	if (name.empty()) {
		if (toInt(st) > 0) {
			outCb << "Status::Application(" << toInt(st) << ")";
		} else if (toInt(st) <= -status::STATUS_ERRNO_OFFSET
				&& toInt(st) > -status::STATUS_GENERIC_OFFSET) {
			outCb << "Status::Errno(" << status::toErrno(st) << ")";
		} else if (toInt(st) <= -status::STATUS_GENERIC_OFFSET
				&& toInt(st) > -status::STATUS_GAPI_OFFSET) {
			outCb << "Status::Generic(" << status::toGeneric(st) << ")";
		} else if (toInt(st) <= -status::STATUS_GAPI_OFFSET
				&& toInt(st) > -status::STATUS_WINAPI_OFFSET) {
			outCb << "Status::GApi(" << status::toGApi(st) << ")";
		} else if (toInt(st) <= -status::STATUS_WINAPI_OFFSET
				&& toInt(st) > -status::STATUS_IDN_OFFSET) {
			outCb << "Status::WinAPI(" << status::toWinApi(st) << ")";
		} else if (toInt(st) <= -status::STATUS_IDN_OFFSET
				&& toInt(st) > -status::STATUS_END_OFFSET) {
			outCb << "Status::Idn(" << status::toIdn(st) << ")";
		} else {
			outCb << "Status::Unknown(" << -toInt(st) << ")";
		}
	} else {
		outCb << name;
	}

	auto desc = getInternalDescription(st);
	if (!desc.empty()) {
		outCb << ": " << desc;
	} else if (status::isErrno(st)) {
		// errno-based error
		outCb << ": ";

		auto err = status::toErrno(st);
		auto len = __constexpr_strlen(strerrBuffer);
		auto target = &strerrBuffer[len];

		::__sprt_strerror_s(target, STATUS_DESC_BUFFER_SIZE - len - 1, err);

#if SPRT_WINDOWS
	} else if (status::isWinApi(st)) {
		outCb << ": ";

		auto errorMessageID = status::toWinApi(st);

		char16_t errorBuffer[1'024] = {0};

		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
				errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (wchar_t *)errorBuffer,
				1'024, nullptr);

		unicode::toUtf8([&](StringView str) { outCb << str; }, WideStringView(errorBuffer));
#endif
	} else {
		outCb << ": No description found";
	}

	cb(StringView(strerrBuffer, __constexpr_strlen(strerrBuffer)));
}

} // namespace sprt::status
