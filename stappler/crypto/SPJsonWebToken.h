/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CRYPTO_SPJSONWEBTOKEN_H_
#define STAPPLER_CRYPTO_SPJSONWEBTOKEN_H_

#include "SPCommon.h"

#if MODULE_STAPPLER_DATA

#include "SPString.h"
#include "SPTime.h"
#include "SPData.h"
#include "SPDataWrapper.h"
#include "SPCrypto.h"

namespace STAPPLER_VERSIONIZED stappler {

template <typename Interface>
struct SP_PUBLIC JsonWebToken {
	using String = typename Interface::StringType;
	using Bytes = typename Interface::BytesType;
	using Value = data::ValueTemplate<Interface>;

	enum SigAlg {
		None,
		HS256,
		HS512,
		RS256,
		RS512,
		ES256,
		ES512,
		GS256,
		GS512
	};

	static SigAlg getAlg(StringView);
	static StringView getAlgName(const SigAlg &);

	static JsonWebToken make(StringView iss, StringView aud, TimeInterval maxage = TimeInterval(), StringView sub = StringView());

	bool validate(StringView key) const;
	bool validate(BytesView key) const;
	bool validate(const crypto::PublicKey &key) const;

	bool validatePayload(StringView issuer, StringView aud) const;
	bool validatePayload() const;

	void setMaxAge(TimeInterval maxage);

	Value data() const;

	String exportPlain(data::EncodeFormat = data::EncodeFormat::Json) const;

	String exportSigned(SigAlg, StringView key,
			const CoderSource &passwd = CoderSource(), data::EncodeFormat = data::EncodeFormat::Json) const;
	String exportSigned(SigAlg, BytesView key,
			const CoderSource &passwd = CoderSource(), data::EncodeFormat = data::EncodeFormat::Json) const;
	String exportSigned(SigAlg, const crypto::PrivateKey &key, data::EncodeFormat = data::EncodeFormat::Json) const;
	String exportSigned(const crypto::PrivateKey &key, data::EncodeFormat = data::EncodeFormat::Json) const;

	JsonWebToken(Value &&payload, TimeInterval maxage = TimeInterval());
	JsonWebToken(const StringView &);

	String message;
	Value header;
	Value payload;
	Bytes sig;

	SigAlg alg = None;
	String kid;
};

template <typename Interface>
class SP_PUBLIC AesToken : public data::WrapperTemplate<Interface> {
public:
	using String = typename Interface::StringType;
	using Bytes = typename Interface::BytesType;
	using Value = data::ValueTemplate<Interface>;

	template <typename T>
	using Function = typename Interface::template FunctionType<T>;

	struct Keys {
		crypto::PublicKey *pub;
		crypto::PrivateKey *priv;
		BytesView secret;
	};

	// struct to pass identity data to fingerprinting algorithm
	struct Fingerprint {
		crypto::HashFunction func = crypto::HashFunction::GOST_3411;
		BytesView fpb;
		Function<void(const crypto::HashCoderCallback &)> cb;

		Fingerprint(crypto::HashFunction fn, BytesView v) : func(fn), fpb(v) { }
		Fingerprint(crypto::HashFunction fn, Function<void(const crypto::HashCoderCallback &)> &&cb) : func(fn), cb(sp::move(cb)) { }
	};

	// parse from JsonWebToken source
	static AesToken parse(StringView token, const Fingerprint &, StringView iss, StringView aud = StringView(), Keys = Keys());

	// parse from data::Value source
	static AesToken parse(const Value &, const Fingerprint &, Keys = Keys());

	static AesToken create(Keys = Keys());

	explicit operator bool () const;

	String exportToken(StringView iss, const Fingerprint &fpb, TimeInterval maxage, StringView sub) const;
	Value exportData(const Fingerprint &fpb) const;

protected:
	static std::array<uint8_t, 64> getFingerprint(const Fingerprint &, Time t, BytesView secret);

	Bytes encryptAes(const crypto::BlockKey256 &, const Value &) const;
	static Value decryptAes(const crypto::BlockKey256 &, BytesView);

	AesToken();
	AesToken(Keys keys);
	AesToken(Value &&, Keys keys);

	Keys _keys;
};

template <typename Interface>
auto JsonWebToken<Interface>::getAlg(StringView name) -> typename JsonWebToken<Interface>::SigAlg  {
	if (name == "HS256") {
		return HS256;
	} else if (name == "HS512") {
		return HS512;
	} else if (name == "RS256") {
		return RS256;
	} else if (name == "RS512") {
		return RS512;
	} else if (name == "ES256") {
		return ES256;
	} else if (name == "ES512") {
		return ES512;
	} else if (name == "GS256") {
		return GS256;
	} else if (name == "GS512") {
		return GS512;
	}
	return JsonWebToken::None;
}

template <typename Interface>
StringView JsonWebToken<Interface>::getAlgName(const SigAlg &alg) {
	switch (alg) {
	case None: return "none"; break;
	case HS256: return "HS256"; break;
	case HS512: return "HS512"; break;
	case RS256: return "RS256"; break;
	case RS512: return "RS512"; break;
	case ES256: return "ES256"; break;
	case ES512: return "ES512"; break;
	case GS256: return "GS256"; break;
	case GS512: return "GS512"; break;
	}
	return StringView();
}

template <typename Interface>
JsonWebToken<Interface> JsonWebToken<Interface>::make(StringView iss, StringView aud, TimeInterval maxage, StringView sub) {
	Value payload;
	payload.setString(iss, "iss");
	if (!sub.empty()) {
		payload.setString(sub, "sub");
	}

	if (!aud.empty()) {
		payload.setString(aud, "aud");
	}

	return JsonWebToken(move(payload), maxage);
}

template <typename Interface>
JsonWebToken<Interface>::JsonWebToken(Value &&val, TimeInterval maxage) : payload(move(val)) {
	if (maxage != nullptr) {
		payload.setInteger((Time::now() + maxage).toSeconds(), "exp");
	}
}

template <typename Interface>
JsonWebToken<Interface>::JsonWebToken(const StringView &token) {
	StringView r(token);
	auto head = r.readUntil<StringView::Chars<'.'>>();
	if (r.is('.')) {
		++ r;
	}

	header = data::read<Interface>(base64::decode<Interface>(head));
	for (auto &it : header.asDict()) {
		if (it.first == "alg") {
			alg = getAlg(it.second.getString());
		} else if (it.first == "kid") {
			kid = it.second.asString();
		}
	}

	auto pl = r.readUntil<StringView::Chars<'.'>>();

	message = String(token.data(), token.size() - r.size());

	if (r.is('.')) {
		++ r;
	}

	payload = data::read<Interface>(base64::decode<Interface>(pl));
	sig = base64::decode<Interface>(r);
}

template <typename Interface>
void JsonWebToken<Interface>::setMaxAge(TimeInterval maxage) {
	payload.setInteger((Time::now() + maxage).toSeconds(), "exp");
}

template <typename Interface>
bool JsonWebToken<Interface>::validate(StringView key) const {
	return validate(BytesView((const uint8_t *)key.data(), key.size() + 1));
}

template <typename Interface>
bool JsonWebToken<Interface>::validate(BytesView key) const {
	if (key.empty()) {
		return false;
	}

	switch (alg) {
	case HS256: {
		auto keySig = string::Sha256::hmac(message, key);
		if (sig.size() == keySig.size() && memcmp(sig.data(), keySig.data(), sig.size()) == 0) {
			return true;
		}
		break;
	}
	case HS512: {
		auto keySig = string::Sha512::hmac(message, key);
		if (sig.size() == keySig.size() && memcmp(sig.data(), keySig.data(), sig.size()) == 0) {
			return true;
		}
		break;
	}
	default: {
		crypto::PublicKey pk(key);

		if (pk) {
			return validate(pk);
		}
		break;
	}
	}

	return false;
}

template <typename Interface>
bool JsonWebToken<Interface>::validate(const crypto::PublicKey &pk) const {
	if (!pk) {
		return false;
	}

	crypto::SignAlgorithm algo = crypto::SignAlgorithm::RSA_SHA512;
	switch (alg) {
	case RS256: algo = crypto::SignAlgorithm::RSA_SHA256; break;
	case ES256: algo = crypto::SignAlgorithm::ECDSA_SHA256; break;
	case GS256: algo = crypto::SignAlgorithm::GOST_256; break;
	case RS512: algo = crypto::SignAlgorithm::RSA_SHA512; break;
	case ES512: algo = crypto::SignAlgorithm::ECDSA_SHA512; break;
	case GS512: algo = crypto::SignAlgorithm::GOST_512; break;
	default: return false; break;
	}

	if (pk.verify(BytesView((const uint8_t *)message.data(), message.size()), sig, algo)) {
		return true;
	}

	return false;
}

template <typename Interface>
bool JsonWebToken<Interface>::validatePayload(StringView issuer, StringView aud) const {
	auto exp = payload.getInteger("exp");
	if (issuer == payload.getString("iss") && aud == payload.getString("aud")
			&& (exp == 0 || exp > int64_t(Time::now().toSeconds()))) {
		return true;
	}
	return false;
}

template <typename Interface>
bool JsonWebToken<Interface>::validatePayload() const {
	auto exp = payload.getInteger("exp");
	if (exp == 0 || exp > int64_t(Time::now().toSeconds())) {
		return true;
	}
	return false;
}

template <typename Interface>
auto JsonWebToken<Interface>::data() const -> Value {
	Value data;
	data.setValue(header, "header");
	data.setValue(payload, "payload");
	data.setBytes(sig, "sig");
	return data;
}

template <typename Interface>
auto JsonWebToken<Interface>::exportPlain(data::EncodeFormat format) const -> String {
	return string::toString<Interface>(
			base64url::encode(data::write(header, format)), ".", base64url::encode(data::write(payload, format)));
}

template <typename Interface>
auto JsonWebToken<Interface>::exportSigned(SigAlg alg, StringView key,
		const CoderSource &passwd, data::EncodeFormat format) const -> String {
	return exportSigned(alg, BytesView((const uint8_t *)key.data(), key.size()), passwd, format);
}

template <typename Interface>
auto JsonWebToken<Interface>::exportSigned(SigAlg alg, BytesView key,
		const CoderSource &passwd, data::EncodeFormat format) const -> String {

	switch (alg) {
	case HS256: {
		Value hederData(header);
		hederData.setString(getAlgName(alg), "alg");

		auto data =  string::toString<Interface>(
				base64url::encode<Interface>(data::write<Interface>(hederData, format)),
				".", base64url::encode<Interface>(data::write<Interface>(payload, format)));

		return string::toString<Interface>(
				data, ".", base64url::encode<Interface>(string::Sha256::hmac(data, key)));
		break;
	}
	case HS512: {
		Value hederData(header);
		hederData.setString(getAlgName(alg), "alg");

		auto data =  string::toString<Interface>(
				base64url::encode<Interface>(data::write<Interface>(hederData, format)),
				".", base64url::encode<Interface>(data::write<Interface>(payload, format)));

		return string::toString<Interface>(
				data, ".", base64url::encode<Interface>(string::Sha512::hmac(data, key)));
		break;
	}
	default: {
		crypto::PrivateKey pk(key, passwd);
		if (pk) {
			return exportSigned(alg, pk, format);
		}
	}
	}

	return String();
}

template <typename Interface>
auto JsonWebToken<Interface>::exportSigned(SigAlg alg, const crypto::PrivateKey &pk, data::EncodeFormat format) const -> String {
	if (!pk) {
		return String();
	}

	Value hederData(header);
	hederData.setString(getAlgName(alg), "alg");

	auto data =  string::toString<Interface>(
			base64url::encode<Interface>(data::write<Interface>(hederData, format)),
			".", base64url::encode<Interface>(data::write<Interface>(payload, format)));

	crypto::SignAlgorithm algo = crypto::SignAlgorithm::RSA_SHA512;
	switch (alg) {
	case RS256: algo = crypto::SignAlgorithm::RSA_SHA256; break;
	case ES256: algo = crypto::SignAlgorithm::ECDSA_SHA256; break;
	case GS256: algo = crypto::SignAlgorithm::GOST_256; break;
	case RS512: algo = crypto::SignAlgorithm::RSA_SHA512; break;
	case ES512: algo = crypto::SignAlgorithm::ECDSA_SHA512; break;
	case GS512: algo = crypto::SignAlgorithm::GOST_512; break;
	default: break;
	}

	if (pk.sign([&] (BytesView sign) {
		data = string::toString<Interface>(data, ".", base64url::encode<Interface>(sign));
	}, data, algo)) {
		return data;
	}

	return String();
}

template <typename Interface>
auto JsonWebToken<Interface>::exportSigned(const crypto::PrivateKey &key, data::EncodeFormat fmt) const -> String {
	switch (key.getType()) {
	case crypto::KeyType::RSA:
		return exportSigned(RS512, key, fmt);
		break;
	case crypto::KeyType::ECDSA:
	case crypto::KeyType::EDDSA_ED448:
		return exportSigned(ES512, key, fmt);
		break;
	case crypto::KeyType::GOST3410_2012_256:
		return exportSigned(GS256, key, fmt);
		break;
	case crypto::KeyType::GOST3410_2012_512:
		return exportSigned(GS512, key, fmt);
		break;
	default:
		break;
	}
	return String();
}

template <typename Interface>
AesToken<Interface> AesToken<Interface>::parse(StringView token, const Fingerprint &fpb, StringView iss, StringView aud, Keys keys) {
	if (aud.empty()) {
		aud = iss;
	}

	JsonWebToken<Interface> input(token);
	if (input.validate(*keys.pub) && input.validatePayload(iss, aud)) {
		Time tf = Time::microseconds(input.payload.getInteger("tf"));
		auto fp = getFingerprint(fpb, tf, keys.secret);

		if (BytesView(fp.data(), fp.size()) == BytesView(input.payload.getBytes("fp"))) {
			auto v = crypto::getBlockInfo(input.payload.getBytes("p"));
			crypto::BlockKey256 aesKey;
			if (keys.priv) {
				aesKey = crypto::makeBlockKey(*keys.priv, BytesView(fp.data(), fp.size()), v.cipher, v.version);
			} else {
				aesKey = crypto::makeBlockKey(keys.secret, BytesView(fp.data(), fp.size()), v.cipher, v.version);
			}

			auto p = decryptAes(aesKey, input.payload.getBytes("p"));
			if (p) {
				return AesToken(move(p), keys);
			}
		}
	}
	return AesToken();
}

template <typename Interface>
AesToken<Interface> AesToken<Interface>::parse(const Value &payload, const Fingerprint &fpb, Keys keys) {
	Time tf = Time::microseconds(payload.getInteger("tf"));
	auto fp = getFingerprint(fpb, tf, keys.secret);

	if (BytesView(fp.data(), fp.size()) == BytesView(payload.getBytes("fp"))) {
		auto v = crypto::getBlockInfo(payload.getBytes("p"));

		crypto::BlockKey256 aesKey;
		if (keys.priv) {
			aesKey = crypto::makeBlockKey(*keys.priv, BytesView(fp.data(), fp.size()), v.cipher, v.version);
		} else {
			aesKey = crypto::makeBlockKey(keys.secret, BytesView(fp.data(), fp.size()), v.cipher, v.version);
		}

		auto p = decryptAes(aesKey, payload.getBytes("p"));
		if (p) {
			return AesToken(move(p), keys);
		}
	}
	return AesToken();
}

template <typename Interface>
AesToken<Interface> AesToken<Interface>::create(Keys keys) {
	return AesToken(keys);
}

template <typename Interface>
AesToken<Interface>::operator bool () const {
	return !this->_data.isNull() && _keys.priv && *_keys.priv && !_keys.secret.empty();
}

template <typename Interface>
auto AesToken<Interface>::exportToken(StringView iss, const Fingerprint &fpb, TimeInterval maxage, StringView sub) const -> String {
	auto t = Time::now();

	JsonWebToken<Interface> token = JsonWebToken<Interface>::make(iss, iss, maxage, sub);
	auto fp = getFingerprint(fpb, t, _keys.secret);

	token.payload.setBytes(BytesView(fp.data(), fp.size()), "fp");
	token.payload.setInteger(t.toMicros(), "tf");

	crypto::BlockKey256 aesKey;
	if (_keys.priv) {
		switch (_keys.priv->getType()) {
		case crypto::KeyType::GOST3410_2012_256:
		case crypto::KeyType::GOST3410_2012_512:
			aesKey = crypto::makeBlockKey(*_keys.priv, BytesView(fp.data(), fp.size()), crypto::BlockCipher::Gost3412_2015_CTR_ACPKM);
			break;
		default:
			aesKey = crypto::makeBlockKey(*_keys.priv, BytesView(fp.data(), fp.size()));
			break;
		}
	} else {
		aesKey = crypto::makeBlockKey(_keys.secret, BytesView(fp.data(), fp.size()));
	}

	token.payload.setBytes(encryptAes(aesKey, this->_data), "p");
	return token.exportSigned(*_keys.priv, data::EncodeFormat::Cbor);
}

template <typename Interface>
auto AesToken<Interface>::exportData(const Fingerprint &fpb) const -> Value {
	auto t = Time::now();
	auto fp = getFingerprint(fpb, t, _keys.secret);

	Value payload;
	payload.setBytes(BytesView(fp.data(), fp.size()), "fp");
	payload.setInteger(t.toMicros(), "tf");

	crypto::BlockKey256 aesKey;
	if (_keys.priv) {
		switch (_keys.priv->getType()) {
		case crypto::KeyType::GOST3410_2012_256:
		case crypto::KeyType::GOST3410_2012_512:
			aesKey = crypto::makeBlockKey(*_keys.priv, BytesView(fp.data(), fp.size()), crypto::BlockCipher::Gost3412_2015_CTR_ACPKM);
			break;
		default:
			aesKey = crypto::makeBlockKey(*_keys.priv, BytesView(fp.data(), fp.size()));
			break;
		}
	} else {
		aesKey = crypto::makeBlockKey(_keys.secret, BytesView(fp.data(), fp.size()));
	}

	payload.setBytes(encryptAes(aesKey, this->_data), "p");
	return payload;
}

template <typename Interface>
std::array<uint8_t, 64> AesToken<Interface>::getFingerprint(const Fingerprint &fp, Time t, BytesView secret) {
	auto v = byteorder::HostToBig(t.toMicros());
	if (!fp.fpb.empty()) {
		switch (fp.func) {
		case crypto::HashFunction::SHA_2:
			return crypto::Sha512().update(secret).update(fp.fpb).update(CoderSource((const uint8_t *)&v, sizeof(v))) .final();
			break;
		case crypto::HashFunction::GOST_3411:
			return crypto::Gost3411_512().update(secret).update(fp.fpb).update(CoderSource((const uint8_t *)&v, sizeof(v))) .final();
			break;
		}
	} else if (fp.cb) {
		return crypto::hash512([&] (const crypto::HashCoderCallback &cb) {
			fp.cb(cb);
		}, fp.func);
	} else {
		switch (fp.func) {
		case crypto::HashFunction::SHA_2:
			return crypto::Sha512().update(secret).update(CoderSource((const uint8_t *)&v, sizeof(v))).final();
			break;
		case crypto::HashFunction::GOST_3411:
			return crypto::Gost3411_512().update(secret).update(CoderSource((const uint8_t *)&v, sizeof(v))).final();
			break;
		}
	}
	return std::array<uint8_t, 64>();
}

template <typename Interface>
auto AesToken<Interface>::encryptAes(const crypto::BlockKey256 &key, const Value &val) const -> Bytes {
	auto d = data::write<Interface>(val, data::EncodeFormat::CborCompressed);
	Bytes out;
	crypto::encryptBlock(key, d, [&] (BytesView data) {
		out = data.bytes<Interface>();
	});
	return out;
}

template <typename Interface>
auto AesToken<Interface>::decryptAes(const crypto::BlockKey256 &key, BytesView val) -> Value {
	Value out;
	crypto::decryptBlock(key, val, [&] (BytesView data) {
		out = data::read<Interface>(data);
	});
	return out;
}

template <typename Interface>
AesToken<Interface>::AesToken() { }

template <typename Interface>
AesToken<Interface>::AesToken(Keys keys) : _keys(keys) { }

template <typename Interface>
AesToken<Interface>::AesToken(Value &&v, Keys keys) : data::WrapperTemplate<Interface>(sp::move(v)), _keys(keys) { }

}

#else

#warning "Enable module stappler_data to use JsonWebToken"

#endif // MODULE_STAPPLER_DATA

#endif /* STAPPLER_CRYPTO_SPJSONWEBTOKEN_H_ */
