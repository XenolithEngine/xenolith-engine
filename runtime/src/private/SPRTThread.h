/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_SRC_PRIVATE_SPRTTHREAD_H_
#define RUNTIME_SRC_PRIVATE_SPRTTHREAD_H_

#include <sprt/runtime/thread/entry.h>
#include <sprt/runtime/thread/info.h>

namespace sprt::thread {

SPRT_LOCAL void _entry_platform(const callbacks &, NotNull<Ref>);

SPRT_LOCAL void _init(const callbacks &cb, Ref *tm);
SPRT_LOCAL bool _worker(const callbacks &cb, Ref *tm);
SPRT_LOCAL void _dispose(const callbacks &cb, Ref *tm);

} // namespace sprt::thread

#endif // RUNTIME_SRC_PRIVATE_SPRTTHREAD_H_
