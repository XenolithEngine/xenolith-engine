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

#include <sprt/runtime/dispatch/event.h>

#include <sprt/c/bits/__sprt_def.h> // SPRT_WASM

#if SPRT_LINUX || SPRT_ANDROID

#include "platform/linux/SPEvent-linux.cc"
#include "platform/uring/SPEventThreadHandle-uring.cc"
#include "platform/uring/SPEventTimer-uring.cc"
#include "platform/uring/SPEvent-uring.cc"

#include "platform/android/SPEvent-alooper.cc"
#include "platform/android/SPEventThreadHandle-alooper.cc"

#include "platform/epoll/SPEventThreadHandle-epoll.cc"
#include "platform/epoll/SPEvent-epoll.cc"

#include "platform/fd/SPEventEventFd.cc"
#include "platform/fd/SPEventSignalFd.cc"
#include "platform/fd/SPEventInotify.cc"
#include "platform/fd/SPEventTimerFd.cc"
#include "platform/fd/SPEventPollFd.cc"
#include "platform/fd/SPEventProcess.cc"
#include "platform/fd/SPEventProcessFd.cc"
#include "platform/fd/SPEventFileFd.cc"
#endif

#if SPRT_WINDOWS
#include "platform/windows/SPEvent-windows.cc"
#include "platform/windows/SPEvent-iocp.cc"
#include "platform/windows/SPEventTimerIocp.cc"
#include "platform/windows/SPEventTimerWin.cc"
#include "platform/windows/SPEventThreadIocp.cc"
#include "platform/windows/SPEventPollIocp.cc"
#include "platform/windows/SPEventProcessIocp.cc"
#include "platform/windows/SPEventFileIocp.cc"
#include "platform/windows/SPEventWatchIocp.cc"
#endif

#if SPRT_APPLE
#include "platform/fd/SPEventProcess.cc"
#include "platform/darwin/SPEvent-darwin.cc"
#include "platform/darwin/SPEvent-kqueue.cc"
#include "platform/darwin/SPEvent-runloop.cc"
#endif

// WebAssembly: a pure futex/timer-heap reactor (no fds). Completes Queue::Data
// with the wasm engine + the timer handle. Threads/sockets/processes/files are
// not wired yet (see wasm-dispatch-design).
#if SPRT_WASM
#include "platform/wasm/SPEvent-wasm.cc"
#endif

// NuttX: timer heap + atomic wakeup + CLOCK_MONOTONIC spin (no self-pipe —
// NuttX pipes fill and block). Drives xenolith hello (Looper + Director).
#if SPRT_NUTTX
#include "platform/nuttx/SPEvent-nuttx.cc"
#endif

#if SPRT_EMBOX
#include "platform/embox/SPEvent-embox.cc"
#endif

// Platform-neutral async file I/O (shared op-state machine + inline handle +
// QueueData::readFile/writeFile). The io_uring-native handle lives in
// SPEventFileFd.cc (Linux/Android only); the inline handle here serves every
// other backend.
#include "platform/fd/SPEventFile.cc"

// Portable stat-polling file-watch (a repeating reactor timer diffing stat
// snapshots) for backends without a native filesystem-notification primitive
// (CFRunLoop, wasm). Linux/Android use inotify, Windows uses
// ReadDirectoryChangesW, kqueue uses EVFILT_VNODE instead.
#include "platform/fd/SPEventStatWatch.cc"

// Platform-neutral stream-socket API (shared state machine + SocketAddress +
// QueueData::listenSocket/connectSocket + the portable probe poller). Each
// backend contributes a readiness poll (QueueData::_socketPoll) and may
// override the strategy with native handles (_makeSocketListen/_makeSocketStream
// - io_uring below); wasm keeps everything null and the factories return
// nullptr. Compiles against the ENOSYS socket stubs on wasm but is never
// invoked there.
#include "platform/fd/SPEventSocket.cc"

#if SPRT_LINUX || SPRT_ANDROID
// io_uring-native socket strategy (ACCEPT/RECV/SEND SQEs); uses helpers from
// SPEventSocket.cc, so it must follow it in this SCU
#include "platform/fd/SPEventSocketFd.cc"
#endif

#if SPRT_WINDOWS
// WSAEventSelect readiness adapter + IOCP-native overlapped stream strategy;
// uses helpers from SPEventSocket.cc, so it must follow it in this SCU
#include "platform/windows/SPEventSocketIocp.cc"
#endif

#include "detail/SPRuntimeDispatchHandleClass.cc"
#include "detail/SPRuntimeDispatchQueueData.cc"
#include "SPRuntimeDispatchHandle.cc"
#include "SPRuntimeDispatchQueue.cc"
#include "SPRuntimeDispatchLooper.cc"
#include "SPRuntimeDispatchBus.cc"
