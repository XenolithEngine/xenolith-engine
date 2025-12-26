/**
Copyright (c) 2016-2019 Roman Katuntsev <sbkarr@stappler.org>
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
#include "SPValid.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::data {

using NextToken = chars::Chars<char, '=', '&', ';', '[', ']', '+', '%'>;
using NextKey = chars::Chars<char, '&', ';', '+'>;

template <typename Interface>
class UrlencodeParser {
public:
	using Reader = StringView;

	enum class Literal {
		None,
		String,
		Percent,
		Open,
		Close,
		Delim,
	};

	enum class VarState {
		Key,
		SubKey,
		SubKeyEnd,
		Value,
		End,
	};

	UrlencodeParser(ValueTemplate<Interface> &target, size_t length = maxOf<size_t>(),
			size_t maxVarSize = maxOf<size_t>())
	: target(&target), length(length), maxVarSize(maxVarSize) { }

	size_t read(const uint8_t *s, size_t count);

	ValueTemplate<Interface> *flushString(StringView &r, ValueTemplate<Interface> *,
			VarState state);

	ValueTemplate<Interface> &data() { return *target; }
	const ValueTemplate<Interface> &data() const { return *target; }

protected:
	void bufferize(Reader &r);
	void bufferize(char c);
	void flush(Reader &r);

	ValueTemplate<Interface> *target;
	size_t length = maxOf<size_t>();
	size_t maxVarSize = maxOf<size_t>();

	bool skip = false;
	VarState state = VarState::Key;
	Literal literal = Literal::None;

	BufferTemplate<Interface> buf;
	ValueTemplate<Interface> *current = nullptr;
};

template <typename Interface>
void UrlencodeParser<Interface>::bufferize(Reader &r) {
	if (!skip) {
		if (buf.size() + r.size() > maxVarSize) {
			buf.clear();
			skip = true;
		} else {
			buf.put(r.data(), r.size());
		}
	}
}

template <typename Interface>
void UrlencodeParser<Interface>::bufferize(char c) {
	if (!skip) {
		if (buf.size() + 1 > maxVarSize) {
			buf.clear();
			skip = true;
		} else {
			buf.putc(c);
			return;
		}
	}
	buf.putc(c);
}

template <typename Interface>
void UrlencodeParser<Interface>::flush(Reader &r) {
	if (!skip) {
		if (r.size() < maxVarSize) {
			current = flushString(r, current, state);
		} else {
			skip = true;
		}
		buf.clear();
	}
}

template <typename Interface>
size_t UrlencodeParser<Interface>::read(const uint8_t *s, size_t count) {
	if (count >= length) {
		count = length;
		length = 0;
	}
	Reader r((const char *)s, count);

	while (!r.empty()) {
		Reader str;
		if (state == VarState::Value) {
			str = r.readUntil<NextKey>();
		} else {
			str = r.readUntil<NextToken>();
		}

		if (buf.empty() && (!r.empty() || length == 0) && !r.is('+') && !r.is('%')) {
			flush(str);
		} else {
			bufferize(str);
			if (!r.empty() && !r.is('+') && !r.is('%')) {
				Reader tmp = buf.get();
				flush(tmp);
			}
		}

		char c = r[0];
		if (c == '+') {
			bufferize(' ');
			++r;
		} else {
			++r;
			if (c == '%') {
				if (r.is("5B")) {
					c = '[';
					r += 2;
				} else if (r.is("5D")) {
					c = ']';
					r += 2;
				} else {
					bufferize('%');
					c = 0;
				}
			}

			if (c != 0) {
				switch (state) {
				case VarState::Key:
					switch (c) {
					case '[': state = VarState::SubKey; break;
					case '=': state = VarState::Value; break;
					case '&':
					case ';': state = VarState::Key; break;
					default: state = VarState::End; break;
					}
					break;
				case VarState::SubKey:
					switch (c) {
					case ']': state = VarState::SubKeyEnd; break;
					default: state = VarState::End; break;
					}
					break;
				case VarState::SubKeyEnd:
					switch (c) {
					case '[': state = VarState::SubKey; break;
					case '=': state = VarState::Value; break;
					case '&':
					case ';': state = VarState::Key; break;
					default: state = VarState::End; break;
					}
					break;
				case VarState::Value:
					switch (c) {
					case '&':
					case ';':
						state = VarState::Key;
						skip = false;
						break;
					default: state = VarState::End; break;
					}
					break;
				default: break;
				}
			}
		}
		if (skip) {
			break;
		}
	}

	if (!buf.empty()) {
		auto tmp = buf.get();
		flush(tmp);
	}

	return count;
}

template <typename Interface>
auto UrlencodeParser<Interface>::flushString(StringView &r, ValueTemplate<Interface> *cur,
		VarState varState) -> ValueTemplate<Interface> * {
	auto str = string::urldecode<Interface>(r);

	switch (varState) {
	case VarState::Key:
		if (!str.empty()) {
			if (target->hasValue(str)) {
				cur = &target->getValue(str);
			} else {
				cur = &target->setValue(ValueTemplate<Interface>(true), str);
			}
		}
		break;
	case VarState::SubKey:
		if (cur) {
			if (!str.empty() && valid::validateNumber(str)) {
				auto num = StringView(str).readInteger().get();
				if (cur->isArray()) {
					if (num < int64_t(cur->size())) {
						cur = &cur->getValue(num);
						return cur;
					} else if (num == int64_t(cur->size())) {
						cur = &cur->addValue(ValueTemplate<Interface>(true));
						return cur;
					}
				} else if (!cur->isDictionary() && num == 0) {
					cur->setArray(typename ValueTemplate<Interface>::ArrayType());
					cur = &cur->addValue(ValueTemplate<Interface>(true));
					return cur;
				}
			}
			if (str.empty()) {
				if (!cur->isArray()) {
					cur->setArray(typename ValueTemplate<Interface>::ArrayType());
				}
				cur = &cur->addValue(ValueTemplate<Interface>(true));
			} else {
				if (!cur->isDictionary()) {
					cur->setDict(typename ValueTemplate<Interface>::DictionaryType());
				}
				if (cur->hasValue(str)) {
					cur = &cur->getValue(str);
				} else {
					cur = &cur->setValue(ValueTemplate<Interface>(true), str);
				}
			}
		}
		break;
	case VarState::Value:
	case VarState::End:
		if (cur) {
			if (!str.empty()) {
				cur->setString(str);
			}
			cur = nullptr;
		}
		break;
	default: break;
	}

	return cur;
}

template <>
SP_PUBLIC auto readUrlencoded<memory::PoolInterface>(StringView r, size_t maxVarSize)
		-> ValueTemplate<memory::PoolInterface> {
	ValueTemplate<memory::PoolInterface> ret;
	UrlencodeParser<memory::PoolInterface> parser(ret, r.size(), maxVarSize);
	parser.read((const uint8_t *)r.data(), r.size());
	return ret;
}

template <>
SP_PUBLIC auto readUrlencoded<memory::StandartInterface>(StringView r, size_t maxVarSize)
		-> ValueTemplate<memory::StandartInterface> {
	ValueTemplate<memory::StandartInterface> ret;
	UrlencodeParser<memory::StandartInterface> parser(ret, r.size(), maxVarSize);
	parser.read((const uint8_t *)r.data(), r.size());
	return ret;
}


} // namespace stappler::data
