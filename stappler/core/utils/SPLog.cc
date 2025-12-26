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

#include "SPLog.h"

#if MODULE_STAPPLER_THREADS
#include "SPThread.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::log {

static const constexpr int MAX_LOG_FUNC = 16;

#ifndef STAPPLER_LOG_LEVEL
#if DEBUG
static std::bitset<6> s_logMask;
#else
static std::bitset<6> s_logMask = (1 | 2 | 4 | 8);
#endif
#else
#if STAPPLER_LOG_LEVEL == 0
// restrict all
static std::bitset<6> s_logMask = (1 | 2 | 4 | 8 | 16 | 32);
#elif STAPPLER_LOG_LEVEL == 1
static std::bitset<6> s_logMask = (1 | 2 | 4 | 8);
#else
// allow all
static std::bitset<6> s_logMask = (0);
#endif
#endif

using LogFeatures = sprt::log::LogFeatures;

struct CustomLogManager {
	static void initialize(void *ptr) { reinterpret_cast<CustomLogManager *>(ptr)->init(); }
	static void terminate(void *ptr) { reinterpret_cast<CustomLogManager *>(ptr)->term(); }

	CustomLog::log_fn logFuncArr[MAX_LOG_FUNC] = {0};
	std::atomic<int> logFuncCount;
	std::mutex logFuncMutex;

	LogFeatures features;

	CustomLogManager();

	void init();
	void term();

	void insert(CustomLog::log_fn fn);
	void remove(CustomLog::log_fn fn);
	void log(LogType type, StringView tag, const SourceLocation &source, CustomLog::Type t,
			CustomLog::VA &va);
};

static CustomLogManager s_logManager;

static void DefaultLog2(LogType type, StringView tag, const SourceLocation &source,
		StringView text) {
	std::stringstream prefixStream;

#if !ANDROID
	prefixStream << s_logManager.features.reverse << s_logManager.features.bold;
	switch (type) {
	case LogType::Verbose:
		prefixStream << s_logManager.features.fcyan << "[V]" << s_logManager.features.fdef;
		break;
	case LogType::Debug:
		prefixStream << s_logManager.features.fblue << "[D]" << s_logManager.features.fdef;
		break;
	case LogType::Info:
		prefixStream << s_logManager.features.fgreen << "[I]" << s_logManager.features.fdef;
		break;
	case LogType::Warn:
		prefixStream << s_logManager.features.fyellow << "[W]" << s_logManager.features.fdef;
		break;
	case LogType::Error:
		prefixStream << s_logManager.features.fred << "[E]" << s_logManager.features.fdef;
		break;
	case LogType::Fatal:
		prefixStream << s_logManager.features.fred << "[F]" << s_logManager.features.fdef;
		break;
	}
	prefixStream << s_logManager.features.drop;
#endif

#if MODULE_STAPPLER_THREADS
	prefixStream << s_logManager.features.italic;
	if (auto local = thread::ThreadInfo::getThreadInfo()) {
		if (!local->managed) {
			prefixStream << "[Thread:" << std::this_thread::get_id() << "]";
		} else if (local->workerId == thread::ThreadInfo::DetachedWorker) {
			prefixStream << "[" << local->name << "]";
		} else {
			prefixStream << "[" << local->name << ":" << local->workerId << "]";
		}
	} else {
		prefixStream << "[Log]";
	}
	prefixStream << s_logManager.features.drop << " ";
#endif
	auto prefix = prefixStream.str();

#if SP_SOURCE_DEBUG
	if (!source.empty()) {
		auto textToLog = mem_std::toString(text, " ", s_logManager.features.underline,
				s_logManager.features.dim, source.fileName, ":", source.line,
				s_logManager.features.drop);
		sprt::log::print(type, sprt::StringView(prefix.data(), prefix.size()),
				sprt::StringView(tag.data(), tag.size()),
				sprt::StringView(textToLog.data(), textToLog.size()));
	} else {
		sprt::log::print(type, sprt::StringView(prefix.data(), prefix.size()),
				sprt::StringView(tag.data(), tag.size()),
				sprt::StringView(text.data(), text.size()));
	}
#else
	sprt::log::print(type, sprt::StringView(prefix.data(), prefix.size()),
			sprt::StringView(tag.data(), tag.size()), sprt::StringView(text.data(), text.size()));
#endif
}

static void DefaultLog(LogType type, StringView tag, const SourceLocation &source,
		CustomLog::Type t, CustomLog::VA &va) {
	if (t == CustomLog::Text) {
		DefaultLog2(type, tag, source, va.text);
	} else {
		char stackBuf[1_KiB];
		va_list tmpList;
		va_copy(tmpList, va.format.args);
		int size = ::vsnprintf(stackBuf, size_t(1_KiB - 1), va.format.format, tmpList);
		va_end(tmpList);
		if (size > int(1_KiB - 1)) {
			char *buf = new char[size + 1];
			size = ::vsnprintf(buf, size_t(size), va.format.format, va.format.args);
			DefaultLog2(type, tag, source, StringView(buf, size));
			delete[] buf;
		} else if (size >= 0) {
			DefaultLog2(type, tag, source, StringView(stackBuf, size));
		} else {
			DefaultLog2(type, tag, source, "Log error");
		}
	}
}

CustomLogManager::CustomLogManager() { addInitializer(this, initialize, terminate); }

void CustomLogManager::init() { features = sprt::log::LogFeatures::acquire(); }

void CustomLogManager::term() { }

void CustomLogManager::insert(CustomLog::log_fn fn) {
	logFuncMutex.lock();
	if (logFuncCount.load() < MAX_LOG_FUNC) {
		logFuncArr[logFuncCount] = fn;
		++logFuncCount;
	}
	logFuncMutex.unlock();
}

void CustomLogManager::remove(CustomLog::log_fn fn) {
	logFuncMutex.lock();
	int count = logFuncCount.load();
	for (int i = 0; i < count; i++) {
		if (logFuncArr[i] == fn) {
			if (i != count - 1) {
				memmove(&logFuncArr[i], &logFuncArr[i + 1],
						(count - i - 1) * sizeof(CustomLog::log_fn));
			}
			--logFuncCount;
			break;
		}
	}
	logFuncMutex.unlock();
}

void CustomLogManager::log(LogType type, StringView tag, const SourceLocation &source,
		CustomLog::Type t, CustomLog::VA &va) {
	if (s_logMask.test(toInt(type))) {
		return;
	}

	int count = logFuncCount.load();
	if (count == 0) {
		DefaultLog(type, tag, source, t, va);
	} else {
		bool success = true;
		logFuncMutex.lock();
		count = logFuncCount.load();
		for (int i = 0; i < count; i++) {
			success = logFuncArr[i](type, tag, source, t, va);
			if (!success) {
				break;
			}
		}
		logFuncMutex.unlock();
		if (success) {
			DefaultLog(type, tag, source, t, va);
		}
	}
}

CustomLog::CustomLog(log_fn logfn) : fn(logfn) {
	manager = &s_logManager;
	if (fn) {
		reinterpret_cast<CustomLogManager *>(manager)->insert(fn);
	}
}

CustomLog::~CustomLog() {
	if (fn) {
		reinterpret_cast<CustomLogManager *>(manager)->remove(fn);
	}
}

CustomLog::CustomLog(CustomLog &&other) : fn(other.fn) {
	manager = &s_logManager;
	other.fn = nullptr;
}

CustomLog &CustomLog::operator=(CustomLog &&other) {
	manager = &s_logManager;
	if (fn && fn != other.fn) {
		reinterpret_cast<CustomLogManager *>(manager)->remove(fn);
	}
	fn = other.fn;
	other.fn = nullptr;
	return *this;
}

static std::bitset<6> makeFilterMask(InitializerList<LogType> type) {
	std::bitset<6> mask;
	for (auto &it : type) { mask.set(toInt(it)); }
	return mask;
}

std::bitset<6> None = makeFilterMask({
	LogType::Verbose,
	LogType::Debug,
	LogType::Info,
	LogType::Warn,
	LogType::Error,
	LogType::Fatal,
});
std::bitset<6> ErrorsOnly = makeFilterMask({
	LogType::Verbose,
	LogType::Debug,
	LogType::Info,
	LogType::Warn,
});
std::bitset<6> Full = std::bitset<6>(0);

void setLogFilterMask(const std::bitset<6> &mask) { s_logMask = mask; }

void setLogFilterMask(std::bitset<6> &&mask) { s_logMask = sp::move(mask); }

void setLogFilterMask(InitializerList<LogType> types) { setLogFilterMask(makeFilterMask(types)); }

std::bitset<6> getlogFilterMask() { return s_logMask; }

void format(LogType type, const char *tag, const SourceLocation &source, const char *fmt, ...) {
	CustomLog::VA va;
	va_start(va.format.args, fmt);
	va.format.format = fmt;

	s_logManager.log(type, tag, source, CustomLog::Format, va);

	va_end(va.format.args);
}

void format(LogType type, StringView tag, const SourceLocation &source, const char *fmt, ...) {
	CustomLog::VA va;
	va_start(va.format.args, fmt);
	va.format.format = fmt;

	s_logManager.log(type, tag, source, CustomLog::Format, va);

	va_end(va.format.args);
}

void text(LogType type, const char *tag, const SourceLocation &source, const char *text) {
	CustomLog::VA va;
	va.text = StringView(text);
	s_logManager.log(type, tag, source, CustomLog::Text, va);
}

void text(LogType type, StringView tag, StringView text, const SourceLocation &source) {
	CustomLog::VA va;
	va.text = text;
	s_logManager.log(type, tag, source, CustomLog::Text, va);
}

} // namespace stappler::log
