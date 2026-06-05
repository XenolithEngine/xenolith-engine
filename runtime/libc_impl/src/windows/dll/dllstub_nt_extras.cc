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

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/winver.h>
#include <sprt/runtime/thread/qonce.h>

#include "dllloader.h"

extern "C" {
WINAPI NTSTATUS NtCreateWaitCompletionPacket(PHANDLE WaitCompletionPacketHandle,
		ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtCreateWaitCompletionPacket)>(
			loader->ntdll.NtCreateWaitCompletionPacket, WaitCompletionPacketHandle, DesiredAccess,
			ObjectAttributes);
}

WINAPI NTSTATUS NtAssociateWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		HANDLE IoCompletionHandle, HANDLE TargetObjectHandle, PVOID KeyContext, PVOID ApcContext,
		NTSTATUS IoStatus, ULONG_PTR IoStatusInformation, PBOOLEAN AlreadySignaled) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtAssociateWaitCompletionPacket)>(
			loader->ntdll.NtAssociateWaitCompletionPacket, WaitCompletionPacketHandle,
			IoCompletionHandle, TargetObjectHandle, KeyContext, ApcContext, IoStatus,
			IoStatusInformation, AlreadySignaled);
}

WINAPI NTSTATUS NtCancelWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		BOOLEAN RemoveSignaledPacket) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtCancelWaitCompletionPacket)>(
			loader->ntdll.NtCancelWaitCompletionPacket, WaitCompletionPacketHandle,
			RemoveSignaledPacket);
}

WINAPI BOOL NtCompletionPacketAvailable() {
	auto loader = sprt::DllLoader::get();
	if (!loader->ntdll.NtCreateWaitCompletionPacket.fn) {
		if (!loader->ntdll.load(&loader->ntdll.NtCreateWaitCompletionPacket)) {
			return FALSE;
		}
	}
	if (!loader->ntdll.NtAssociateWaitCompletionPacket.fn) {
		if (!loader->ntdll.load(&loader->ntdll.NtAssociateWaitCompletionPacket)) {
			return FALSE;
		}
	}
	if (!loader->ntdll.NtCancelWaitCompletionPacket.fn) {
		if (!loader->ntdll.load(&loader->ntdll.NtCancelWaitCompletionPacket)) {
			return FALSE;
		}
	}
	return TRUE;
}

WINAPI int __sprt_RestartEventCompletion2(void *hPacket, void *hIOCP, void *hEvent,
		DWORD dwNumberOfBytesTransferred, UINT_PTR dwCompletionKey, void *lpOverlapped) {
	HRESULT hr = NtAssociateWaitCompletionPacket(hPacket, hIOCP, hEvent, (PVOID)dwCompletionKey,
			(PVOID)lpOverlapped, 0, dwNumberOfBytesTransferred, NULL);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		switch (hr) {
		case STATUS_NO_MEMORY: SetLastError(ERROR_OUTOFMEMORY); break;
		case STATUS_INVALID_HANDLE: // not valid handle passed for hIOCP
		case STATUS_OBJECT_TYPE_MISMATCH: // incorrect handle passed for hIOCP
		case STATUS_INVALID_PARAMETER_1:
		case STATUS_INVALID_PARAMETER_2: SetLastError(ERROR_INVALID_PARAMETER); break;
		case STATUS_INVALID_PARAMETER_3:
			if (hEvent) {
				SetLastError(ERROR_INVALID_HANDLE);
			} else {
				SetLastError(ERROR_INVALID_PARAMETER);
			}
			break;
		default: SetLastError(hr);
		}
		return FALSE;
	}
}

WINAPI int __sprt_RestartEventCompletion(void *hPacket, void *hIOCP, void *hEvent,
		const void **ncompletion) {
	if (!ncompletion) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	auto completion = (const OVERLAPPED_ENTRY *)ncompletion;

	return __sprt_RestartEventCompletion2(hPacket, hIOCP, hEvent,
			completion->dwNumberOfBytesTransferred, completion->lpCompletionKey,
			completion->lpOverlapped);
}

WINAPI int __sprt_CancelEventCompletion(void *hPacket, int cancel) {
	HRESULT hr = NtCancelWaitCompletionPacket(hPacket, cancel);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		SetLastError(hr);
		return FALSE;
	}
}

WINAPI void *__sprt_ReportEventAsCompletion(void *hIOCP, void *hEvent,
		DWORD dwNumberOfBytesTransferred, UINT_PTR dwCompletionKey, void *lpOverlapped) {

	HANDLE hPacket = NULL;
	HRESULT hr = NtCreateWaitCompletionPacket(&hPacket, GENERIC_ALL, NULL);

	if (SUCCEEDED(hr)) {
		OVERLAPPED_ENTRY completion{};
		completion.dwNumberOfBytesTransferred = dwNumberOfBytesTransferred;
		completion.lpCompletionKey = dwCompletionKey;
		completion.lpOverlapped = (LPOVERLAPPED)lpOverlapped;

		if (!__sprt_RestartEventCompletion(hPacket, hIOCP, hEvent, (const void **)&completion)) {
			NtClose(hPacket);
			hPacket = NULL;
		}
	} else {
		switch (hr) {
		case STATUS_NO_MEMORY: SetLastError(ERROR_OUTOFMEMORY); break;
		default: SetLastError(hr);
		}
	}
	return hPacket;
}

WINAPI WINAPI_PROVIDER GetWinApiProvider() {
	static sprt::qonce s_WinAPIOnce;
	static WINAPI_PROVIDER s_provider;

	s_WinAPIOnce([] {
		auto ntdll = GetModuleHandleW(L"ntdll.dll");
		if (ntdll && GetProcAddress(ntdll, "wine_get_version")) {
			s_provider = WinApiProviderWine;
			return;
		}

		OSVERSIONINFOW info;
		memset(&info, 0, sizeof(info));
		// GetVersionExW REQUIRES dwOSVersionInfoSize to equal sizeof(OSVERSIONINFOW)
		// (or the EX variant); otherwise it returns FALSE and leaves szCSDVersion
		// untouched — which would silently break the ReactOS check below.
		info.dwOSVersionInfoSize = sizeof(info);
		if (GetVersionExW(&info)) {
			// ReactOS stores a second "ReactOS <version>" string in szCSDVersion,
			// immediately after the Windows-compatible CSD string's NUL terminator.
			// Genuine Windows leaves only the single CSD string.
			constexpr size_t kCap = sizeof(info.szCSDVersion) / sizeof(info.szCSDVersion[0]);
			static const wchar_t kSig[] = L"ReactOS";
			constexpr size_t kSigLen = (sizeof(kSig) / sizeof(kSig[0])) - 1; // excludes NUL

			// Bounded length of the first (CSD) string, so a non-terminated buffer
			// cannot over-read.
			size_t len = 0;
			while (len < kCap && info.szCSDVersion[len] != 0) {
				++len;
			}

			// The signature starts just past that NUL (index len+1) and the whole
			// comparison must stay inside the fixed array.
			if (len < kCap && len + 1 + kSigLen <= kCap
					&& memcmp(&info.szCSDVersion[len + 1], kSig, kSigLen * sizeof(wchar_t)) == 0) {
				s_provider = WinApiProviderReactOS;
				return;
			}
		}

		s_provider = WinApiProviderMicrosoft;
	});

	return s_provider;
}
}
