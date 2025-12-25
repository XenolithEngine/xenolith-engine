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

#include "SPRuntimeLog.h"
#include "SPRuntimeStringBuffer.h"
#include <c/__sprt_stdlib.h>
#include <stdio.h>

#if SPRT_ANDROID
#include <android/log.h>
#endif

#if SPRT_LINUX
#include <c/sys/__sprt_stat.h>
#endif

#if SPRT_WINDOWS
#include "private/SPUnistd.h" // IWYU pragma: keep
#endif

namespace sprt::log {

struct LogFeaturesInit : LogFeatures {
	char tmpbuf[256] = {0};
	char *tmpTarget = nullptr;

	LogFeaturesInit();

	StringView copy(StringView str);
};

#if SPRT_LINUX

static bool parseLogFeatures(BytesView data, LogFeaturesInit &ret) {
	static constexpr uint32_t numbers_max_colors = 13;
	static constexpr uint16_t strings_enter_bold_mode = 27;
	static constexpr uint16_t strings_enter_dim_mode = 30;
	static constexpr uint16_t string_exit_attribute_mode = 39;
	static constexpr uint16_t strings_enter_reverse_mode = 34;
	static constexpr uint16_t strings_enter_underline_mode = 36;
	static constexpr uint16_t strings_enter_italics_mode = 311;

	StringView names;
	SpanView<int8_t> bools;
	SpanView<int16_t> numbers16;
	SpanView<int32_t> numbers32;
	SpanView<uint16_t> stringOffsets;
	BytesView strings;

	uint32_t offset = 0;
	bool useI32 = false;
	auto header = data.readSpan<uint16_t>(6);
	if (header.at(0) == 0x021e) {
		useI32 = true;
	} else if (header.at(0) != 0x011A) {
		return false;
	}

	auto namesBytes = header.at(1) == Max<uint16_t> ? 0 : header.at(1);
	auto boolsBytes = header.at(2) == Max<uint16_t> ? 0 : header.at(2);
	auto numbersCount = header.at(3) == Max<uint16_t> ? 0 : header.at(3);
	auto stringOffsetsCount = header.at(4) == Max<uint16_t> ? 0 : header.at(4);
	auto stringTableBytes = header.at(5) == Max<uint16_t> ? 0 : header.at(5);

	names = BytesView(data).readString(namesBytes);
	offset += namesBytes;

	bools = BytesView(data.data() + offset, boolsBytes).readSpan<int8_t>(boolsBytes);
	offset += boolsBytes;

	if (offset % 2 != 0) {
		++offset;
	}

	if (useI32) {
		numbers32 = BytesView(data.data() + offset, numbersCount * sizeof(int32_t))
							.readSpan<int32_t>(numbersCount);
		offset += numbersCount * sizeof(int32_t);
	} else {
		numbers16 = BytesView(data.data() + offset, numbersCount * sizeof(int16_t))
							.readSpan<int16_t>(numbersCount);
		offset += numbersCount * sizeof(int16_t);
	}

	stringOffsets = BytesView(data.data() + offset, stringOffsetsCount * sizeof(uint16_t))
							.readSpan<uint16_t>(stringOffsetsCount);
	offset += stringOffsetsCount * sizeof(uint16_t);

	strings = BytesView(data.data() + offset, stringTableBytes);
	offset += stringTableBytes;

	if (data.size() < offset) {
		return false;
	}

	auto readInteger = [&](uint16_t index) -> int32_t {
		if (index > numbersCount) {
			return 0;
		}
		if (useI32) {
			return numbers32.at(index);
		} else {
			return numbers16.at(index);
		}
	};

	// not used for now, maybe layer?
	SPRT_UNUSED auto readBool = [&](uint16_t index) -> bool {
		if (index > boolsBytes) {
			return false;
		}
		return bools.at(index) > 0;
	};

	auto readString = [&](uint16_t index) -> StringView {
		if (index > stringOffsetsCount) {
			return StringView();
		}
		auto offset = stringOffsets.at(index);
		if (offset < strings.size() && offset != Max<uint16_t>) {
			auto d = strings;
			d.offset(offset);
			return d.readString();
		}

		return StringView();
	};

	auto ncolors = readInteger(numbers_max_colors);
	auto bold = readString(strings_enter_bold_mode);
	auto dim = readString(strings_enter_dim_mode);
	auto underline = readString(strings_enter_underline_mode);
	auto italic = readString(strings_enter_italics_mode);
	auto reverse = readString(strings_enter_reverse_mode);
	auto drop = readString(string_exit_attribute_mode);

	if (!drop.empty()) {
		ret.drop = ret.copy(drop);
		if (__sprt_memcmp(ret.drop.data(), "\033(", 2) == 0) {
			ret.features |= LogFeatures::AnsiCompatible;
		}
	}
	if (ncolors >= 8) {
		ret.features |= LogFeatures::Colors;
		ret.ncolors = ncolors;

		if ((ret.features & LogFeatures::AnsiCompatible) != LogFeatures::None) {
			ret.fblack = StringView("\033[30m");
			ret.fred = StringView("\033[31m");
			ret.fgreen = StringView("\033[32m");
			ret.fyellow = StringView("\033[33m");
			ret.fblue = StringView("\033[34m");
			ret.fmagenta = StringView("\033[35m");
			ret.fcyan = StringView("\033[36m");
			ret.fwhite = StringView("\033[37m");
			ret.fdef = StringView("\033[39m");

			ret.bblack = StringView("\033[40m");
			ret.bred = StringView("\033[41m");
			ret.bgreen = StringView("\033[42m");
			ret.byellow = StringView("\033[43m");
			ret.bblue = StringView("\033[44m");
			ret.bmagenta = StringView("\033[45m");
			ret.bcyan = StringView("\033[46m");
			ret.bwhite = StringView("\033[47m");
			ret.bdef = StringView("\033[49m");
		}
	}
	if (!bold.empty() && !drop.empty()) {
		ret.features |= LogFeatures::Bold;
		ret.bold = ret.copy(bold);
	}
	if (!italic.empty() && !drop.empty()) {
		ret.features |= LogFeatures::Italic;
		ret.italic = ret.copy(italic);
	}
	if (!underline.empty() && !drop.empty()) {
		ret.features |= LogFeatures::Underline;
		ret.underline = ret.copy(underline);
	}
	if (!reverse.empty() && !drop.empty()) {
		ret.features |= LogFeatures::Reverse;
		ret.reverse = ret.copy(reverse);
	}
	if (!dim.empty() && !drop.empty()) {
		ret.features |= LogFeatures::Dim;
		ret.dim = ret.copy(dim);
	}

	return true;
}

static bool checkLogFeatureWithFilename(StringView str, LogFeaturesInit &ret) {
	bool result = false;
	struct stat s;
	auto r = ::__sprt_stat(str.data(), &s);
	if (r == 0 && s.st_size > 0) {
		uint8_t buf[s.st_size];
		auto f = fopen(str.data(), "r");
		if (f) {
			if (fread(buf, s.st_size, 1, f) > 0) {
				result = parseLogFeatures(BytesView(buf, s.st_size), ret);
			}
			fclose(f);
		}
	}
	return result;
}

static void checkLogFeaturesSupport(LogFeaturesInit &result) {
	char buf[512] = {0};
	auto envTerm = ::__sprt_getenv("TERM");
	auto envTermInfo = ::__sprt_getenv("TERMINFO");
	if (envTerm) {
		if (envTermInfo) {
			::__sprt_memset(buf, 0, 512);
			size_t targetSize = 512;
			auto target = buf;

			target = strappend(target, &targetSize, envTermInfo, ::__sprt_strlen(envTermInfo));
			target = strappend(target, &targetSize, "/", 1);
			target = strappend(target, &targetSize, envTerm, 1);
			target = strappend(target, &targetSize, "/", 1);
			target = strappend(target, &targetSize, envTerm, ::__sprt_strlen(envTerm));

			if (targetSize > 1) {
				if (checkLogFeatureWithFilename(StringView(buf, target - buf), result)) {
					return;
				}
			}
		}

		::__sprt_memset(buf, 0, 512);

		size_t targetSize = 512;
		auto target = buf;

		target =
				strappend(target, &targetSize, "/etc/terminfo/", ::__sprt_strlen("/etc/terminfo/"));
		target = strappend(target, &targetSize, envTerm, 1);
		target = strappend(target, &targetSize, "/", 1);
		target = strappend(target, &targetSize, envTerm, ::__sprt_strlen(envTerm));

		if (targetSize > 1) {
			if (checkLogFeatureWithFilename(StringView(buf, target - buf), result)) {
				return;
			}
		}

		::__sprt_memset(buf, 0, 512);

		targetSize = 512;
		target = buf;

		target = strappend(target, &targetSize, "/usr/lib/terminfo/",
				::__sprt_strlen("/usr/lib/terminfo/"));
		target = strappend(target, &targetSize, envTerm, 1);
		target = strappend(target, &targetSize, "/", 1);
		target = strappend(target, &targetSize, envTerm, ::__sprt_strlen(envTerm));

		if (targetSize > 1) {
			if (checkLogFeatureWithFilename(StringView(buf, target - buf), result)) {
				return;
			}
		}

		::__sprt_memset(buf, 0, 512);

		targetSize = 512;
		target = buf;

		target = strappend(target, &targetSize, "/usr/share/terminfo/",
				::__sprt_strlen("/usr/share/terminfo/"));
		target = strappend(target, &targetSize, envTerm, 1);
		target = strappend(target, &targetSize, "/", 1);
		target = strappend(target, &targetSize, envTerm, ::__sprt_strlen(envTerm));

		if (targetSize > 1) {
			if (checkLogFeatureWithFilename(StringView(buf, target - buf), result)) {
				return;
			}
		}
	}
}
#endif

#if SPRT_MACOS

static void checkLogFeaturesSupport(LogFeaturesInit &ret) {
	ret.features = LogFeatures::AnsiCompatible | LogFeatures::Colors | LogFeatures::Bold
			| LogFeatures::Underline | LogFeatures::Italic | LogFeatures::Reverse
			| LogFeatures::Dim;

	ret.drop = StringView("\033[0m");
	ret.bold = StringView("\033[1m");
	ret.underline = StringView("\033[4m");
	ret.italic = StringView("\033[3m");
	ret.reverse = StringView("\033[7m");
	ret.dim = StringView("\033[2m");

	ret.fblack = StringView("\033[30m");
	ret.fred = StringView("\033[31m");
	ret.fgreen = StringView("\033[32m");
	ret.fyellow = StringView("\033[33m");
	ret.fblue = StringView("\033[34m");
	ret.fmagenta = StringView("\033[35m");
	ret.fcyan = StringView("\033[36m");
	ret.fwhite = StringView("\033[37m");
	ret.fdef = StringView("\033[39m");

	ret.bblack = StringView("\033[40m");
	ret.bred = StringView("\033[41m");
	ret.bgreen = StringView("\033[42m");
	ret.byellow = StringView("\033[43m");
	ret.bblue = StringView("\033[44m");
	ret.bmagenta = StringView("\033[45m");
	ret.bcyan = StringView("\033[46m");
	ret.bwhite = StringView("\033[47m");
	ret.bdef = StringView("\033[49m");
}

#endif

#if SPRT_WINDOWS

static void checkLogFeaturesSupport(LogFeaturesInit &ret) {
	/*auto hStdout = ::GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD mode = 0;
	if (::GetConsoleMode(hStdout, &mode)) {
		if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {*/
	ret.features = LogFeatures::AnsiCompatible | LogFeatures::Colors | LogFeatures::Bold
			| LogFeatures::Underline | LogFeatures::Italic | LogFeatures::Reverse
			| LogFeatures::Dim;

	ret.drop = StringView("\033[0m");
	ret.bold = StringView("\033[1m");
	ret.underline = StringView("\033[4m");
	ret.italic = StringView("\033[3m");
	ret.reverse = StringView("\033[7m");
	ret.dim = StringView("\033[2m");

	ret.fblack = StringView("\033[30m");
	ret.fred = StringView("\033[31m");
	ret.fgreen = StringView("\033[32m");
	ret.fyellow = StringView("\033[33m");
	ret.fblue = StringView("\033[34m");
	ret.fmagenta = StringView("\033[35m");
	ret.fcyan = StringView("\033[36m");
	ret.fwhite = StringView("\033[37m");
	ret.fdef = StringView("\033[39m");

	ret.bblack = StringView("\033[40m");
	ret.bred = StringView("\033[41m");
	ret.bgreen = StringView("\033[42m");
	ret.byellow = StringView("\033[43m");
	ret.bblue = StringView("\033[44m");
	ret.bmagenta = StringView("\033[45m");
	ret.bcyan = StringView("\033[46m");
	ret.bwhite = StringView("\033[47m");
	ret.bdef = StringView("\033[49m");
	//}
	//}
}

#endif

#if SPRT_ANDROID

static void checkLogFeaturesSupport(LogFeaturesInit &ret) { }

#endif

static LogFeaturesInit s_logInit;

void print(LogType type, StringView prefix, StringView tag, StringView text) {
#if defined(SPRT_ANDROID)
	tag.performWithTerminated([&](const char *tagBuf, size_t len) {
		switch (type) {
		case LogType::Verbose:
			__android_log_print(ANDROID_LOG_VERBOSE, tagBuf, "%.*s", int(text.size()), text.data());
			break;
		case LogType::Debug:
			__android_log_print(ANDROID_LOG_DEBUG, tagBuf, "%.*s", int(text.size()), text.data());
			break;
		case LogType::Info:
			__android_log_print(ANDROID_LOG_INFO, tagBuf, "%.*s", int(text.size()), text.data());
			break;
		case LogType::Warn:
			__android_log_print(ANDROID_LOG_WARN, tagBuf, "%.*s", int(text.size()), text.data());
			break;
		case LogType::Error:
			__android_log_print(ANDROID_LOG_ERROR, tagBuf, "%.*s", int(text.size()), text.data());
			break;
		case LogType::Fatal:
			__android_log_print(ANDROID_LOG_FATAL, tagBuf, "%.*s", int(text.size()), text.data());
			break;
		}
	});
#else
	StringBuffer<char> prefixData;
	if (prefix.empty()) {
		switch (type) {
		case LogType::Verbose:
			prefixData = StringBuffer<char>::create(s_logInit.reverse, s_logInit.bold,
					s_logInit.fcyan, "[V]", s_logInit.fdef, s_logInit.drop);
			break;
		case LogType::Debug:
			prefixData = StringBuffer<char>::create(s_logInit.reverse, s_logInit.bold,
					s_logInit.fblue, "[D]", s_logInit.fdef, s_logInit.drop);
			break;
		case LogType::Info:
			prefixData = StringBuffer<char>::create(s_logInit.reverse, s_logInit.bold,
					s_logInit.fgreen, "[I]", s_logInit.fdef, s_logInit.drop);
			break;
		case LogType::Warn:
			prefixData = StringBuffer<char>::create(s_logInit.reverse, s_logInit.bold,
					s_logInit.fyellow, "[W]", s_logInit.fdef, s_logInit.drop);
			break;
		case LogType::Error:
			prefixData = StringBuffer<char>::create(s_logInit.reverse, s_logInit.bold,
					s_logInit.fred, "[E]", s_logInit.fdef, s_logInit.drop);
			break;
		case LogType::Fatal:
			prefixData = StringBuffer<char>::create(s_logInit.reverse, s_logInit.bold,
					s_logInit.fred, "[F]", s_logInit.fdef, s_logInit.drop);
			break;
		}
		prefix = prefixData;
	}

	auto bufSize = prefix.size() + 1 + tag.size() + 2 + text.size() + 3;
	auto buf = new char[bufSize];

	auto freeSize = bufSize;
	auto target = buf;

	target = strappend(target, &freeSize, prefix.data(), prefix.size());
	target = strappend(target, &freeSize, " ", 1);
	target = strappend(target, &freeSize, tag.data(), tag.size());
	target = strappend(target, &freeSize, ": ", 2);
	target = strappend(target, &freeSize, text.data(), text.size());

#ifndef __apple__
	target = strappend(target, &freeSize, "\n", 1);
#endif

	::fwrite(buf, target - buf, 1, stderr);
	::fflush(stderr);
#endif // platform switch
}

LogFeaturesInit::LogFeaturesInit() {
	::__sprt_memset(tmpbuf, 0, 256);
	tmpTarget = tmpbuf;
	checkLogFeaturesSupport(*this);
}

StringView LogFeaturesInit::copy(StringView str) {
	::__sprt_memcpy(tmpTarget, str.data(), str.size());
	StringView ret(tmpTarget, str.size());
	tmpTarget += str.size();
	return ret;
}

LogFeatures LogFeatures::acquire() {
	LogFeatures ret(static_cast<const LogFeatures &>(s_logInit));
	return ret;
}

} // namespace sprt::log
