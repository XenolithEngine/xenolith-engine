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

#ifndef SPRT_WRAPPERS_WINDOWS_COM_API_H_
#define SPRT_WRAPPERS_WINDOWS_COM_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/gen/wrappers/windows/abi/com_api.h>

/* Clean public names (materialized __SPRT_ values live in abi/com_api.h) */
#define RPC_C_AUTHZ_NONE __SPRT_RPC_C_AUTHZ_NONE
#define RPC_C_AUTHZ_NAME __SPRT_RPC_C_AUTHZ_NAME
#define RPC_C_AUTHZ_DCE __SPRT_RPC_C_AUTHZ_DCE
#define RPC_C_AUTHZ_DEFAULT __SPRT_RPC_C_AUTHZ_DEFAULT
#define RPC_C_AUTHN_NONE __SPRT_RPC_C_AUTHN_NONE
#define RPC_C_AUTHN_DCE_PRIVATE __SPRT_RPC_C_AUTHN_DCE_PRIVATE
#define RPC_C_AUTHN_DCE_PUBLIC __SPRT_RPC_C_AUTHN_DCE_PUBLIC
#define RPC_C_AUTHN_DEC_PUBLIC __SPRT_RPC_C_AUTHN_DEC_PUBLIC
#define RPC_C_AUTHN_GSS_NEGOTIATE __SPRT_RPC_C_AUTHN_GSS_NEGOTIATE
#define RPC_C_AUTHN_WINNT __SPRT_RPC_C_AUTHN_WINNT
#define RPC_C_AUTHN_GSS_SCHANNEL __SPRT_RPC_C_AUTHN_GSS_SCHANNEL
#define RPC_C_AUTHN_GSS_KERBEROS __SPRT_RPC_C_AUTHN_GSS_KERBEROS
#define RPC_C_AUTHN_DPA __SPRT_RPC_C_AUTHN_DPA
#define RPC_C_AUTHN_MSN __SPRT_RPC_C_AUTHN_MSN
#define RPC_C_AUTHN_LEVEL_DEFAULT __SPRT_RPC_C_AUTHN_LEVEL_DEFAULT
#define RPC_C_AUTHN_LEVEL_NONE __SPRT_RPC_C_AUTHN_LEVEL_NONE
#define RPC_C_AUTHN_LEVEL_CONNECT __SPRT_RPC_C_AUTHN_LEVEL_CONNECT
#define RPC_C_AUTHN_LEVEL_CALL __SPRT_RPC_C_AUTHN_LEVEL_CALL
#define RPC_C_AUTHN_LEVEL_PKT __SPRT_RPC_C_AUTHN_LEVEL_PKT
#define RPC_C_AUTHN_LEVEL_PKT_INTEGRITY __SPRT_RPC_C_AUTHN_LEVEL_PKT_INTEGRITY
#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY __SPRT_RPC_C_AUTHN_LEVEL_PKT_PRIVACY
#define RPC_C_IMP_LEVEL_DEFAULT __SPRT_RPC_C_IMP_LEVEL_DEFAULT
#define RPC_C_IMP_LEVEL_ANONYMOUS __SPRT_RPC_C_IMP_LEVEL_ANONYMOUS
#define RPC_C_IMP_LEVEL_IDENTIFY __SPRT_RPC_C_IMP_LEVEL_IDENTIFY
#define RPC_C_IMP_LEVEL_IMPERSONATE __SPRT_RPC_C_IMP_LEVEL_IMPERSONATE
#define RPC_C_IMP_LEVEL_DELEGATE __SPRT_RPC_C_IMP_LEVEL_DELEGATE

#include <sprt/gen/wrappers/windows/api/com_api.h>

#endif
