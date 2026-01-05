/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "SPMemPriorityQueue.h"
#include "SPMemUuid.h"
#include "SPString.h"
#include <sprt/runtime/uuid.h>

namespace STAPPLER_VERSIONIZED stappler::memory {

SP_PUBLIC void PriorityQueue_lock_noOp(void *) {
	// no-op, really!
}

SP_PUBLIC void PriorityQueue_lock_std_mutex(void *ptr) {
	std::mutex *mutex = (std::mutex *)ptr;
	mutex->lock();
}

SP_PUBLIC void PriorityQueue_unlock_std_mutex(void *ptr) {
	std::mutex *mutex = (std::mutex *)ptr;
	mutex->unlock();
}

SP_PUBLIC void PriorityQueue_lock_qmutex(void *ptr) {
	sprt::qmutex *mutex = (sprt::qmutex *)ptr;
	mutex->lock();
}

SP_PUBLIC void PriorityQueue_unlock_qmutex(void *ptr) {
	sprt::qmutex *mutex = (sprt::qmutex *)ptr;
	mutex->unlock();
}

uuid uuid::generate() {
	uuid_t d;
	sprt::genuuid(d.data());
	return uuid(d);
}

void uuid::format(char *buf, const uuid_t &d) {
	snprintf(buf, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", d[0],
			d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14],
			d[15]);
}

static uint8_t parse_hexpair(const char *s) {
	int result;
	int temp;

	result = s[0] - '0';
	if (result > 48) {
		result = (result - 39) << 4;
	} else if (result > 16) {
		result = (result - 7) << 4;
	} else {
		result = result << 4;
	}

	temp = s[1] - '0';
	if (temp > 48) {
		result |= temp - 39;
	} else if (temp > 16) {
		result |= temp - 7;
	} else {
		result |= temp;
	}

	return (uint8_t)result;
}

bool uuid::parse(uuid_t &d, StringView str) {
	size_t i;

	if (str.size() < FormattedLength) {
		return false;
	}

	auto uuid_str = str.data();

	for (i = 0; i < FormattedLength; ++i) {
		char c = uuid_str[i];
		if (!isxdigit(c) && !(c == '-' && (i == 8 || i == 13 || i == 18 || i == 23))) {
			return false;
		}
	}

	d[0] = base16::hexToChar(uuid_str[0], uuid_str[1]);
	d[1] = base16::hexToChar(uuid_str[2], uuid_str[3]);
	d[2] = base16::hexToChar(uuid_str[4], uuid_str[5]);
	d[3] = base16::hexToChar(uuid_str[6], uuid_str[7]);

	d[4] = base16::hexToChar(uuid_str[9], uuid_str[10]);
	d[5] = base16::hexToChar(uuid_str[11], uuid_str[12]);

	d[6] = base16::hexToChar(uuid_str[14], uuid_str[15]);
	d[7] = base16::hexToChar(uuid_str[16], uuid_str[17]);

	d[8] = base16::hexToChar(uuid_str[19], uuid_str[20]);
	d[9] = base16::hexToChar(uuid_str[21], uuid_str[22]);

	for (i = 6; i--;) { d[10 + i] = parse_hexpair(&uuid_str[i * 2 + 24]); }

	return true;
}

} // namespace stappler::memory
