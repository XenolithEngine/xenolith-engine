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

#include "SPString.h"

#include <sprt/runtime/base64.h>
#include <sprt/runtime/base16.h>

namespace STAPPLER_VERSIONIZED stappler::base64 {

size_t encodeSize(size_t l) { return sprt::base64::getEncodeSize(l); }
size_t decodeSize(size_t l) { return sprt::base64::getDecodeSize(l); }

template <>
auto encode<memory::PoolInterface>(const CoderSource &source) ->
		typename memory::PoolInterface::StringType {
	typename memory::PoolInterface::StringType output;
	output.resize(encodeSize(source.size()));
	output.resize(sprt::base64::encode(source.data(), source.size(), output.data(), output.size()));
	return output;
}

template <>
auto encode<memory::StandartInterface>(const CoderSource &source) ->
		typename memory::StandartInterface::StringType {
	typename memory::StandartInterface::StringType output;
	output.resize(encodeSize(source.size()));
	output.resize(sprt::base64::encode(source.data(), source.size(), output.data(), output.size()));
	return output;
}

void encode(std::basic_ostream<char> &stream, const CoderSource &source) {
	sprt::base64::encode(source.data(), source.size(),
			[&](const char *str, size_t size) { stream.write(str, size); });
}

size_t encode(char *buf, size_t bsize, const CoderSource &source) {
	return sprt::base64::encode(source.data(), source.size(), buf, bsize);
}

template <>
auto decode<memory::PoolInterface>(const CoderSource &source) ->
		typename memory::PoolInterface::BytesType {
	typename memory::PoolInterface::BytesType output;
	output.resize(decodeSize(source.size()));
	output.resize(sprt::base64::decode((const char *)source.data(), source.size(), output.data(),
			output.size()));
	return output;
}

template <>
auto decode<memory::StandartInterface>(const CoderSource &source) ->
		typename memory::StandartInterface::BytesType {
	typename memory::StandartInterface::BytesType output;
	output.resize(decodeSize(source.size()));
	output.resize(sprt::base64::decode((const char *)source.data(), source.size(), output.data(),
			output.size()));
	return output;
}

void decode(std::basic_ostream<char> &stream, const CoderSource &source) {
	sprt::base64::decode((const char *)source.data(), source.size(),
			[&](const uint8_t *str, size_t size) { stream.write((const char *)str, size); });
}

size_t decode(uint8_t *buf, size_t bsize, const CoderSource &source) {
	return sprt::base64::decode((const char *)source.data(), source.size(), buf, bsize);
}

} // namespace stappler::base64

namespace STAPPLER_VERSIONIZED stappler::base64url {

template <>
auto encode<memory::PoolInterface>(const CoderSource &source) ->
		typename memory::PoolInterface::StringType {
	typename memory::PoolInterface::StringType output;
	output.resize(encodeSize(source.size()));
	output.resize(
			sprt::base64url::encode(source.data(), source.size(), output.data(), output.size()));
	return output;
}

template <>
auto encode<memory::StandartInterface>(const CoderSource &source) ->
		typename memory::StandartInterface::StringType {
	typename memory::StandartInterface::StringType output;
	output.resize(encodeSize(source.size()));
	output.resize(
			sprt::base64url::encode(source.data(), source.size(), output.data(), output.size()));
	return output;
}

void encode(std::basic_ostream<char> &stream, const CoderSource &source) {
	sprt::base64url::encode(source.data(), source.size(),
			[&](const char *str, size_t size) { stream.write(str, size); });
}

size_t encode(char *buf, size_t bsize, const CoderSource &source) {
	return sprt::base64url::encode(source.data(), source.size(), buf, bsize);
}

} // namespace stappler::base64url

namespace STAPPLER_VERSIONIZED stappler::base16 {

size_t encodeSize(size_t length) { return length * 2; }
size_t decodeSize(size_t length) { return length / 2; }

const char *charToHex(const char &c, bool upper) {
	return sprt::base16::toHex(static_cast<uint8_t>(c), upper);
}

uint8_t hexToChar(const char &c) { return sprt::base16::toChar(c); }

uint8_t hexToChar(const char &c, const char &d) { return sprt::base16::toChar(c, d); }

template <>
auto encode<memory::PoolInterface>(const CoderSource &source, bool upper) ->
		typename memory::PoolInterface::StringType {
	typename memory::PoolInterface::StringType output;
	output.resize(encodeSize(source.size()));
	output.resize(sprt::base16::encode(source.data(), source.size(), output.data(), output.size(),
			upper));
	return output;
}

template <>
auto encode<memory::StandartInterface>(const CoderSource &source, bool upper) ->
		typename memory::StandartInterface::StringType {
	typename memory::StandartInterface::StringType output;
	output.resize(encodeSize(source.size()));
	output.resize(sprt::base16::encode(source.data(), source.size(), output.data(), output.size(),
			upper));
	return output;
}

void encode(std::basic_ostream<char> &stream, const CoderSource &source, bool upper) {
	sprt::base16::encode(source.data(), source.size(),
			[&](const char *str, size_t size) { stream.write(str, size); }, upper);
}

size_t encode(char *buf, size_t bsize, const CoderSource &source, bool upper) {
	return sprt::base16::encode(source.data(), source.size(), buf, bsize, upper);
}

template <>
auto decode<memory::PoolInterface>(const CoderSource &source) ->
		typename memory::PoolInterface::BytesType {
	typename memory::PoolInterface::BytesType output;
	output.resize(decodeSize(source.size()));
	output.resize(sprt::base16::decode((const char *)source.data(), source.size(), output.data(),
			output.size()));
	return output;
}

template <>
auto decode<memory::StandartInterface>(const CoderSource &source) ->
		typename memory::StandartInterface::BytesType {
	typename memory::StandartInterface::BytesType output;
	output.resize(decodeSize(source.size()));
	output.resize(sprt::base16::decode((const char *)source.data(), source.size(), output.data(),
			output.size()));
	return output;
}

void decode(std::basic_ostream<char> &stream, const CoderSource &source) {
	sprt::base16::decode((const char *)source.data(), source.size(),
			[&](const uint8_t *str, size_t size) { stream.write((const char *)str, size); });
}

size_t decode(uint8_t *buf, size_t bsize, const CoderSource &source) {
	return sprt::base16::decode((const char *)source.data(), source.size(), buf, bsize);
}

} // namespace stappler::base16
