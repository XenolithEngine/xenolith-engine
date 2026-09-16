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

// <rpc.h> — minimal MS-RPC surface: just the UUID generation/formatting used to
// mint unique names (e.g. LLDB's anonymous pipe names).

#ifndef SPRT_WRAPPERS_WINDOWS_RPC_H_
#define SPRT_WRAPPERS_WINDOWS_RPC_H_

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/abi/rpc.h>

/* Clean public names (materialized __SPRT_ values / types live in abi/rpc.h) */
#define RPC_S_OK __SPRT_RPC_S_OK
#define RPC_S_UUID_LOCAL_ONLY __SPRT_RPC_S_UUID_LOCAL_ONLY
#define RPC_S_UUID_NO_ADDRESS __SPRT_RPC_S_UUID_NO_ADDRESS

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI RPC_STATUS UuidCreate(UUID *Uuid);

__SPRT_WIN_IMPORT WINAPI RPC_STATUS UuidCreateSequential(UUID *Uuid);

__SPRT_WIN_IMPORT WINAPI RPC_STATUS UuidToStringA(const UUID *Uuid, RPC_CSTR *StringUuid);

__SPRT_WIN_IMPORT WINAPI RPC_STATUS UuidToStringW(const UUID *Uuid, unsigned short **StringUuid);

__SPRT_WIN_IMPORT WINAPI RPC_STATUS RpcStringFreeA(RPC_CSTR *String);

__SPRT_WIN_IMPORT WINAPI RPC_STATUS RpcStringFreeW(unsigned short **String);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_RPC_H_
