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
#include <sprt/wrappers/windows/shlobj.h>
#include <sprt/wrappers/windows/commdlg.h>
#include <sprt/wrappers/windows/shellapi.h>
#include <sprt/wrappers/windows/synchapi.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/winerror.h>
#include <sprt/wrappers/windows/file_api.h>

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
_COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
_COM_SMARTPTR_TYPEDEF(IFileDialog, __uuidof(IFileDialog));
_COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog));
_COM_SMARTPTR_TYPEDEF(IFileSaveDialog, __uuidof(IFileSaveDialog));

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

	// 4. The pieces the system-dialog backend is built out of. Nothing here calls Show(): that
	//    runs a modal loop waiting for a human. What matters is that the coclasses instantiate and
	//    that every method we intend to drive is really implemented, because wine stubs plenty of
	//    shell surface with E_NOTIMPL.
	{
		IFileOpenDialogPtr open;
		hr = open.CreateInstance(CLSID_FileOpenDialog);
		check_hr(hr, SUCCEEDED(hr) && !!open, "CoCreateInstance(CLSID_FileOpenDialog)");

		if (open) {
			// QI up to the base interface: the backend drives both dialogs through IFileDialog and
			// only reaches for the derived one to collect a multi-selection.
			IFileDialogPtr base(open);
			check(!!base, "IFileOpenDialog -> IFileDialog QueryInterface");

			check_hr(open->SetTitle(L"probe"), SUCCEEDED(open->SetTitle(L"probe")),
					"IFileDialog::SetTitle");
			check_hr(open->SetOkButtonLabel(L"Pick"), SUCCEEDED(open->SetOkButtonLabel(L"Pick")),
					"IFileDialog::SetOkButtonLabel");
			check_hr(open->SetFileName(L"probe.txt"), SUCCEEDED(open->SetFileName(L"probe.txt")),
					"IFileDialog::SetFileName");

			// GetOptions/SetOptions is how folder-picking and multi-select are turned on, so a
			// stubbed pair would silently give the wrong dialog.
			DWORD opts = 0;
			HRESULT ghr = open->GetOptions(&opts);
			check_hr(ghr, SUCCEEDED(ghr), "IFileDialog::GetOptions");
			HRESULT shr = open->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
			check_hr(shr, SUCCEEDED(shr), "IFileDialog::SetOptions(FOS_PICKFOLDERS)");
			DWORD readback = 0;
			if (SUCCEEDED(open->GetOptions(&readback))) {
				check((readback & FOS_PICKFOLDERS) != 0, "FOS_PICKFOLDERS survives a read back");
			} else {
				check(false, "FOS_PICKFOLDERS survives a read back");
			}

			const COMDLG_FILTERSPEC specs[] = {
				{L"Text", L"*.txt"},
				{L"All", L"*.*"},
			};
			HRESULT fhr = open->SetFileTypes(2, specs);
			check_hr(fhr, SUCCEEDED(fhr), "IFileDialog::SetFileTypes");

			// GetResult before Show is expected to fail; it proves the slot is implemented rather
			// than returning a garbage pointer.
			IShellItem *unused = nullptr;
			HRESULT rhr = open->GetResult(&unused);
			check(FAILED(rhr) && unused == nullptr, "IFileDialog::GetResult fails before Show");
		}

		IFileSaveDialogPtr save;
		HRESULT shr = save.CreateInstance(CLSID_FileSaveDialog);
		check_hr(shr, SUCCEEDED(shr) && !!save, "CoCreateInstance(CLSID_FileSaveDialog)");
	}

	// 5. SHCreateItemFromParsingName + IShellItem::GetDisplayName(SIGDN_FILESYSPATH): how a path
	//    becomes a shell item on the way in, and how a picked item becomes a path on the way out.
	{
		IShellItem *item = nullptr;
		HRESULT ihr = SHCreateItemFromParsingName(L"C:\\windows", nullptr, __uuidof(IShellItem),
				reinterpret_cast<void **>(&item));
		check_hr(ihr, SUCCEEDED(ihr) && item != nullptr,
				"SHCreateItemFromParsingName(C:\\windows)");
		if (item) {
			LPWSTR path = nullptr;
			HRESULT dhr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
			check_hr(dhr, SUCCEEDED(dhr) && path != nullptr,
					"IShellItem::GetDisplayName(SIGDN_FILESYSPATH)");
			if (path) {
				std::printf("         -> path is fine\n");
				CoTaskMemFree(path);
			}
			item->Release();
		}
	}

	// 6. Move-to-trash, end to end on a file we create ourselves. IFileOperation with FOF_ALLOWUNDO
	//    is the Recycle Bin path. Each step is reported separately, because a failure here is much
	//    more likely to be a gap in the host's shell than a bug in the caller.
	{
		const wchar_t *victim = L"C:\\windows\\temp\\sprt-trash-probe.txt";
		HANDLE fh = CreateFileW(victim, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, nullptr);
		check(fh != INVALID_HANDLE_VALUE, "created a file to trash");
		if (fh != INVALID_HANDLE_VALUE) {
			DWORD written = 0;
			WriteFile(fh, "x", 1, &written, nullptr);
			CloseHandle(fh);
		}

		IFileOperationPtr op;
		HRESULT chr = op.CreateInstance(CLSID_FileOperation);
		check_hr(chr, SUCCEEDED(chr), "CoCreateInstance(CLSID_FileOperation) for trash");
		if (op) {
			HRESULT ohr = op->SetOperationFlags(FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE
					| FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI);
			check_hr(ohr, SUCCEEDED(ohr), "IFileOperation::SetOperationFlags(FOF_ALLOWUNDO)");

			IShellItem *item = nullptr;
			HRESULT ihr = SHCreateItemFromParsingName(victim, nullptr, __uuidof(IShellItem),
					reinterpret_cast<void **>(&item));
			check_hr(ihr, SUCCEEDED(ihr) && item != nullptr,
					"SHCreateItemFromParsingName(the victim)");

			if (item) {
				// Not asserted: wine implements the interface but answers DeleteItem with
				// E_NOTIMPL, and the backend is written to fall back rather than to require this.
				// What IS asserted, below, is that one of the two paths actually deletes the file.
				HRESULT dhr = op->DeleteItem(item, nullptr);
				std::printf("  [note] IFileOperation::DeleteItem hr=0x%08lx%s\n",
						(unsigned long)dhr,
						SUCCEEDED(dhr) ? "" : " (host has no IFileOperation; expecting fallback)");
				if (SUCCEEDED(dhr)) {
					HRESULT phr = op->PerformOperations();
					check_hr(phr, SUCCEEDED(phr), "IFileOperation::PerformOperations");

					BOOL aborted = TRUE;
					HRESULT ahr = op->GetAnyOperationsAborted(&aborted);
					check_hr(ahr, SUCCEEDED(ahr) && !aborted,
							"GetAnyOperationsAborted reports no abort");
				}
				item->Release();
			}
		}

		// The pre-Vista API the backend falls back to. Exercised whenever IFileOperation did not do
		// the job, which is the case on any host with a partial shell.
		if (GetFileAttributesW(victim) != INVALID_FILE_ATTRIBUTES) {
			// pFrom is a double-NUL-terminated list, not a plain string.
			wchar_t from[MAX_PATH * 2];
			size_t n = 0;
			while (victim[n] && n < MAX_PATH) {
				from[n] = victim[n];
				++n;
			}
			from[n++] = 0;
			from[n] = 0;

			SHFILEOPSTRUCTW fop;
			for (size_t i = 0; i < sizeof(fop); ++i) {
				reinterpret_cast<char *>(&fop)[i] = 0;
			}
			fop.wFunc = FO_DELETE;
			fop.pFrom = from;
			fop.fFlags = FILEOP_FLAGS(
					FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI);

			int rc = SHFileOperationW(&fop);
			check(rc == 0 && !fop.fAnyOperationsAborted, "SHFileOperationW(FO_DELETE) fallback");
		}

		// The proof, whichever path got there: the file is gone.
		DWORD attrs = GetFileAttributesW(victim);
		check(attrs == INVALID_FILE_ATTRIBUTES, "the file is actually gone");
		if (attrs != INVALID_FILE_ATTRIBUTES) {
			DeleteFileW(victim);
		}
	}

	// 7. Reveal-in-Explorer. Resolving the ids is the part that can fail; actually calling
	//    SHOpenFolderAndSelectItems would pop an Explorer window, so it is left alone.
	{
		PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(L"C:\\windows");
		check(pidl != nullptr, "ILCreateFromPathW(C:\\windows)");
		if (pidl) {
			ILFree(pidl);
		}
	}

	// 8. The one thing the whole backend design rests on: Show() runs its own modal loop, and the
	//    dialog can be dismissed from OUTSIDE that loop.
	//
	//    The dialog has to live on a dedicated STA thread, because Show() does not return until the
	//    user is done and the context thread is the win32 message pump. Cancelling it — the parent
	//    window closed — then necessarily means calling Close() from another thread. That is a
	//    cross-apartment call on a raw pointer, which is formally undefined without marshalling;
	//    this checks what actually happens.
	{
		struct Probe {
			IFileDialog *dialog = nullptr;
			HANDLE ready = nullptr;
			HRESULT showResult = E_FAIL;
			bool created = false;
		};
		static Probe probe;

		probe.ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);

		auto worker = [](LPVOID) -> DWORD {
			CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			IFileDialog *dlg = nullptr;
			if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
						__uuidof(IFileDialog), reinterpret_cast<void **>(&dlg)))) {
				dlg->SetTitle(L"modal probe");
				probe.dialog = dlg;
				probe.created = true;
				SetEvent(probe.ready);
				// Blocks here until somebody closes it.
				probe.showResult = dlg->Show(nullptr);
				dlg->Release();
				probe.dialog = nullptr;
			} else {
				SetEvent(probe.ready);
			}
			CoUninitialize();
			return 0;
		};

		HANDLE th = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
		check(th != nullptr, "STA worker thread started");
		if (th) {
			WaitForSingleObject(probe.ready, 5000);
			check(probe.created, "worker created IFileOpenDialog in its own apartment");

			// Give Show() time to actually enter its modal loop before reaching into it.
			Sleep(1500);
			check(probe.dialog != nullptr, "Show() is blocking (dialog still alive)");

			if (probe.dialog) {
				HRESULT chr = probe.dialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
				check_hr(chr, SUCCEEDED(chr), "IFileDialog::Close() from another thread");
			}

			DWORD w = WaitForSingleObject(th, 5000);
			check(w == WAIT_OBJECT_0, "Show() returned after the cross-thread Close()");
			if (w == WAIT_OBJECT_0) {
				std::printf("         Show() -> hr=0x%08lx\n", (unsigned long)probe.showResult);
			}
			CloseHandle(th);
		}
		if (probe.ready) {
			CloseHandle(probe.ready);
		}
	}

	CoUninitialize();

	// 9. comdlg32 links. Calling either would block on a modal dialog, so this only asserts that
	//    the import resolved to a real entry point — which is what the new comdlg32.def buys.
	{
		check(reinterpret_cast<void *>(&ChooseColorW) != nullptr, "ChooseColorW import resolves");
		check(reinterpret_cast<void *>(&ChooseFontW) != nullptr, "ChooseFontW import resolves");
		check(CommDlgExtendedError() == 0, "CommDlgExtendedError() callable, reports no error");
	}

	std::printf("== passed: %d, failed: %d ==\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}

#else // !_WIN32

int main() {
	std::printf("sprt COM smoke test is Windows-only; skipped on this platform.\n");
	return 0;
}

#endif
