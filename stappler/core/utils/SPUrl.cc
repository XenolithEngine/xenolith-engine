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

#include "SPUrl.h"
#include "SPString.h"
#include "SPStringView.h"
#include "SPSharedModule.h"

#include "SPIdn.h"

namespace STAPPLER_VERSIONIZED stappler {

using Scheme = chars::Compose<char, chars::CharGroup<char, CharGroupId::Alphanumeric>,
		chars::Chars<char, '+', '-', '.'>>;
using Ipv6 = chars::Compose<char, chars::CharGroup<char, CharGroupId::Hexadecimial>,
		chars::Chars<char, ':'>>;

using Unreserved = chars::Compose<char, chars::CharGroup<char, CharGroupId::Alphanumeric>,
		chars::Chars<char, '-', '.', '_', '~', '%'>>;
using SubDelim = chars::Chars<char, '!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '='>;
using GenDelim = chars::Chars<char, ':', '/', '?', '#', '[', ']', '@'>;

using UnreservedUni = chars::Compose<char, Unreserved, chars::UniChar>;

bool UrlView::validateScheme(const StringView &r) {
	auto cpy = r;
	if (cpy.is<StringView::Compose<StringView::CharGroup<CharGroupId::Alphanumeric>,
					StringView::Chars<'.'>>>()) {
		cpy++;
		cpy.skipChars<Scheme>();
		if (cpy.empty()) {
			return true;
		}
	}
	return false;
}

bool UrlView::validateHost(StringView &r) {
	if (r.empty()) {
		return false;
	}
	auto cpy = r;
	if (cpy.is('[')) {
		// check ipv6
		++cpy;
		cpy.skipChars<Ipv6>();
		if (cpy.is(']')) {
			cpy++;
			if (cpy.empty()) {
				return true;
			}
		}
	} else {
		cpy.skipChars<Unreserved, SubDelim, chars::UniChar>();
		if (cpy.empty()) {
			auto c = r.sub(r.size() - 1, 1);
			while (c.is(',') || c.is('.') || c.is(';')) {
				r = r.sub(0, r.size() - 1);
				c = r.sub(r.size() - 1, 1);
			}
			if (c.is<StringView::Compose<StringView::CharGroup<CharGroupId::Alphanumeric>,
							chars::UniChar>>()) {
				StringView h(r);

				if (!h.empty()) {
					StringView domain;
					while (!h.empty()) {
						domain = h.readUntil<StringView::Chars<'.'>>();
						if (h.is('.')) {
							++h;
						}

						if (domain.empty()) {
							return false;
						}
					}

					if (!domain.empty()) {
						auto tmp = domain;
						tmp.skipChars<StringView::CharGroup<CharGroupId::Alphanumeric>>();
						if (!tmp.empty()) {
							if (idn::isKnownTld(domain)) {
								return true;
							}
						} else {
							return true;
						}
					}
				} else {
					return true;
				}
			}
		}
	}
	return false;
}

bool UrlView::validateUserOrPassword(const StringView &r) {
	auto cpy = r;
	cpy.skipChars<Unreserved, SubDelim, chars::UniChar>();
	if (cpy.empty()) {
		return true;
	}
	return false;
}

bool UrlView::parseUrl(StringView &s, const Callback<void(StringViewUtf8, UrlView::UrlToken)> &cb) {
	UrlView::UrlToken state = UrlView::UrlToken::Scheme;

	StringView tmp;
	if (s.is('[')) {
		state = UrlView::UrlToken::Host;
	} else if (s.is("mailto:")) {
		cb(StringView(s, 6), UrlView::UrlToken::Scheme);
		s += 6;
		cb(StringView(s, 1), UrlView::UrlToken::Blank);
		++s;
		state = UrlView::UrlToken::User;
	} else {
		tmp = s.readChars<UnreservedUni>();
	}

	if (state == UrlView::UrlToken::Scheme) {
		if (s.is(':')) {
			// scheme or host+port
			if (tmp.empty()) {
				return false; // error
			}

			if (s.is("://")) {
				if (!validateScheme(tmp)) {
					return false;
				}

				cb(tmp, UrlView::UrlToken::Scheme);
				cb(StringView(s, 3), UrlView::UrlToken::Blank);
				s += 3;

				if (s.is('[')) {
					state = UrlView::UrlToken::Host;
				} else {
					state = UrlView::UrlToken::User;
				}
			} else {
				// if it's port, next chars will be numbers only
				auto tmpS = s;
				tmpS++;
				auto port = tmpS.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
				if (!port.empty() && !tmpS.is<UnreservedUni>() && !tmpS.is('@')) {
					// host + port
					if (!validateHost(tmp)) {
						return true;
					}

					cb(tmp, UrlView::UrlToken::Host);
					cb(StringView(port.data() - 1, 1), UrlView::UrlToken::Blank);
					cb(port, UrlView::UrlToken::Port);
					s = tmpS;

					if (s.is('/')) {
						state = UrlView::UrlToken::Path;
					} else if (s.is<StringView::Chars<'?'>>()) {
						state = UrlView::UrlToken::Query;
					} else if (s.is('#')) {
						state = UrlView::UrlToken::Fragment;
					} else {
						return true;
					}
				} else {
					tmpS = s;
					++tmpS;
					auto arg = tmpS.readChars<UnreservedUni, SubDelim>();
					if (tmpS.is('@')) {
						// username + password
						if (!validateUserOrPassword(tmp) || !validateUserOrPassword(arg)) {
							return false;
						}

						cb(tmp, UrlView::UrlToken::User);
						cb(StringView(arg.data() - 1, 1), UrlView::UrlToken::Blank);
						cb(arg, UrlView::UrlToken::Password);
						cb(StringView(tmpS, 1), UrlView::UrlToken::Blank);
						state = UrlView::UrlToken::Host;
						++tmpS;
						s = tmpS;
					} else {
						// scheme without authority segment
						if (!validateScheme(tmp)) {
							return false;
						}
						cb(tmp, UrlView::UrlToken::Scheme);
						state = UrlView::UrlToken::Path;
						cb(s.sub(0, 1), UrlView::UrlToken::Blank);
						++s;
					}
				}
			}
		} else if (s.is('@')) {
			if (tmp.empty() || !validateUserOrPassword(tmp)) {
				return false;
			}
			cb(tmp, UrlView::UrlToken::User);
			state = UrlView::UrlToken::Host;
			cb(StringView(s, 1), UrlView::UrlToken::Blank);
			++s;
		} else if (s.is('/')) {
			// host + path
			if (!tmp.empty()) {
				if (!validateHost(tmp)) {
					return false;
				}
				cb(tmp, UrlView::UrlToken::Host);
			}
			state = UrlView::UrlToken::Path;
		} else if (s.is('?')) {
			// host + query
			if (tmp.empty()) {
				return false;
			}
			if (!validateHost(tmp)) {
				return false;
			}
			cb(tmp, UrlView::UrlToken::Host);
			state = UrlView::UrlToken::Query;
		} else if (s.is('#')) {
			// host + fragment
			if (tmp.empty()) {
				return false;
			}
			if (!validateHost(tmp)) {
				return false;
			}
			cb(tmp, UrlView::UrlToken::Host);
			state = UrlView::UrlToken::Fragment;
		} else {
			// assume host-only
			if (!tmp.empty()) {
				if (!validateHost(tmp)) {
					return false;
				}
				cb(tmp, UrlView::UrlToken::Host);
				return true;
			}
			return false;
		}
	}

	if (state == UrlView::UrlToken::User) {
		auto tmp_s = s;
		tmp = tmp_s.readChars<UnreservedUni, SubDelim>();

		if (tmp_s.is('@')) {
			// user only part
			if (!validateUserOrPassword(tmp)) {
				return false;
			}
			cb(tmp, UrlView::UrlToken::User);
			state = UrlView::UrlToken::Host;
			cb(StringView(tmp_s, 1), UrlView::UrlToken::Blank);
			++tmp_s;
			s = tmp_s;
		} else if (tmp_s.is(':')) {
			// user + password or host + port
			tmp_s++;
			auto tmpS = tmp_s;

			// if it's port, next chars will be numbers only
			auto port = tmpS.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
			if (!port.empty() && !tmpS.is('@')) {
				// host + port
				if (!validateHost(tmp)) {
					return true;
				}

				cb(tmp, UrlView::UrlToken::Host);
				cb(StringView(port.data() - 1, 1), UrlView::UrlToken::Blank);
				cb(port, UrlView::UrlToken::Port);
				s = tmpS;

				if (s.is('/')) {
					state = UrlView::UrlToken::Path;
				} else if (s.is('?')) {
					state = UrlView::UrlToken::Query;
				} else if (s.is('#')) {
					state = UrlView::UrlToken::Fragment;
				} else {
					return true;
				}
			} else {
				// user + password
				if (!validateUserOrPassword(tmp)) {
					return false;
				}
				cb(tmp, UrlView::UrlToken::User);

				if (tmpS.is('@')) {
					cb(StringView(port.data() - 1, 1), UrlView::UrlToken::Blank);
					cb(port, UrlView::UrlToken::Password);
					++tmpS;
					state = UrlView::UrlToken::Host;
					s = tmpS;
				} else {
					tmp = tmp_s.readChars<UnreservedUni, SubDelim>();
					if (!tmp_s.is('@')) {
						return false;
					}
					++tmp_s;
					if (!validateUserOrPassword(tmp)) {
						return false;
					}
					cb(StringView(tmp.data() - 1, 1), UrlView::UrlToken::Blank);
					cb(tmp, UrlView::UrlToken::Password);
					s = tmp_s;
					cb(StringView(s.data() - 1, 1), UrlView::UrlToken::Blank);
					state = UrlView::UrlToken::Host;
				}
			}
		} else {
			// host
			if (!validateHost(tmp)) {
				return false;
			}

			cb(tmp, UrlView::UrlToken::Host);
			s = tmp_s;

			if (tmp_s.is('/')) {
				state = UrlView::UrlToken::Path;
			} else if (tmp_s.is('?')) {
				state = UrlView::UrlToken::Query;
			} else if (tmp_s.is('#')) {
				state = UrlView::UrlToken::Fragment;
			} else {
				return true;
			}
		}
	}

	if (state == UrlView::UrlToken::Host) {
		bool stop = false;
		if (s.is('[')) {
			auto t = s;
			++t;
			tmp = t.readChars<UnreservedUni, SubDelim, StringView::Chars<':'>>();
			if (t.is(']')) {
				++t;
				tmp = StringView(s.data(), t.data() - s.data());
				s = t;
			} else {
				return false;
			}
		} else {
			tmp = s.readChars<UnreservedUni, SubDelim, StringView::Chars<'[', ']'>>();
		}
		if (!validateHost(tmp)) {
			return false;
		}
		cb(tmp, UrlView::UrlToken::Host);
		if (s.is(':')) {
			auto tmp2 = s;
			++tmp2;
			auto port = tmp2.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
			if (port.empty() || s.is<UnreservedUni>()) {
				state = UrlView::UrlToken::Path;
				stop = true;
			} else {
				cb(StringView(port.data() - 1, 1), UrlView::UrlToken::Blank);
				cb(port, UrlView::UrlToken::Port);
				s = tmp2;
			}
		}
		if (stop) {
			// do nothing
		} else if (s.is('/')) {
			state = UrlView::UrlToken::Path;
		} else if (s.is('?')) {
			state = UrlView::UrlToken::Query;
		} else if (s.is('#')) {
			state = UrlView::UrlToken::Fragment;
		} else {
			return true;
		}
	}

	if (state == UrlView::UrlToken::Path) {
		tmp = s.readChars<UnreservedUni, SubDelim, StringView::Chars<'/', ':', '@'>>();
		if (!tmp.empty()) {
			cb(tmp, UrlView::UrlToken::Path);
		}

		if (s.is('?')) {
			state = UrlView::UrlToken::Query;
		} else if (s.is('#')) {
			state = UrlView::UrlToken::Fragment;
		} else {
			return true;
		}
	}

	if (state == UrlView::UrlToken::Query) {
		tmp = s.readChars<UnreservedUni, SubDelim,
				StringView::Chars<'/', ':', '@', '?', '[', ']'>>();
		if (!tmp.empty()) {
			if (tmp.is('?')) {
				cb(StringView(tmp, 1), UrlView::UrlToken::Blank);
				++tmp;
			}
			if (!tmp.empty()) {
				cb(tmp, UrlView::UrlToken::Query);
			}
		}

		if (s.is('#')) {
			state = UrlView::UrlToken::Fragment;
		} else {
			return true;
		}
	}

	if (state == UrlView::UrlToken::Fragment) {
		tmp = s.readChars<UnreservedUni, SubDelim,
				StringView::Chars<'/', ':', '@', '?', '#', '[', ']'>>();
		if (!tmp.empty()) {
			if (tmp.is('#')) {
				cb(StringView(tmp, 1), UrlView::UrlToken::Blank);
				++tmp;
			}
			if (!tmp.empty()) {
				cb(tmp, UrlView::UrlToken::Fragment);
			}
		}
	}

	return true;
}

template <typename Vector>
auto _parsePath(StringView str, Vector &ret) {
	StringView s(str);
	do {
		if (s.is('/')) {
			s++;
		}
		auto path = s.readUntil<StringView::Chars<'/', '?', ';', '&', '#'>>();
		if (path == "..") {
			if (!ret.empty()) {
				ret.pop_back();
			}
		} else if (path == ".") {
			// skip this component
		} else {
			if (!path.empty()) {
				ret.push_back(str);
			}
		}
	} while (!s.empty() && s.is('/'));
}

template <>
auto UrlView::parsePath<memory::StandartInterface>(StringView str)
		-> memory::StandartInterface::VectorType<StringView> {
	memory::StandartInterface::VectorType<StringView> ret;
	_parsePath(str, ret);
	return ret;
}

template <>
auto UrlView::parsePath<memory::PoolInterface>(StringView str)
		-> memory::PoolInterface::VectorType<StringView> {
	memory::PoolInterface::VectorType<StringView> ret;
	_parsePath(str, ret);
	return ret;
}

UrlView::UrlView() { }

UrlView::UrlView(StringView str) { parse(str); }

void UrlView::clear() {
	scheme.clear();
	user.clear();
	password.clear();
	host.clear();
	port.clear();
	path.clear();
	query.clear();
	fragment.clear();
	url.clear();
}

bool UrlView::parse(const StringView &str) {
	StringView r(str);
	return parse(r);
}

bool UrlView::parse(StringView &str) {
	auto tmp = str;
	if (parseUrl(str, [this](StringView str, UrlToken tok) {
		switch (tok) {
		case UrlToken::Scheme: scheme = str; break;
		case UrlToken::User: user = str; break;
		case UrlToken::Password: password = str; break;
		case UrlToken::Host: host = str; break;
		case UrlToken::Port: port = str; break;
		case UrlToken::Path: path = str; break;
		case UrlToken::Query: query = str; break;
		case UrlToken::Fragment: fragment = str; break;
		case UrlToken::Blank: break;
		}
	})) {
		url = StringView(tmp.data(), str.data() - tmp.data());
		return true;
	} else {
		clear();
		return false;
	}
}

static void UrlView_get(std::ostream &stream, const UrlView &view) {
	if (!view.scheme.empty()) {
		stream << view.scheme << ":";
	}
	if (!view.scheme.empty() && !view.host.empty() && view.scheme != "mailto") {
		stream << "//";
	}
	if (!view.host.empty()) {
		if (!view.user.empty()) {
			stream << view.user;
			if (!view.password.empty()) {
				stream << ":" << view.password;
			}
			stream << "@";
		}
		stream << view.host;
		if (!view.port.empty()) {
			stream << ":" << view.port;
		}
	}
	if (!view.path.empty()) {
		stream << view.path;
	}
	if (!view.query.empty()) {
		stream << "?" << view.query;
	}
	if (!view.fragment.empty()) {
		stream << "#" << view.fragment;
	}
}

template <>
auto UrlView::get<memory::PoolInterface>() const -> memory::PoolInterface::StringType {
	memory::PoolInterface::StringStreamType stream;
	UrlView_get(stream, *this);
	return stream.str();
}

template <>
auto UrlView::get<memory::StandartInterface>() const -> memory::StandartInterface::StringType {
	memory::StandartInterface::StringStreamType stream;
	UrlView_get(stream, *this);
	return stream.str();
}

bool UrlView::isEmail() const {
	return (!user.empty() && !host.empty())
			&& (scheme.empty() && password.empty() && port.empty() && path.empty() && query.empty()
					&& fragment.empty());
}

bool UrlView::isPath() const {
	return !path.empty()
			&& (scheme.empty() && user.empty() && password.empty() && host.empty() && port.empty()
					&& query.empty() && fragment.empty());
}


#if MODULE_STAPPLER_DATA

template <>
auto UrlView::parseArgs<memory::PoolInterface>(StringView str, size_t maxVarSize)
		-> data::ValueTemplate<memory::PoolInterface> {
	if (str.empty()) {
		data::ValueTemplate<memory::PoolInterface>();
	}
	StringView r(str);
	if (r.front() == '?' || r.front() == '&' || r.front() == ';') {
		++r;
	}

	auto fn = SharedModule::acquireTypedSymbol<
			decltype(&data::readUrlencoded<memory::PoolInterface>)>(
			buildconfig::MODULE_STAPPLER_DATA_NAME, "readUrlencoded");
	if (!fn) {
		log::source().error("UrlView",
				"Module MODULE_STAPPLER_DATA declared, but not available in runtime");
	}
	return fn(str, maxVarSize);
}

template <>
auto UrlView::parseArgs<memory::StandartInterface>(StringView str, size_t maxVarSize)
		-> data::ValueTemplate<memory::StandartInterface> {
	if (str.empty()) {
		data::ValueTemplate<memory::StandartInterface>();
	}
	StringView r(str);
	if (r.front() == '?' || r.front() == '&' || r.front() == ';') {
		++r;
	}

	auto fn = SharedModule::acquireTypedSymbol<
			decltype(&data::readUrlencoded<memory::StandartInterface>)>(
			buildconfig::MODULE_STAPPLER_DATA_NAME, "readUrlencoded");
	if (!fn) {
		log::source().error("UrlView",
				"Module MODULE_STAPPLER_DATA declared, but not available in runtime");
	}
	return fn(str, maxVarSize);
}

#endif


} // namespace STAPPLER_VERSIONIZED stappler
