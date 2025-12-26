/**
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

#ifndef CORE_CRYPTO_SPCRYPTOASN1_H_
#define CORE_CRYPTO_SPCRYPTOASN1_H_

#include "SPCommon.h"

namespace STAPPLER_VERSIONIZED stappler::crypto {

template <typename T>
struct SP_PUBLIC Asn1DecoderTraits {
	using success = char;
	using failure = long;

	InvokerCallTest_MakeCallTest(onBeginSet, success, failure);
	InvokerCallTest_MakeCallTest(onEndSet, success, failure);
	InvokerCallTest_MakeCallTest(onBeginSequence, success, failure);
	InvokerCallTest_MakeCallTest(onEndSequence, success, failure);
	InvokerCallTest_MakeCallTest(onOid, success, failure);
	InvokerCallTest_MakeCallTest(onNull, success, failure);
	InvokerCallTest_MakeCallTest(onInteger, success, failure);
	InvokerCallTest_MakeCallTest(onBigInteger, success, failure);
	InvokerCallTest_MakeCallTest(onBoolean, success, failure);
	InvokerCallTest_MakeCallTest(onBytes, success, failure);
	InvokerCallTest_MakeCallTest(onString, success, failure);
	InvokerCallTest_MakeCallTest(onCustom, success, failure);
};

template <typename Interface, typename ReaderType, typename Traits = Asn1DecoderTraits<ReaderType>>
struct SP_PUBLIC Asn1Decoder {
	using Bytes = typename Interface::BytesType;
	using StringStream = typename Interface::StringStreamType;

	enum Type : uint8_t {
		Boolean = 0x01,
		Integer = 0x02,
		BitString = 0x03,
		OctetString = 0x04,
		Null = 0x05,
		Oid = 0x06,
		Utf8String = 0x0C,
		Sequence = 0x10,
		Set = 0x11,
		PrintableString = 0x13,
		T61String = 0x14,
		AsciiString = 0x16,
		UtcTime = 0x17,
		Time = 0x18,
		UniversalString = 0x1C,
		BmpString = 0x1E,
		HighForm = 0x1F,
		Primitive = 0x00,

		ConstructedBit = 0x20,
		ContextSpecificBit = 0x80,
	};

	size_t decodeSize(BytesViewNetwork &r) {
		size_t size = 0;
		auto sizeByte = r.readUnsigned();
		if (sizeByte & 0x80) {
			auto count = sizeByte & (~0x80);
			switch (count) {
			case 1: size = r.readUnsigned(); break;
			case 2: size = r.readUnsigned16(); break;
			case 3: size = r.readUnsigned24(); break;
			case 4: size = r.readUnsigned32(); break;
			case 8: size = r.readUnsigned64(); break;
			default: return 0; break;
			}
		} else {
			size = sizeByte;
		}
		return size;
	}

	bool decodeValue(ReaderType &reader, BytesViewNetwork &r) {
		auto b = r.readUnsigned();
		Type type = Type(b & 0x1F);
		switch (type) {
		case Primitive:
			if (b & (ContextSpecificBit | ConstructedBit)) {
				return decodeAny(reader, r);
			} else {
				return decodeUnknown(reader, r, b);
			}
			break;
		case Boolean: return decodeBoolean(reader, r); break;
		case Integer: return decodeInteger(reader, r); break;
		case Oid: return decodeOid(reader, r); break;
		case Sequence: return decodeSequence(reader, r); break;
		case Set: return decodeSet(reader, r); break;
		case OctetString: return decodeOctetString(reader, r); break;
		case Null:
			r += decodeSize(r);
			if constexpr (Traits::onNull) {
				reader.onNull(*this);
			} else if constexpr (Traits::onCustom) {
				reader.onCustom(*this, uint8_t(type), BytesViewNetwork());
			}
			return true;
			break;
		case Utf8String:
		case UniversalString:
		case AsciiString:
		case PrintableString:
		case T61String:
		case BmpString:
		case UtcTime:
		case Time:
			return decodeString(reader, r, type);
			break;
		case HighForm:
			return false;
			break;
		case BitString:
			return decodeBitString(reader, r, b & ConstructedBit);
			break;
		default:
			return decodeUnknown(reader, r, b); break;
		}
		return false;
	}

	bool decodeSequence(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size == 0) {
			return false;
		}

		BytesViewNetwork nextR(r.data(), size);
		r += size;

		if constexpr (Traits::onBeginSequence) { reader.onBeginSequence(*this); }
		while(!nextR.empty()) {
			if (!decodeValue(reader, nextR)) {
				if constexpr (Traits::onEndSequence) { reader.onEndSequence(*this); }
				return false;
			}
		}
		if constexpr (Traits::onEndSequence) { reader.onEndSequence(*this); }

		return true;
	}

	bool decodeSet(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size == 0) {
			return false;
		}

		BytesViewNetwork nextR(r.data(), size);
		r += size;

		if constexpr (Traits::onBeginSet) { reader.onBeginSet(*this); }
		while(!nextR.empty()) {
			if (!decodeValue(reader, nextR)) {
				if constexpr (Traits::onEndSet) { reader.onEndSet(*this); }
				return false;
			}
		}
		if constexpr (Traits::onEndSet) { reader.onEndSet(*this); }

		return true;
	}

	bool decodeUnknown(ReaderType &reader, BytesViewNetwork &r, uint8_t t) {
		auto size = decodeSize(r);
		r += size;

		if constexpr (Traits::onCustom) { reader.onCustom(*this, t, BytesViewNetwork(r.data(), size)); }
		return true;
	}

	bool decodeAny(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size == 0) {
			return false;
		}

		BytesViewNetwork nextR(r.data(), size);
		if (decodeValue(reader, nextR)) {
			r += size;
			return true;
		}
		return false;
	}

	bool decodeOid(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size == 0) {
			return false;
		}

		StringStream str;

		BytesViewNetwork nextR(r.data(), size);

		auto first = nextR.readUnsigned();

		str << int(first / 40) << "." << int(first % 40);

		uint32_t accum = 0;
		while (!nextR.empty()) {
			auto value = nextR.readUnsigned();
			if (value & 0x80) {
				accum = accum << 7 | (value & (~0x80));
			} else {
				str << "." << (accum << 7 | value);
				accum = 0;
			}
		}


		if constexpr (Traits::onOid) {
			reader.onOid(*this, str.str());
		} else if constexpr (Traits::onCustom) {
			reader.onCustom(*this, uint8_t(Oid), BytesViewNetwork(r.data(), size));
		}

		r += size;

		return true;
	}

	bool decodeInteger(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size == 0) {
			return false;
		}

		if constexpr (Traits::onInteger) {
			switch(size) {
			case 1: reader.onInteger(*this, bit_cast<int8_t>(r.readUnsigned())); break;
			case 2: reader.onInteger(*this, bit_cast<int16_t>(r.readUnsigned16())); break;
			case 3: {
				auto val = r.readUnsigned24();
				if (val <= 0x7FFFFF) {
					reader.onInteger(*this, val);
				} else {
					reader.onInteger(*this, val - 0x1000000);
				}
			}
				break;
			case 4: reader.onInteger(*this, bit_cast<int32_t>(r.readUnsigned32())); break;
			case 8: reader.onInteger(*this, bit_cast<int64_t>(r.readUnsigned64())); break;
			default:
				if (size >= 8) {
					if constexpr (Traits::onBigInteger) {
						reader.onBigInteger(*this, BytesViewNetwork(r.data(), size));
						r += size;
					} else {
						return false;
					}
				} else {
					uint64_t accum = 0;
					for (uint32_t i = 0; i < size; ++ i) {
						accum = (accum << 8) | r.readUnsigned();
					}
					uint64_t base = 1ULL << (size * 8 - 1);
					if (accum > base) {
						reader.onInteger(*this, accum - (1ULL << (size * 8)));
					} else {
						reader.onInteger(*this, accum);
					}
				}

				break;
			}
		} else if constexpr (Traits::onBigInteger) {
			reader.onBigInteger(*this, BytesViewNetwork(r.data(), size));
			r += size;
		} else if constexpr (Traits::onCustom) {
			reader.onCustom(*this, uint8_t(Integer), BytesViewNetwork(r.data(), size));
			r += size;
		} else {
			r += size;
		}

		return true;
	}

	bool decodeBoolean(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size == 1) {
			if constexpr (Traits::onBoolean) {
				auto value = r.readUnsigned();
				reader.onBoolean(*this, value != 0);
			} else if constexpr (Traits::onCustom) {
				reader.onCustom(*this, uint8_t(Boolean), BytesViewNetwork(r.data(), size));
				r += size;
			}
			return true;
		}

		return false;
	}

	bool decodeOctetString(ReaderType &reader, BytesViewNetwork &r) {
		auto size = decodeSize(r);
		if (size > 0) {
			if constexpr (Traits::onBytes) {
				reader.onBytes(*this, BytesViewNetwork(r.data(), size));
			} else if constexpr (Traits::onCustom) {
				reader.onCustom(*this, uint8_t(OctetString), BytesViewNetwork(r.data(), size));
			}
			r += size;
			return true;
		}

		return true;
	}

	bool decodeString(ReaderType &reader, BytesViewNetwork &r, Type t) {
		auto size = decodeSize(r);
		if constexpr (Traits::onString) {
			reader.onString(*this, BytesViewNetwork(r.data(), size), t);
		} else if constexpr (Traits::onCustom) {
			reader.onCustom(*this, uint8_t(t), BytesViewNetwork(r.data(), size));
		}
		r += size;
		return true;
	}

	bool decodeBitString(ReaderType &reader, BytesViewNetwork &r, bool constructed) {
		auto size = decodeSize(r);

		if (!constructed) {
			if (size > 1) {
				auto unused = r.readUnsigned();

				if (unused > 0 && unused < 8) {
					Bytes b; b.resize(size  - 1);
					memcpy(b.data(), r.data(), size - 1);

					uint8_t mask = 0xFF << unused;
					b.back() = b.back() & mask;

					if constexpr (Traits::onBytes) {
						reader.onBytes(*this, BytesViewNetwork(b.data(), b.size()));
					} else if constexpr (Traits::onCustom) {
						reader.onCustom(*this, uint8_t(BitString), BytesViewNetwork(b.data(), b.size()));
					}
					r += size - 1;
					return true;
				} else if (unused == 0) {
					if constexpr (Traits::onBytes) {
						reader.onBytes(*this, BytesViewNetwork(r.data(), size - 1));
					} else if constexpr (Traits::onCustom) {
						reader.onCustom(*this, uint8_t(BitString), BytesViewNetwork(r.data(), size - 1));
					}
					r += size - 1;
					return true;
				}
			}
			return false;
		} else {
			r += size;
			return true;
		}
	}

	bool decode(ReaderType &reader, const Bytes &source) {
		return decode(reader, BytesViewNetwork(source));
	}

	bool decode(ReaderType &reader, const BytesViewNetwork &source) {
		BytesViewNetwork r(source);

		while (!r.empty()) {
			if (!decodeValue(reader, r)) {
				return false;
			}
		}

		return true;
	}
};

}

#endif /* CORE_CRYPTO_SPCRYPTOASN1_H_ */
