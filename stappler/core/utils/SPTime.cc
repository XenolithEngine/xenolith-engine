/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPTime.h"
#include "SPPlatform.h"

inline time_t _time() { return time(NULL); }

namespace STAPPLER_VERSIONIZED stappler {

using sp_time_t = uint32_t;

TimeInterval TimeInterval::Infinite(maxOf<uint64_t>());

TimeInterval TimeInterval::between(const Time &v1, const Time &v2) {
	if (v1 > v2) {
		return TimeInterval(v1._value - v2._value);
	} else {
		return TimeInterval(v2._value - v1._value);
	}
}

uint64_t TimeStorage::toMicroseconds() const { return _value; }
uint64_t TimeStorage::toMilliseconds() const { return _value / 1'000ULL; }
uint64_t TimeStorage::toSeconds() const { return _value / 1'000'000ULL; }
float TimeStorage::toFloatSeconds() const { return _value / 1000000.0f; }
double TimeStorage::toDoubleSeconds() const { return _value / 1000000.0; }

tm TimeStorage::asLocal() const {
	auto sec = time_t(toSeconds());
	tm tm;
	localtime_r(&sec, &tm);
	return tm;
}

tm TimeStorage::asGmt() const {
	auto sec = time_t(toSeconds());
	tm tm;
	gmtime_r(&sec, &tm);
	return tm;
}

void TimeStorage::setMicroseconds(uint64_t v) { _value = v; }
void TimeStorage::setMilliseconds(uint64_t v) { _value = v * 1'000ULL; }
void TimeStorage::setSeconds(time_t v) { _value = v * 1'000'000ULL; }

void TimeStorage::clear() { _value = 0; }

TimeInterval::TimeInterval(nullptr_t) { _value = 0; }
TimeInterval &TimeInterval::operator=(nullptr_t) {
	_value = 0;
	return *this;
}


Time Time::now() { return Time(platform::clock(ClockType::Monotonic)); }

Time Time::microseconds(uint64_t mksec) { return Time(mksec); }
Time Time::milliseconds(uint64_t msec) { return Time(msec * 1'000ULL); }
Time Time::seconds(time_t sec) { return Time(sec * 1'000'000ULL); }
Time Time::floatSeconds(float sec) { return Time(uint64_t(sec * 1000000.0f)); }

Time::Time(nullptr_t) : TimeStorage(0) { }
Time &Time::operator=(nullptr_t) {
	_value = 0;
	return *this;
}

static const int s_months[12] = {('J' << 16) | ('a' << 8) | 'n', ('F' << 16) | ('e' << 8) | 'b',
	('M' << 16) | ('a' << 8) | 'r', ('A' << 16) | ('p' << 8) | 'r', ('M' << 16) | ('a' << 8) | 'y',
	('J' << 16) | ('u' << 8) | 'n', ('J' << 16) | ('u' << 8) | 'l', ('A' << 16) | ('u' << 8) | 'g',
	('S' << 16) | ('e' << 8) | 'p', ('O' << 16) | ('c' << 8) | 't', ('N' << 16) | ('o' << 8) | 'v',
	('D' << 16) | ('e' << 8) | 'c'};

Time Time::fromCompileTime(const char *date, const char *time) {
	sprt::time::time_exp_t ds;
	ds.tm_year = ((date[7] - '0') * 10 + (date[8] - '0') - 19) * 100;
	if (ds.tm_year < 0) {
		return Time();
	}

	ds.tm_year += ((date[9] - '0') * 10) + (date[10] - '0');
	ds.tm_mday = ((date[4] != ' ') ? ((date[4] - '0') * 10) : 0) + (date[5] - '0');

	int mint = (date[0] << 16) | (date[1] << 8) | date[2];
	int mon = 0;
	for (; mon < 12; mon++) {
		if (mint == s_months[mon]) {
			break;
		}
	}

	if (mon == 12) {
		return Time();
	}

	if (ds.tm_mday <= 0 || ds.tm_mday > 31) {
		return Time();
	}

	if ((ds.tm_mday == 31) && (mon == 3 || mon == 5 || mon == 8 || mon == 10)) {
		return Time();
	}

	if ((mon == 1)
			&& ((ds.tm_mday > 29)
					|| ((ds.tm_mday == 29)
							&& ((ds.tm_year & 3)
									|| (((ds.tm_year % 100) == 0)
											&& (((ds.tm_year % 400) != 100))))))) {
		return Time();
	}

	ds.tm_mon = mon;
	ds.tm_hour = ((time[0] - '0') * 10) + (time[1] - '0');
	ds.tm_min = ((time[3] - '0') * 10) + (time[4] - '0');
	ds.tm_sec = ((time[6] - '0') * 10) + (time[7] - '0');

	if ((ds.tm_hour > 23) || (ds.tm_min > 59) || (ds.tm_sec > 61)) {
		return Time();
	}

	ds.tm_usec = 0;
	ds.tm_gmtoff = 0;
	return Time::microseconds(ds.ltz_geti());
}

Time Time::fromHttp(StringView r) {
	sprt::time::time_exp_t ds;
	if (!ds.read(r)) {
		return Time();
	}

	switch (ds.tm_gmt_type) {
	case sprt::time::time_exp_t::gmt_set: return Time::microseconds(ds.gmt_geti());
	case sprt::time::time_exp_t::gmt_local: return Time::microseconds(ds.ltz_geti());
	case sprt::time::time_exp_t::gmt_unset: return Time::microseconds(ds.geti());
	}

	return Time();
}

size_t Time::encodeToFormat(char *buf, size_t bufSize, const char *fmt) const {
	return sprt::time::strftime(buf, bufSize, fmt, toMicroseconds());
}

} // namespace STAPPLER_VERSIONIZED stappler
