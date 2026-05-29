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
#include <sprt/wrappers/windows/bcrypt.h>

#include "dllloader.h"

extern "C" {
WINAPI NTSTATUS BCryptGenRandom(BCRYPT_ALG_HANDLE hAlgorithm, PUCHAR pbBuffer, ULONG cbBuffer,
		ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptGenRandom, hAlgorithm, pbBuffer, cbBuffer,
			dwFlags);
}

WINAPI NTSTATUS BCryptGenerateSymmetricKey(BCRYPT_ALG_HANDLE hAlgorithm, BCRYPT_KEY_HANDLE *phKey,
		PUCHAR pbKeyObject, ULONG cbKeyObject, PUCHAR pbSecret, ULONG cbSecret, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptGenerateSymmetricKey, hAlgorithm, phKey,
			pbKeyObject, cbKeyObject, pbSecret, cbSecret, dwFlags);
}

WINAPI NTSTATUS BCryptDestroyKey(BCRYPT_KEY_HANDLE hKey) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptDestroyKey, hKey);
}

WINAPI NTSTATUS BCryptCreateHash(BCRYPT_ALG_HANDLE hAlgorithm, BCRYPT_HASH_HANDLE *phHash,
		PUCHAR pbHashObject, ULONG cbHashObject, PUCHAR pbSecret, ULONG cbSecret, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptCreateHash, hAlgorithm, phHash, pbHashObject,
			cbHashObject, pbSecret, cbSecret, dwFlags);
}

WINAPI NTSTATUS BCryptHashData(BCRYPT_HASH_HANDLE hHash, PUCHAR pbInput, ULONG cbInput,
		ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptHashData, hHash, pbInput, cbInput, dwFlags);
}

WINAPI NTSTATUS BCryptFinishHash(BCRYPT_HASH_HANDLE hHash, PUCHAR pbOutput, ULONG cbOutput,
		ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptFinishHash, hHash, pbOutput, cbOutput, dwFlags);
}

WINAPI NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE hHash) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptDestroyHash, hHash);
}

WINAPI NTSTATUS BCryptGetProperty(BCRYPT_HANDLE hObject, LPCWSTR pszProperty, PUCHAR pbOutput,
		ULONG cbOutput, ULONG *pcbResult, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptGetProperty, hObject, pszProperty, pbOutput,
			cbOutput, pcbResult, dwFlags);
}

WINAPI NTSTATUS BCryptSetProperty(BCRYPT_HANDLE hObject, LPCWSTR pszProperty, PUCHAR pbInput,
		ULONG cbInput, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptSetProperty, hObject, pszProperty, pbInput,
			cbInput, dwFlags);
}

WINAPI NTSTATUS BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE *phAlgorithm, LPCWSTR pszAlgId,
		LPCWSTR pszImplementation, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptOpenAlgorithmProvider, phAlgorithm, pszAlgId,
			pszImplementation, dwFlags);
}

WINAPI NTSTATUS BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE hAlgorithm, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptCloseAlgorithmProvider, hAlgorithm, dwFlags);
}

WINAPI NTSTATUS BCryptDeriveKeyPBKDF2(BCRYPT_ALG_HANDLE hPrf, PUCHAR pbPassword, ULONG cbPassword,
		PUCHAR pbSalt, ULONG cbSalt, ULONGLONG cIterations, PUCHAR pbDerivedKey, ULONG cbDerivedKey,
		ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptDeriveKeyPBKDF2, hPrf, pbPassword, cbPassword,
			pbSalt, cbSalt, cIterations, pbDerivedKey, cbDerivedKey, dwFlags);
}

WINAPI NTSTATUS BCryptEncrypt(BCRYPT_KEY_HANDLE hKey, PUCHAR pbInput, ULONG cbInput,
		VOID *pPaddingInfo, PUCHAR pbIV, ULONG cbIV, PUCHAR pbOutput, ULONG cbOutput,
		ULONG *pcbResult, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptEncrypt, hKey, pbInput, cbInput, pPaddingInfo,
			pbIV, cbIV, pbOutput, cbOutput, pcbResult, dwFlags);
}

WINAPI NTSTATUS BCryptDecrypt(BCRYPT_KEY_HANDLE hKey, PUCHAR pbInput, ULONG cbInput,
		VOID *pPaddingInfo, PUCHAR pbIV, ULONG cbIV, PUCHAR pbOutput, ULONG cbOutput,
		ULONG *pcbResult, ULONG dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, bcrypt, BCryptDecrypt, hKey, pbInput, cbInput, pPaddingInfo,
			pbIV, cbIV, pbOutput, cbOutput, pcbResult, dwFlags);
}
}
