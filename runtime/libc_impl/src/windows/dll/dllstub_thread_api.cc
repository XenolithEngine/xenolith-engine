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

#include <sprt/wrappers/windows/thread_api.h>

#include "dllloader.h"

extern "C" {

// ---- Thread Management Functions (kernel32) ----

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags,
		LPDWORD lpThreadId) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CreateThread)>(loader->kernel32.CreateThread,
			lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags,
			lpThreadId);
}

HANDLE OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&OpenThread)>(loader->kernel32.OpenThread,
			dwDesiredAccess, bInheritHandle, dwThreadId);
}

HANDLE GetCurrentThread(void) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetCurrentThread)>(loader->kernel32.GetCurrentThread);
}

DWORD GetCurrentThreadId(void) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetCurrentThreadId)>(
			loader->kernel32.GetCurrentThreadId);
}

VOID GetCurrentThreadStackLimits(PULONG_PTR LowLimit, PULONG_PTR HighLimit) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&GetCurrentThreadStackLimits)>(
			loader->kernel32.GetCurrentThreadStackLimits, LowLimit, HighLimit);
}

int GetThreadPriority(HANDLE hThread) {
	auto loader = sprt::DllLoader::get();
	return (int)loader->kernel32.call<decltype(&GetThreadPriority)>(
			loader->kernel32.GetThreadPriority, hThread);
}

BOOL SetThreadPriority(HANDLE hThread, int nPriority) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetThreadPriority)>(loader->kernel32.SetThreadPriority,
			hThread, (DWORD)nPriority);
}

void ExitThread(DWORD dwExitCode) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&ExitThread)>(loader->kernel32.ExitThread, dwExitCode);
}

DWORD ResumeThread(HANDLE hThread) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&ResumeThread)>(loader->kernel32.ResumeThread, hThread);
}

BOOL QueueUserAPC2(PAPCFUNC ApcRoutine, HANDLE Thread, ULONG_PTR Data, QUEUE_USER_APC_FLAGS Flags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&QueueUserAPC2)>(loader->kernel32.QueueUserAPC2,
			ApcRoutine, Thread, Data, Flags);
}

HRESULT SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetThreadDescription)>(
			loader->kernel32.GetThreadDescription, hThread, lpThreadDescription);
}

BOOL GetSystemCpuSetInformation(PSYSTEM_CPU_SET_INFORMATION Information, ULONG BufferLength,
		PULONG ReturnedLength, HANDLE Process, ULONG Flags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetSystemCpuSetInformation)>(
			loader->kernel32.GetSystemCpuSetInformation, Information, BufferLength, ReturnedLength,
			Process, Flags);
}

BOOL SetThreadSelectedCpuSets(HANDLE Thread, const ULONG *CpuSetIds, ULONG CpuSetIdCount) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetThreadSelectedCpuSets)>(
			loader->kernel32.SetThreadSelectedCpuSets, Thread, (ULONG *)CpuSetIds, CpuSetIdCount);
}

BOOL GetThreadSelectedCpuSets(HANDLE Thread, PULONG CpuSetIds, ULONG CpuSetIdCount,
		PULONG RequiredIdCount) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetThreadSelectedCpuSets)>(
			loader->kernel32.GetThreadSelectedCpuSets, Thread, CpuSetIds, CpuSetIdCount,
			RequiredIdCount);
}

// ---- Toolhelp Functions (kernel32) ----

HANDLE CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CreateToolhelp32Snapshot)>(
			loader->kernel32.CreateToolhelp32Snapshot, dwFlags, th32ProcessID);
}

BOOL Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&Thread32First)>(loader->kernel32.Thread32First,
			hSnapshot, lpte);
}

BOOL Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&Thread32Next)>(loader->kernel32.Thread32Next, hSnapshot,
			lpte);
}

WINAPI VOID InitializeSRWLock(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&InitializeSRWLock)>(loader->kernel32.InitializeSRWLock,
			SRWLock);
}

WINAPI VOID ReleaseSRWLockExclusive(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&ReleaseSRWLockExclusive)>(
			loader->kernel32.ReleaseSRWLockExclusive, SRWLock);
}

WINAPI VOID ReleaseSRWLockShared(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&ReleaseSRWLockShared)>(loader->kernel32.ReleaseSRWLockShared,
			SRWLock);
}

WINAPI VOID AcquireSRWLockExclusive(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&AcquireSRWLockExclusive)>(
			loader->kernel32.AcquireSRWLockExclusive, SRWLock);
}

WINAPI VOID AcquireSRWLockShared(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&AcquireSRWLockShared)>(loader->kernel32.AcquireSRWLockShared,
			SRWLock);
}

WINAPI BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&TryAcquireSRWLockExclusive)>(
			loader->kernel32.TryAcquireSRWLockExclusive, SRWLock);
}

WINAPI BOOLEAN TryAcquireSRWLockShared(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&TryAcquireSRWLockShared)>(
			loader->kernel32.TryAcquireSRWLockShared, SRWLock);
}

WINAPI BOOL SwitchToThread(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SwitchToThread)>(loader->kernel32.SwitchToThread);
}

} // extern "C"
