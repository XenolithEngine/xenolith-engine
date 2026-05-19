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
#include "windows/dll/dllstub_basic_api.cc"
#include "windows/dll/dllstub_context_api.cc"
#include "windows/dll/dllstub_dl_api.cc"
#include "windows/dll/dllstub_file_api.cc"
#include "windows/dll/dllstub_message_api.cc"
#include "windows/dll/dllstub_monitor_api.cc"
#include "windows/dll/dllstub_process_api.cc"
#include "windows/dll/dllstub_thread_api.cc"
#include "windows/dll/dllstub_time_api.cc"
#include "windows/dll/dllstub_user_api.cc"
#include "windows/dll/dllstub_windows.cc"
#include "windows/dll/dllstub_winsock.cc"
#include "windows/dll/dllstub_security_api.cc"
#include "windows/dll/dllstub_debug_api.cc"
#include "windows/startup.cc"
#include "windows/terminate.cc"
#include "windows/memory.cc"
#include "windows/except.cc"
#endif
