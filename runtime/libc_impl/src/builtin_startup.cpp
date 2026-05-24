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

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_WINDOWS
#include "windows/dll/dllloader.cc"

#include "windows/dll/dllstub_nt_extras.cc"


#if !__SPRT_WIN_USE_IMPORT_STRING_LIB

#include "windows/dll/dllstub_gen_strings.cc"

#endif


#if !__SPRT_WIN_USE_IMPORT_LIB

#include "windows/dll/dllstub2_advapi32.cc"
#include "windows/dll/dllstub2_bcrypt.cc"
#include "windows/dll/dllstub2_combase.cc"
#include "windows/dll/dllstub2_crypt32.cc"
#include "windows/dll/dllstub2_dbghelp.cc"
#include "windows/dll/dllstub2_dwmapi.cc"
#include "windows/dll/dllstub2_dxva2.cc"
#include "windows/dll/dllstub2_gdi32.cc"
#include "windows/dll/dllstub2_kernel32.cc"
#include "windows/dll/dllstub2_kernelbase.cc"
#include "windows/dll/dllstub2_ntdll.cc"
#include "windows/dll/dllstub2_oleaut32.cc"
#include "windows/dll/dllstub2_setupapi.cc"
#include "windows/dll/dllstub2_shcore.cc"
#include "windows/dll/dllstub2_shell32.cc"
#include "windows/dll/dllstub2_user32.cc"
#include "windows/dll/dllstub2_userenv.cc"
#include "windows/dll/dllstub2_uxtheme.cc"
#include "windows/dll/dllstub2_ws2_32.cc"

#endif

#include "windows/startup.cc"
#include "windows/terminate.cc"
#include "windows/memory.cc"
#include "windows/except.cc"
#endif
