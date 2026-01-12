/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPWasm.h"
#include "SPString.h"

namespace stappler::wasm {

static uint32_t StapplerCoreItoaU8(wasm_exec_env_t exec_env, int64_t val, char *buf,
		uint32_t bufLen) {
	return uint32_t(string::detail::itoa(val, buf, bufLen));
}

static uint32_t StapplerCoreItoaU16(wasm_exec_env_t exec_env, int64_t val, char16_t *buf,
		uint32_t bufLen) {
	return uint32_t(string::detail::itoa(val, buf, bufLen));
}

static uint32_t StapplerCoreItoaLen(wasm_exec_env_t exec_env, int64_t val) {
	return uint32_t(string::detail::itoa(val, (char *)nullptr, 0));
}

static uint32_t StapplerCoreDtoaU8(wasm_exec_env_t exec_env, double val, char *buf,
		uint32_t bufLen) {
	return uint32_t(string::detail::dtoa(val, buf, bufLen));
}

static uint32_t StapplerCoreDtoaU16(wasm_exec_env_t exec_env, double val, char16_t *buf,
		uint32_t bufLen) {
	return uint32_t(string::detail::dtoa(val, buf, bufLen));
}

static uint32_t StapplerCoreDtoaLen(wasm_exec_env_t exec_env, double val) {
	return uint32_t(string::detail::dtoa(val, (char *)nullptr, 0));
}

static void StapplerCoreToUtf16(wasm_exec_env_t exec_env, char *ptr, uint32_t size,
		ListOutput *outputData) {
	auto env = ExecEnv::get(exec_env);
	auto sourceString = StringView(ptr, size);

	const auto outSize = string::getUtf16Length(sourceString);

	char16_t *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(outSize * sizeof(char16_t)), &outStringBuffer);

	auto targetBuf = outStringBuffer;

	uint8_t offset = 0;
	auto sourcePtr = sourceString.data();
	auto sourceLen = sourceString.size();
	auto end = sourcePtr + sourceString.size();
	while (sourcePtr < end) {
		auto c = unicode::utf8Decode32(sourcePtr, sourceLen, offset);
		targetBuf += unicode::utf16EncodeBuf(targetBuf, c);
		sourcePtr += offset;
		sourceLen -= offset;
	}

	outputData->ptr = outOffset;
	outputData->len = uint32_t(outSize);
}

static void StapplerCoreToUtf8(wasm_exec_env_t exec_env, char16_t *ptr, uint32_t size,
		ListOutput *outputData) {
	auto env = ExecEnv::get(exec_env);
	auto sourceString = WideStringView(ptr, size);

	const auto outSize = string::getUtf8Length(sourceString);

	char *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(outSize * sizeof(char)), &outStringBuffer);

	auto targetBuf = outStringBuffer;

	uint8_t offset;
	auto sourcePtr = sourceString.data();
	auto end = sourcePtr + sourceString.size();
	while (sourcePtr < end) {
		auto c = unicode::utf16Decode32(sourcePtr, end - sourcePtr, offset);
		targetBuf += unicode::utf8EncodeBuf(targetBuf, c);
		sourcePtr += offset;
	}

	outputData->ptr = outOffset;
	outputData->len = uint32_t(outSize);
}

template <typename Char>
static void StapplerCoreConvertCase(wasm_exec_env_t exec_env, Char *ptr, uint32_t size,
		ListOutput *outputData, const Callback<std::basic_string<Char>(StringViewBase<Char>)> &cb) {
	auto env = ExecEnv::get(exec_env);
	auto sourceString = StringViewBase<Char>(ptr, size);

	auto outString = cb(sourceString);

	char *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(outString.size() * sizeof(Char)), &outStringBuffer);

	memcpy(outStringBuffer, outString.data(), outString.size() * sizeof(Char));

	outputData->ptr = outOffset;
	outputData->len = uint32_t(outString.size());
}

static void StapplerCoreToUpperU8(wasm_exec_env_t exec_env, char *ptr, uint32_t size,
		ListOutput *target) {
	StapplerCoreConvertCase<char>(exec_env, ptr, size, target,
			[](StringView str) { return platform::toupper<Interface>(str); });
}

static void StapplerCoreToLowerU8(wasm_exec_env_t exec_env, char *ptr, uint32_t size,
		ListOutput *target) {
	StapplerCoreConvertCase<char>(exec_env, ptr, size, target,
			[](StringView str) { return platform::tolower<Interface>(str); });
}

static void StapplerCoreToTitleU8(wasm_exec_env_t exec_env, char *ptr, uint32_t size,
		ListOutput *target) {
	StapplerCoreConvertCase<char>(exec_env, ptr, size, target,
			[](StringView str) { return platform::totitle<Interface>(str); });
}

static void StapplerCoreToUpperU16(wasm_exec_env_t exec_env, char16_t *ptr, uint32_t size,
		ListOutput *target) {
	StapplerCoreConvertCase<char16_t>(exec_env, ptr, size, target,
			[](WideStringView str) { return platform::toupper<Interface>(str); });
}

static void StapplerCoreToLowerU16(wasm_exec_env_t exec_env, char16_t *ptr, uint32_t size,
		ListOutput *target) {
	StapplerCoreConvertCase<char16_t>(exec_env, ptr, size, target,
			[](WideStringView str) { return platform::tolower<Interface>(str); });
}

static void StapplerCoreToTitleU16(wasm_exec_env_t exec_env, char16_t *ptr, uint32_t size,
		ListOutput *target) {
	StapplerCoreConvertCase<char16_t>(exec_env, ptr, size, target,
			[](WideStringView str) { return platform::totitle<Interface>(str); });
}

static uint64_t StapplerCoreTimeNow(wasm_exec_env_t exec_env) { return Time::now().toMicros(); }

static uint32_t StapplerCoreTimeToHttp(wasm_exec_env_t exec_env, uint64_t t, char *buf,
		uint32_t size) {
	if (size < 30) {
		return 0;
	}

	sp_time_exp_t xt(t);
	return uint32_t(xt.encodeRfc822(buf));
}

static uint32_t StapplerCoreTimeToAtomXml(wasm_exec_env_t exec_env, uint64_t t, char *buf,
		uint32_t size) {
	if (size < 21) {
		return 0;
	}

	sp_time_exp_t xt(t, false);
	return uint32_t(xt.encodeIso8601(buf, 0));
}

static uint32_t StapplerCoreTimeToCTime(wasm_exec_env_t exec_env, uint64_t t, char *buf,
		uint32_t size) {
	if (size < 25) {
		return 0;
	}

	sp_time_exp_t xt(t, true);
	return uint32_t(xt.encodeCTime(buf));
}

static uint32_t StapplerCoreTimeToIso8601(wasm_exec_env_t exec_env, uint64_t t, uint32_t precision,
		char *buf, uint32_t size) {
	if (size < 22 + precision) {
		return 0;
	}

	sp_time_exp_t xt(t, true);
	return uint32_t(xt.encodeIso8601(buf, precision));
}

static uint32_t StapplerCoreTimeToFormat(wasm_exec_env_t exec_env, uint64_t t, char *fmt,
		uint32_t fmtLen, char *buf, uint32_t bufLen) {
	return uint32_t(Time(t).encodeToFormat(buf, bufLen, fmt));
}

static NativeSymbol stapper_core_symbols[] = {
	NativeSymbol{"itoa-u8", (void *)&StapplerCoreItoaU8, "(I*~)i", NULL},
	NativeSymbol{"itoa-u16", (void *)&StapplerCoreItoaU16, "(I*~)i", NULL},
	NativeSymbol{"itoa-len", (void *)&StapplerCoreItoaLen, "(I)i", NULL},

	NativeSymbol{"dtoa-u8", (void *)&StapplerCoreDtoaU8, "(F*~)i", NULL},
	NativeSymbol{"dtoa-u16", (void *)&StapplerCoreDtoaU16, "(F*~)i", NULL},
	NativeSymbol{"dtoa-len", (void *)&StapplerCoreDtoaLen, "(F)i", NULL},

	NativeSymbol{"to-utf16", (void *)&StapplerCoreToUtf16, "(*~*)", NULL},
	NativeSymbol{"to-utf8", (void *)&StapplerCoreToUtf8, "(*i*)", NULL},

	NativeSymbol{"toupper-u8", (void *)&StapplerCoreToUpperU8, "(*~*)", NULL},
	NativeSymbol{"tolower-u8", (void *)&StapplerCoreToLowerU8, "(*~*)", NULL},
	NativeSymbol{"totitle-u8", (void *)&StapplerCoreToTitleU8, "(*~*)", NULL},

	NativeSymbol{"toupper-u16", (void *)&StapplerCoreToUpperU16, "(*i*)", NULL},
	NativeSymbol{"tolower-u16", (void *)&StapplerCoreToLowerU16, "(*i*)", NULL},
	NativeSymbol{"totitle-u16", (void *)&StapplerCoreToTitleU16, "(*i*)", NULL},

	NativeSymbol{"time-now", (void *)&StapplerCoreTimeNow, "()I", NULL},
	NativeSymbol{"time-to-http", (void *)&StapplerCoreTimeToHttp, "(I*~)i", NULL},
	NativeSymbol{"time-to-atom-xml", (void *)&StapplerCoreTimeToAtomXml, "((I*~)i", NULL},
	NativeSymbol{"time-to-rfc822", (void *)&StapplerCoreTimeToHttp, "(I*~)i", NULL},
	NativeSymbol{"time-to-ctime", (void *)&StapplerCoreTimeToCTime, "((I*~)i", NULL},
	NativeSymbol{"time-to-iso8601", (void *)&StapplerCoreTimeToIso8601, "((Ii*~)i", NULL},
	NativeSymbol{"time-to-format", (void *)&StapplerCoreTimeToFormat, "((I*~*~)i", NULL},
};

static NativeModule s_coreModule("stappler:wasm/core", stapper_core_symbols,
		sizeof(stapper_core_symbols) / sizeof(NativeSymbol));

} // namespace stappler::wasm
