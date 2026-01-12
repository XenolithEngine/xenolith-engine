/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "SPCommon.h"

#if LINUX || ANDROID

#include "platform/linux/SPEvent-linux.cc"
#include "platform/uring/SPEventThreadHandle-uring.cc"
#include "platform/uring/SPEventTimer-uring.cc"
#include "platform/uring/SPEvent-uring.cc"

#include "platform/android/SPEvent-android.cc"
#include "platform/android/SPEvent-alooper.cc"
#include "platform/android/SPEventThreadHandle-alooper.cc"

#include "platform/epoll/SPEventThreadHandle-epoll.cc"
#include "platform/epoll/SPEvent-epoll.cc"

#include "platform/fd/SPEventEventFd.cc"
#include "platform/fd/SPEventSignalFd.cc"
#include "platform/fd/SPEventTimerFd.cc"
#include "platform/fd/SPEventPollFd.cc"
#endif

#if WIN32
#include "platform/windows/SPEvent-windows.cc"
#include "platform/windows/SPEvent-iocp.cc"
#include "platform/windows/SPEventTimerIocp.cc"
#include "platform/windows/SPEventThreadIocp.cc"
#include "platform/windows/SPEventPollIocp.cc"
#endif

#if MACOS
#include "platform/darwin/SPEvent-darwin.cc"
#include "platform/darwin/SPEvent-kqueue.cc"
#include "platform/darwin/SPEvent-runloop.cc"
#endif

#include "detail/SPEventHandleClass.cc"
#include "detail/SPEventQueueData.cc"
#include "SPEventBufferChain.cc"
#include "SPEventHandle.cc"
#include "SPEventQueue.cc"
#include "SPEventLooper.cc"
#include "SPEventBus.cc"
