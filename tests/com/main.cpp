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

// Standalone smoke test for the freestanding Windows COM support layer. It
// exercises the pieces LLVM's MSVCPaths.cpp depends on — _bstr_t (BSTR owning
// wrapper over oleaut32), _com_ptr_t (reference-counted interface pointer with
// CreateInstance + a QueryInterface-constructor), and _COM_SMARTPTR_TYPEDEF —
// against a live COM runtime. Built for windows-msvc and run under wine.

#include <cstdio>

#ifdef _WIN32

#include <comdef.h>

// Local wide-string compare to avoid depending on a <cwchar> in the freestanding
// sysroot; returns true when the two NUL-terminated wide strings are equal.
static bool wide_equal(const wchar_t *a, const wchar_t *b) {
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	while (*a && (*a == *b)) {
		++a;
		++b;
	}
	return *a == *b;
}

// IShellItem / IFileOperation and CLSID_FileOperation come from com_cxx.hpp,
// which <comdef.h> pulls in.
_COM_SMARTPTR_TYPEDEF(IUnknown, __uuidof(IUnknown));
_COM_SMARTPTR_TYPEDEF(IFileOperation, __uuidof(IFileOperation));

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char *msg) {
	if (cond) {
		std::printf("  [ OK ] %s\n", msg);
		++g_pass;
	} else {
		std::printf("  [FAIL] %s\n", msg);
		++g_fail;
	}
}

static void check_hr(HRESULT hr, bool ok, const char *msg) {
	if (ok) {
		std::printf("  [ OK ] %s (hr=0x%08lx)\n", msg, (unsigned long)hr);
		++g_pass;
	} else {
		std::printf("  [FAIL] %s (hr=0x%08lx)\n", msg, (unsigned long)hr);
		++g_fail;
	}
}

int main() {
	std::printf("== sprt COM smoke test (wine) ==\n");

	// 1. _bstr_t / BSTR round-trip through oleaut32 (SysAllocString / SysStringLen
	//    / SysFreeString). No COM apartment needed.
	{
		bstr_t s(L"Xenolith");
		check(s.length() == 8, "bstr_t::length via SysStringLen == 8");
		check(wide_equal(static_cast<const wchar_t *>(s), L"Xenolith"),
				"bstr_t wide contents match");

		bstr_t copy(s);
		check(copy.length() == 8 && static_cast<const wchar_t *>(copy) != static_cast<const wchar_t *>(s),
				"bstr_t copy is an independent allocation");

		BSTR *slot = s.GetAddress();
		check(slot != nullptr && *slot == nullptr, "GetAddress frees prior value and yields empty slot");
	}

	// 2. Enter a COM apartment.
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	check_hr(hr, SUCCEEDED(hr), "CoInitializeEx(APARTMENTTHREADED)");

	// 3. _com_ptr_t::CreateInstance a genuine coclass, then exercise the smart
	//    pointer + QueryInterface machinery on the live object.
	{
		IFileOperationPtr op;
		hr = op.CreateInstance(CLSID_FileOperation);
		check_hr(hr, SUCCEEDED(hr) && !!op,
				"_com_ptr_t::CreateInstance(CLSID_FileOperation)");

		if (op) {
			// The QueryInterface-constructor: build IUnknownPtr out of the
			// IFileOperation smart pointer (the ISetupConfiguration2Ptr(Query) idiom).
			IUnknownPtr unk(op);
			check(!!unk, "_com_ptr_t QueryInterface-constructor -> IUnknown");

			// Raw QI round-trip: prove AddRef/Release refcounting is real.
			IUnknown *raw = nullptr;
			HRESULT qhr = op->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void **>(&raw));
			check_hr(qhr, SUCCEEDED(qhr) && raw != nullptr,
					"raw IFileOperation->QueryInterface(IID_IUnknown)");
			if (raw) {
				ULONG rc = raw->Release();
				check(rc >= 1, "Release() after QI leaves the object alive (op still holds a ref)");
			}
		}
		// op releases its ref here via ~_com_ptr_t.
	}

	CoUninitialize();

	std::printf("== passed: %d, failed: %d ==\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}

#else // !_WIN32

int main() {
	std::printf("sprt COM smoke test is Windows-only; skipped on this platform.\n");
	return 0;
}

#endif
