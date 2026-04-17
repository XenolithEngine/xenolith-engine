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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_TIME_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_TIME_H_

#include <sprt/runtime/init.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/__sprt_time.h>

namespace sprt::time {

static constexpr uint64_t USEC_PER_SEC(1'000'000);

struct SPRT_API time_exp_t : public __SPRT_TM_NAME {
	static constexpr size_t Rfc822BufferSize = 32;
	static constexpr size_t CTimeBufferSize = 25;
	static constexpr size_t Iso8601BufferSize = 33; // with max 6 precision signs

	static constexpr auto gmt_unset = __SPRT_ID(gmt_unset);
	static constexpr auto gmt_local = __SPRT_ID(gmt_local);
	static constexpr auto gmt_set = __SPRT_ID(gmt_set);

	static time_exp_t get(bool localtime = false);

	time_exp_t();

	// Convert UTC time in microseconds to tm
	time_exp_t(int64_t t);

	// Convert UTC time in microseconds to tm in UTC or local timezone
	time_exp_t(int64_t t, bool use_localtime);

	// Convert UTC time in microseconds to tm in UTC or local timezone, then set GMT offet to value specified
	time_exp_t(int64_t t, int32_t offset_seconds, bool use_localtime);

	// Convert UTC time in microseconds to tm in UTC, then set GMT offet to value specified
	time_exp_t(int64_t t, int32_t offset_seconds);


	/*
	 * Parses an HTTP date in one of three standard forms:
	 *
	 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
	 *     2011-04-28T06:34:00+09:00      ; Atom time format
	 */
	time_exp_t(StringView);

	bool read(StringView);

	int64_t geti() const;
	int64_t gmt_geti() const;
	int64_t ltz_geti() const;

	size_t encodeRfc822(char *, size_t) const;
	size_t encodeCTime(char *, size_t) const;
	size_t encodeIso8601(char *, size_t, size_t precision) const;
};


class SPRT_API Time;

class SPRT_API TimeStorage {
public:
	uint64_t toMicroseconds() const;
	uint64_t toMilliseconds() const;
	uint64_t toSeconds() const;
	float toFloatSeconds() const;
	double toDoubleSeconds() const;

	uint64_t toMicros() const { return toMicroseconds(); }
	uint64_t toMillis() const { return toMilliseconds(); }

	uint64_t mksec() const { return toMicroseconds(); }
	uint64_t msec() const { return toMilliseconds(); }
	uint64_t sec() const { return toSeconds(); }
	float fsec() const { return toFloatSeconds(); }

	tm asLocal() const;
	tm asGmt() const;

	void setMicros(uint64_t value) { setMicroseconds(value); }
	void setMillis(uint64_t value) { setMilliseconds(value); }

	void setMicroseconds(uint64_t);
	void setMilliseconds(uint64_t);
	void setSeconds(sprt::time_t);

	void clear();

	constexpr explicit operator bool() const noexcept { return _value != 0; }

	TimeStorage() = default;
	TimeStorage(const TimeStorage &) = default;
	TimeStorage(TimeStorage &&) = default;
	TimeStorage &operator=(const TimeStorage &) = default;
	TimeStorage &operator=(TimeStorage &&) = default;

	constexpr TimeStorage(uint64_t v) : _value(v) { }

protected:
	uint64_t _value = 0;
};

class SPRT_API TimeInterval : public TimeStorage {
public:
	static TimeInterval Infinite;

	static TimeInterval between(const Time &, const Time &);

	constexpr static TimeInterval microseconds(uint64_t mksec) { return TimeInterval(mksec); }
	constexpr static TimeInterval milliseconds(uint64_t msec) {
		return TimeInterval(msec * 1'000ULL);
	}
	constexpr static TimeInterval seconds(sprt::time_t sec) {
		return TimeInterval(sec * 1'000'000ULL);
	}
	constexpr static TimeInterval floatSeconds(float sec) {
		return TimeInterval(uint64_t(sec * 1000000.0f));
	}

	inline const Time operator+(const Time &v) const;

	inline const TimeInterval operator+(const TimeInterval &v) const;
	inline TimeInterval &operator+=(const TimeInterval &v);

	inline const TimeInterval operator-(const TimeInterval &v) const;
	inline TimeInterval &operator-=(const TimeInterval &v);

	inline const TimeInterval operator*(float s) const;
	inline TimeInterval &operator*=(float s);

	inline const TimeInterval operator/(float s) const;
	inline TimeInterval &operator/=(float s);

	inline bool operator<(const TimeInterval &v) const;
	inline bool operator>(const TimeInterval &v) const;
	inline bool operator<=(const TimeInterval &v) const;
	inline bool operator>=(const TimeInterval &v) const;
	inline bool operator==(const TimeInterval &v) const;
	inline bool operator!=(const TimeInterval &v) const;

	TimeInterval(nullptr_t);
	TimeInterval &operator=(nullptr_t);

	constexpr TimeInterval() : TimeStorage(0) { }
	constexpr TimeInterval(const TimeInterval &other) = default;
	constexpr TimeInterval(TimeInterval &&other) = default;
	constexpr TimeInterval &operator=(const TimeInterval &other) = default;
	constexpr TimeInterval &operator=(TimeInterval &&other) = default;

protected:
	friend class Time;

	using TimeStorage::TimeStorage;
};

class SPRT_API Time : public TimeStorage {
public:
	static Time now();

	static Time fromCompileTime(const char *, const char *);

	/*
	 * Parses an HTTP date in one of three standard forms:
	 *
	 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
	 *     2011-04-28T06:34:00+09:00      ; Atom time format
	 */
	static Time fromHttp(StringView);

	static Time microseconds(uint64_t mksec);
	static Time milliseconds(uint64_t msec);
	static Time seconds(sprt::time_t sec);
	static Time floatSeconds(float sec);

	template <typename StringType>
	auto toHttp() const -> StringType {
		return toRfc822<StringType>();
	}

	template <typename StringType>
	auto toAtomXml() const -> StringType {
		return toIso8601<StringType>(0);
	}

	template <typename StringType>
	auto toRfc822() const -> StringType {
		sprt::time::time_exp_t xt(this->toMicroseconds());
		char buf[sprt::time::time_exp_t::Rfc822BufferSize] = {0};
		return StringType(buf, xt.encodeRfc822(buf, sprt::time::time_exp_t::Rfc822BufferSize));
	}

	template <typename StringType>
	auto toCTime() const -> StringType {
		sprt::time::time_exp_t xt(this->toMicroseconds(), true);
		char buf[sprt::time::time_exp_t::CTimeBufferSize] = {0};
		return StringType(buf, xt.encodeCTime(buf, sprt::time::time_exp_t::CTimeBufferSize));
	}

	// ISO 8601 dateTime format, used by XML/Atom
	// - YYYY-MM-DDThh:mm:ss[.sss]
	// precision defined as digit after decimal sep
	// min - 0 - no decimal
	//       3 - milliseconds precision
	// max - 6 - microseconds precision
	template <typename StringType>
	auto toIso8601(size_t precision = 0) const -> StringType {
		sprt::time::time_exp_t xt(this->toMicroseconds(), false);
		char buf[sprt::time::time_exp_t::Iso8601BufferSize] = {0};
		return StringType(buf,
				xt.encodeIso8601(buf, sprt::time::time_exp_t::Iso8601BufferSize, precision));
	}

	template <typename StringType>
	auto toFormat(const char *fmt) const -> StringType {
		char buf[256] = {0}; // should be enough
		return StringType(buf, encodeToFormat(buf, 256, fmt));
	}

	inline const Time operator+(const TimeInterval &v) const;
	inline Time &operator+=(const TimeInterval &v);

	inline const TimeInterval operator-(const Time &v) const;
	inline const Time operator-(const TimeInterval &v) const;
	inline Time &operator-=(const TimeInterval &v);

	inline bool operator<(const Time &v) const;
	inline bool operator>(const Time &v) const;
	inline bool operator<=(const Time &v) const;
	inline bool operator>=(const Time &v) const;
	inline bool operator==(const Time &v) const;
	inline bool operator!=(const Time &v) const;

	Time(nullptr_t);
	Time &operator=(nullptr_t);

	Time() : TimeStorage(0) { }
	Time(const Time &other) : TimeStorage(other._value) { }
	Time(Time &&other) : TimeStorage(other._value) { }
	Time &operator=(const Time &other) {
		_value = other._value;
		return *this;
	}
	Time &operator=(Time &&other) {
		_value = other._value;
		return *this;
	}

	size_t encodeToFormat(char *, size_t, const char *fmt) const;

protected:
	friend class TimeInterval;

	using TimeStorage::TimeStorage;
};

constexpr TimeInterval operator""_sec(unsigned long long int val) {
	return TimeInterval::seconds((sprt::time_t)val);
}
constexpr TimeInterval operator""_msec(unsigned long long int val) {
	return TimeInterval::milliseconds(val);
}
constexpr TimeInterval operator""_mksec(unsigned long long int val) {
	return TimeInterval::microseconds(val);
}

inline const Time TimeInterval::operator+(const Time &v) const { return v + *this; }

inline const TimeInterval TimeInterval::operator+(const TimeInterval &v) const {
	return TimeInterval(_value + v._value);
}
inline TimeInterval &TimeInterval::operator+=(const TimeInterval &v) {
	_value += v._value;
	return *this;
}

inline const TimeInterval TimeInterval::operator-(const TimeInterval &v) const {
	if (_value < v._value) {
		return TimeInterval();
	} else {
		return TimeInterval(_value - v._value);
	}
}
inline TimeInterval &TimeInterval::operator-=(const TimeInterval &v) {
	if (_value < v._value) {
		_value = 0;
	} else {
		_value -= v._value;
	}
	return *this;
}

inline const TimeInterval TimeInterval::operator*(float s) const {
	return TimeInterval(_value * sprt::fabsf(s));
}
inline TimeInterval &TimeInterval::operator*=(float s) {
	_value *= sprt::fabsf(s);
	return *this;
}

inline const TimeInterval TimeInterval::operator/(float s) const {
	return TimeInterval(_value / sprt::fabsf(s));
}
inline TimeInterval &TimeInterval::operator/=(float s) {
	_value /= sprt::fabsf(s);
	return *this;
}

inline bool TimeInterval::operator<(const TimeInterval &v) const { return _value < v._value; }
inline bool TimeInterval::operator>(const TimeInterval &v) const { return _value > v._value; }
inline bool TimeInterval::operator<=(const TimeInterval &v) const { return _value <= v._value; }
inline bool TimeInterval::operator>=(const TimeInterval &v) const { return _value >= v._value; }
inline bool TimeInterval::operator==(const TimeInterval &v) const { return _value == v._value; }
inline bool TimeInterval::operator!=(const TimeInterval &v) const { return _value != v._value; }

inline const Time Time::operator+(const TimeInterval &v) const { return Time(_value + v._value); }
inline Time &Time::operator+=(const TimeInterval &v) {
	_value += v._value;
	return *this;
}

inline const TimeInterval Time::operator-(const Time &v) const {
	return TimeInterval::between(*this, v);
}
inline const Time Time::operator-(const TimeInterval &v) const {
	if (_value < v._value) {
		return Time();
	}
	return Time(_value - v._value);
}
inline Time &Time::operator-=(const TimeInterval &v) {
	if (_value < v._value) {
		_value = 0;
	}
	_value -= v._value;
	return *this;
}

inline bool Time::operator<(const Time &v) const { return _value < v._value; }
inline bool Time::operator>(const Time &v) const { return _value > v._value; }
inline bool Time::operator<=(const Time &v) const { return _value <= v._value; }
inline bool Time::operator>=(const Time &v) const { return _value >= v._value; }
inline bool Time::operator==(const Time &v) const { return _value == v._value; }
inline bool Time::operator!=(const Time &v) const { return _value != v._value; }

SPRT_API size_t strftime(char *buf, size_t maxsize, const char *format, uint64_t usec);

} // namespace sprt::time

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_TIME_H_
