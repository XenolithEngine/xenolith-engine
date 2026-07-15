//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___NEW_NOTHROW_T_H
#define _LIBCPP___NEW_NOTHROW_T_H

#include <__config>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

#if defined(_LIBCPP_ABI_VCRUNTIME)
#  include <new.h>
#else
_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
struct _LIBCPP_EXPORTED_FROM_ABI nothrow_t {
  explicit nothrow_t() = default;
};
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD
// sprt: the nothrow OBJECT is versioned — its definition comes from the sprt runtime,
// which must not define canonical-namespace symbols. The nothrow_t TYPE above stays
// canonical: it participates in the replaceable operator new/delete manglings, which
// must match libc++abi's weak set.
_LIBCPP_BEGIN_NAMESPACE_STD
extern _LIBCPP_EXPORTED_FROM_ABI const nothrow_t nothrow;
_LIBCPP_END_NAMESPACE_STD
#endif // _LIBCPP_ABI_VCRUNTIME

#endif // _LIBCPP___NEW_NOTHROW_T_H
