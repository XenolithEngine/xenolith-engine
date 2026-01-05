/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_UTILS_SPLOG_H_
#define STAPPLER_CORE_UTILS_SPLOG_H_

#include "SPString.h"
#include "SPStringView.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::log {

struct SP_PUBLIC CustomLog {
	union VA {
		StringView text;
		struct {
			const char *format = nullptr;
			va_list args;
		} format;

		VA() { }
	};

	enum Type {
		Text,
		Format
	};

	using log_fn = bool (*)(LogType, StringView, const sprt::source_location &, Type, VA &);

	CustomLog(log_fn fn);
	~CustomLog();

	CustomLog(const CustomLog &) = delete;
	CustomLog &operator=(const CustomLog &) = delete;

	CustomLog(CustomLog &&);
	CustomLog &operator=(CustomLog &&);

	log_fn fn;
	void *manager;
};

extern std::bitset<6> None;
extern std::bitset<6> ErrorsOnly;
extern std::bitset<6> Full;

// log is suppressed if bit is set
// only default logger is affected
SP_PUBLIC void setLogFilterMask(const std::bitset<6> &);
SP_PUBLIC void setLogFilterMask(std::bitset<6> &&);
SP_PUBLIC void setLogFilterMask(InitializerList<LogType>);
SP_PUBLIC std::bitset<6> getlogFilterMask();

SP_PUBLIC void format(LogType, StringView tag, const sprt::source_location &, const char *, ...)
		SPPRINTF(4, 5);
SP_PUBLIC void text(LogType, StringView tag, StringView,
		const sprt::source_location & = __SPRT_LOCATION);

template <typename... Args>
void verbose(StringView tag, Args &&...args) {
	text(LogType::Verbose, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
			sprt::source_location());
}

template <typename... Args>
void debug(StringView tag, Args &&...args) {
	text(LogType::Debug, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
			sprt::source_location());
}

template <typename... Args>
void info(StringView tag, Args &&...args) {
	text(LogType::Info, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
			sprt::source_location());
}

template <typename... Args>
void warn(StringView tag, Args &&...args) {
	text(LogType::Warn, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
			sprt::source_location());
}

template <typename... Args>
void error(StringView tag, Args &&...args) {
	text(LogType::Error, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
			sprt::source_location());
}

template <typename... Args>
void fatal(StringView tag, Args &&...args) {
	text(LogType::Fatal, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
			sprt::source_location());
}

// Wrap current source location to forward it into log entry
struct SP_PUBLIC SourceWapper {
	constexpr SourceWapper(const sprt::source_location &loc = __SPRT_LOCATION) : source(loc) { }

	template <typename... Args>
	void verbose(StringView tag, Args &&...args) {
		text(LogType::Verbose, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
				source);
	}

	template <typename... Args>
	void debug(StringView tag, Args &&...args) {
		text(LogType::Debug, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
				source);
	}

	template <typename... Args>
	void info(StringView tag, Args &&...args) {
		text(LogType::Info, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
				source);
	}

	template <typename... Args>
	void warn(StringView tag, Args &&...args) {
		text(LogType::Warn, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
				source);
	}

	template <typename... Args>
	void error(StringView tag, Args &&...args) {
		text(LogType::Error, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
				source);
	}

	template <typename... Args>
	void fatal(StringView tag, Args &&...args) {
		text(LogType::Fatal, tag, StringView(mem_std::toString(std::forward<Args>(args)...)),
				source);
	}

	sprt::source_location source;
};

SP_PUBLIC inline SourceWapper source(const sprt::source_location &loc = __SPRT_LOCATION) {
	return SourceWapper(loc);
}

} // namespace stappler::log

namespace STAPPLER_VERSIONIZED stappler {

SP_PUBLIC inline auto plog() { return log::SourceWapper(sprt::source_location()); }

SP_PUBLIC inline auto slog(const sprt::source_location &loc = __SPRT_LOCATION) {
	return log::SourceWapper(loc);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_UTILS_SPLOG_H_ */
