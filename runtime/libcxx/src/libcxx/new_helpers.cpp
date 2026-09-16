//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <__verbose_abort>
#include <new>

// sprt: versioned — the runtime must not define canonical-namespace symbols
// (declarations patched accordingly in <__new/nothrow_t.h> / <__new/exceptions.h>).
_LIBCPP_BEGIN_NAMESPACE_STD

#ifndef __GLIBCXX__
const nothrow_t nothrow{};
#endif

#ifndef LIBSTDCXX

void __throw_bad_alloc() {
#  if _LIBCPP_HAS_EXCEPTIONS
  throw bad_alloc();
#  else
  _LIBCPP_VERBOSE_ABORT("bad_alloc was thrown in -fno-exceptions mode");
#  endif
}

#endif // !LIBSTDCXX

_LIBCPP_END_NAMESPACE_STD
