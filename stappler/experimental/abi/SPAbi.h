/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_ABI_SPABI_H_
#define STAPPLER_ABI_SPABI_H_

#include "SPDso.h"
#include "SPStringView.h" // IWYU pragma: keep


namespace STAPPLER_VERSIONIZED stappler::thread {

class Thread;

}

namespace STAPPLER_VERSIONIZED stappler::abi {

SP_PUBLIC void initialize(int argc, const char *argv[]);

SP_PUBLIC void *open(StringView name, DsoFlags flags, const char **err);
SP_PUBLIC void close(DsoFlags flags, void *handle);
SP_PUBLIC void *sym(void *h, StringView name, DsoSymFlags flags, const char **err);

SP_PUBLIC void *createThread(void (*)(thread::Thread *), thread::Thread *, uint32_t flags);
SP_PUBLIC void *joinThread(void *);
SP_PUBLIC void detachThread(void *);

} // namespace stappler::abi

#endif // STAPPLER_ABI_SPABI_H_
