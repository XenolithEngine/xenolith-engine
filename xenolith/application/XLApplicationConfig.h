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

#ifndef XENOLITH_APPLICATION_XLAPPLICATIONCONFIG_H_
#define XENOLITH_APPLICATION_XLAPPLICATIONCONFIG_H_

#include "XLCoreConfig.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::config {

static constexpr size_t NodePreallocateChilds = 4;

static constexpr size_t MaxMaterialImages = 4;

static constexpr uint32_t MaxAmbientLights = 16;
static constexpr uint32_t MaxDirectLights = 16;

#if DEBUG
static constexpr uint64_t MaxDirectorDeltaTime = 10'000'000 / 16;
#else
static constexpr uint64_t MaxDirectorDeltaTime = 100'000'000 / 16;
#endif

#if THREAD_DEBUG
static inline uint16_t getDefaultMainThreads() { return 2; }
static inline uint16_t getDefaultAppThreads() { return 2; }
#else
static inline uint16_t getDefaultMainThreads() {
	return static_cast<uint16_t>(sprt::thread::hardware_concurrency()) / 2;
}
static inline uint16_t getDefaultAppThreads() {
	return static_cast<uint16_t>(sprt::thread::hardware_concurrency()) / 2;
}
#endif

/* Remote session timers, in microseconds. */

// The server pings the client this often; each side drops a peer silent for its timeout.
static constexpr uint64_t RemoteKeepalivePingInterval = 1'000'000;

// Setup handshake: its deadline, how long a refused connection may send its hello before the
// refusal is written anyway, and the cool-off window that doubles with every failure.
static constexpr uint64_t RemoteHandshakeDeadline = 2'000'000;
static constexpr uint64_t RemoteRefusalHelloWait = 100'000;
static constexpr uint64_t RemoteHandshakeBackoffBase = 250'000;
static constexpr uint64_t RemoteHandshakeBackoffMax = 8'000'000;

// Reply deadlines; a reply past its deadline fails the waiter and resets the connection.
static constexpr uint64_t RemotePeerInfoReplyTimeout = 5'000'000;
static constexpr uint64_t RemoteAnnounceReplyTimeout = 5'000'000;
static constexpr uint64_t RemoteAttachQueueReplyTimeout = 5'000'000;
static constexpr uint64_t RemoteWindowControlReplyTimeout = 5'000'000;
static constexpr uint64_t RemoteCreateWindowReplyTimeout = 10'000'000;
static constexpr uint64_t RemoteCompileQueueReplyTimeout = 15'000'000;

// How long a client may take to commit a frame's input once it answered AcquireFrame. Generous: it
// guards against a client that stopped mid-frame, and the first frames take seconds of round trips.
static constexpr uint64_t RemoteFrameInputDeadline = 10'000'000;

#if SPRT_TAU
// Tau peers run under TCG, where image decoding and the first frames take minutes.
static constexpr uint64_t RemoteKeepalivePingTimeout = 120'000'000;
static constexpr uint64_t RemoteKeepalivePongTimeout = 120'000'000;
static constexpr uint64_t RemoteAcquireFrameReplyTimeout = 120'000'000;
static constexpr uint64_t RemoteFontSourcesReplyTimeout = 120'000'000;
#else
static constexpr uint64_t RemoteKeepalivePingTimeout = 5'000'000;
static constexpr uint64_t RemoteKeepalivePongTimeout = 5'000'000;
static constexpr uint64_t RemoteAcquireFrameReplyTimeout = 2'000'000;
static constexpr uint64_t RemoteFontSourcesReplyTimeout = 10'000'000;
#endif

} // namespace stappler::xenolith::config

#endif /* XENOLITH_APPLICATION_XLAPPLICATIONCONFIG_H_ */
