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

#include "SPRuntimeUuid.h"
#include "SPRuntimePlatform.h"
#include "SPRuntimeHash.h"

#include <c/__sprt_unistd.h>
#include <c/__sprt_string.h>

#if SPRT_WINDOWS
#include <winsock.h>
#endif

namespace sprt {

struct UuidState {
	UuidState() {
		struct {
			__sprt_pid_t pid;
			size_t threadId;
			uint64_t time;
			char hostname[257];
		} r;

#if SPRT_WINDOWS
		r.pid = GetCurrentProcessId();
#else
		r.pid = __sprt_getpid();
#endif

		r.time = sprt::platform::clock();
		r.threadId = __sprt_gettid();

		__sprt_gethostname(r.hostname, 256);

		sha256::Ctx ctx;
		sha256::sha_init(ctx);
		sha256::sha_process(ctx, (const uint8_t *)&r, sizeof(r));
		sha256::sha_done(ctx, node);
	}

	int seqnum = 0;
	uint8_t node[sha256::Length];
};

static thread_local UuidState tl_uuidState;

static uint64_t getCurrentTime() {
	// time magic to convert from epoch to UUID UTC
	uint64_t time_now = (sprt::platform::clock() * 10) + 0x01B2'1DD2'1381'4000ULL;

	thread_local uint64_t time_last = 0;
	thread_local uint64_t fudge = 0;

	if (time_last != time_now) {
		if (time_last + fudge > time_now) {
			fudge = time_last + fudge - time_now + 1;
		} else {
			fudge = 0;
		}
		time_last = time_now;
	} else {
		++fudge;
	}

	return time_now + fudge;
}

void genuuid(uint8_t d[UuidSize]) {
	uint64_t timestamp = getCurrentTime();

	/* time_low, uint32 */
	d[3] = (unsigned char)timestamp;
	d[2] = (unsigned char)(timestamp >> 8);
	d[1] = (unsigned char)(timestamp >> 16);
	d[0] = (unsigned char)(timestamp >> 24);
	/* time_mid, uint16 */
	d[5] = (unsigned char)(timestamp >> 32);
	d[4] = (unsigned char)(timestamp >> 40);
	/* time_hi_and_version, uint16 */
	d[7] = (unsigned char)(timestamp >> 48);
	d[6] = (unsigned char)(((timestamp >> 56) & 0x0F) | 0x50);
	/* clock_seq_hi_and_reserved, uint8 */
	d[8] = (unsigned char)(((tl_uuidState.seqnum >> 8) & 0x3F) | 0x80);
	/* clock_seq_low, uint8 */
	d[9] = (unsigned char)tl_uuidState.seqnum;
	/* node, byte[6] */
	::__sprt_memcpy(&d[10], tl_uuidState.node, 6);
}

} // namespace sprt
