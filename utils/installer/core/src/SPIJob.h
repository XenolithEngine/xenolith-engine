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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIJOB_H_
#define UTILS_INSTALLER_CORE_SRC_SPIJOB_H_

#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Run `fn` on a private single-use thread and block until it returns.
//
// Two of the core's operations are thread-bound and this is what satisfies them:
//
//  * makefile::runBuild acquires a Looper for the CALLING thread, masking out the engines that
//    cannot spawn processes; LooperInfo is honored only on the FIRST acquire for a thread. So it
//    must never run on a thread that already owns a Looper (an app/UI thread) nor on a shared
//    worker-pool thread, where the Looper would outlive the task and be reused with a mask nobody
//    asked for.
//  * ::chdir and ::setenv are process-global; funnelling them through one job at a time is what
//    makes them safe. Serialization is inherent — the caller is blocked for the job's lifetime.
//
// `fn` runs inside a fresh memory pool, so pool-model code (stappler_makefile) has a context, and
// the thread's Looper dies with the thread. Capturing by reference is safe: this call blocks.
SP_PUBLIC Status runJob(const Callback<void()> &fn);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIJOB_H_
