/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_UTILS_SPHTMLPARSER_H_
#define STAPPLER_CORE_UTILS_SPHTMLPARSER_H_

#include "SPString.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::html {

enum class ParserFlags : uint32_t {
	None = 0,

	// Parse only root tag content, stop when it's closed
	RootOnly = 1 << 0,

	// do not parse single-quoted content as opaque
	IgnoreSingleQuote = 1 << 1,

	// do not parse double-quoted content as opaque
	IgnoreDoubleQuote = 1 << 2,

	Relaxed = IgnoreSingleQuote | IgnoreDoubleQuote
};

SP_DEFINE_ENUM_AS_MASK(ParserFlags)

/* Reader sample:
struct Reader {
	using Parser = html::Parser<Reader>;
	using Tag = Parser::Tag;
	using StringReader = Parser::StringReader;

	inline void onBeginTag(Parser &p, Tag &tag) {
		log::source().debug("onBeginTag", tag.name);
	}

	inline void onEndTag(Parser &p, Tag &tag, bool isClosable) {
		log::source().debug("onEndTag", tag.name);
	}

	inline void onTagAttribute(Parser &p, Tag &tag, StringReader &name, StringReader &value) {
		log::source().debug("onTagAttribute", tag.name, ": ", name, " = ", value);
	}

	inline void onPushTag(Parser &p, Tag &tag) {
		log::source().debug("onPushTag", tag.name);
	}

	inline void onPopTag(Parser &p, Tag &tag) {
		log::source().debug("onPopTag", tag.name);
	}

	inline void onInlineTag(Parser &p, Tag &tag) {
		log::source().debug("onInlineTag", tag.name);
	}

	inline void onTagContent(Parser &p, Tag &tag, StringReader &s) {
		log::source().debug("onTagContent", tag.name, ": ", s);
	}
};
*/

template <typename StringReader>
struct Tag;

template <typename ReaderType, typename StringReader = StringViewUtf8,
		typename TagType = typename std::conditional<
				std::is_same<typename ReaderType::Tag, html::Tag<StringReader>>::value,
				html::Tag<StringReader>, typename ReaderType::Tag >::type>
void parse(ReaderType &r, const StringReader &s, ParserFlags = ParserFlags::None);

template <typename T>
struct ParserTraits {
	using success = char;
	using failure = long;

	InvokerCallTest_MakeCallTest(onBeginTag, success, failure);
	InvokerCallTest_MakeCallTest(onEndTag, success, failure);
	InvokerCallTest_MakeCallTest(onTagAttribute, success, failure);
	InvokerCallTest_MakeCallTest(onPushTag, success, failure);
	InvokerCallTest_MakeCallTest(onPopTag, success, failure);
	InvokerCallTest_MakeCallTest(onInlineTag, success, failure);
	InvokerCallTest_MakeCallTest(onTagContent, success, failure);
	InvokerCallTest_MakeCallTest(onReadTagName, success, failure);
	InvokerCallTest_MakeCallTest(onReadAttributeName, success, failure);
	InvokerCallTest_MakeCallTest(onReadAttributeValue, success, failure);
	InvokerCallTest_MakeCallTest(shouldParseTag, success, failure);

	InvokerCallTest_MakeCallTest(onSchemeTag, success,
			failure); // tags like <?tag ""?> or <!TAG tag>
	InvokerCallTest_MakeCallTest(onCommentTag, success, failure); // tags like <!-- -->
	InvokerCallTest_MakeCallTest(onTagAttributeList, success,
			failure); // string with all attributes

	InvokerCallTest_MakeCallTest(readTagContent, success,
			failure); // replace default content reader
};

template <typename StringReader>
SP_PUBLIC auto Tag_readName(StringReader &is) -> StringReader;

template <typename StringReader>
SP_PUBLIC auto Tag_readAttrName(StringReader &s) -> StringReader;

template <typename StringReader>
SP_PUBLIC auto Tag_readAttrValue(StringReader &s) -> StringReader;

template <typename __StringReader>
struct Tag {
	using StringReader = __StringReader;

	Tag(const StringReader &n) : name(n) {
		if (name.is('!')) {
			closable = false;
		}
	}

	const StringReader &getName() const { return name; }

	void setClosable(bool v) { closable = v; }
	bool isClosable() const { return closable; }

	void setHasContent(bool v) { content = v; }
	bool hasContent() const { return content; }

	bool isNestedTagsAllowed() const { return nestedTagsAllowed; }

	StringReader name;
	bool closable = true;
	bool content = false;
	bool nestedTagsAllowed = true;
};

template <typename ReaderType, typename __StringReader = StringViewUtf8,
		typename TagType = typename html::Tag<__StringReader>,
		typename Traits = ParserTraits<ReaderType>>
struct Parser {
	using StringReader = __StringReader;
	using OrigCharType = typename StringReader::CharType;
	using CharType = typename StringReader::MatchCharType;
	using Tag = TagType;

	template <CharType... Args>
	using Chars = sprt::chars::Chars<CharType, Args...>;

	template <CharType First, CharType Last>
	using Range = sprt::chars::Chars<CharType, First, Last>;

	using GroupId = CharGroupId;

	template <GroupId G>
	using Group = sprt::chars::CharGroup<CharType, G>;

	using LtChar = Chars<CharType('<')>;

	Parser(ReaderType &r) : reader(&r) { tagStack.reserve_block_optimal(); }

	inline void cancel() {
		current.clear();
		canceled = true;
	}

	template <CharType C>
	void skipQuoted() {
		if (current.template is<C>()) {
			++current;
		}
		while (!current.empty() && !current.template is<C>()) {
			current.template skipUntil<Chars<CharType('\\'), C>>();
			if (current.is('\\')) {
				current += 2;
			}
		}
		if (current.template is<C>()) {
			++current;
		}
	}

	bool parse(const StringReader &r, ParserFlags f) {
		flags = f;
		current = r;
		while (!current.empty()) {
			auto prefix = readTagContent();
			if (!prefix.empty()) {
				if (!tagStack.empty()) {
					tagStack.back().setHasContent(true);
					onTagContent(tagStack.back(), prefix);
				} else {
					StringReader r;
					Tag t(r);
					t.setHasContent(true);
					onTagContent(t, prefix);
				}
			}

			if (!current.is('<')) {
				break; // next tag not found
			}

			++current; // drop '<'
			if (current.is('/')) { // close some parsed tag
				++current; // drop '/'

				auto tag = current.template readUntil<Chars<CharType('>')>>();
				if (!tag.empty() && current.is('>') && !tagStack.empty()) {
					tag.template trimChars<typename StringReader::WhiteSpace>();
					auto it = tagStack.end();
					do {
						--it;
						auto &name = it->getName();
						if (tag.size() == name.size() && tag.equals(name.data(), name.size())) {
							// close all tag after <tag>
							auto nit = tagStack.end();
							do {
								--nit;
								onPopTag(*nit);
								tagStack.pop_back();
							} while (nit != it);
							break;
						}
					} while (it != tagStack.begin());

					if (hasFlag(flags, ParserFlags::RootOnly) && tagStack.empty()) {
						if (current.is('>')) {
							++current; // drop '>'
						}
						break;
					}
				} else if (current.empty()) {
					break; // fail to parse tag
				}
				++current; // drop '>'
			} else {
				auto name = onReadTagName(current);
				if (name.empty()) { // found tag without readable name
					current.template skipUntil<Chars<CharType('>')>>();
					if (current.is('>')) {
						current++;
					}
					continue;
				}

				if constexpr (sizeof(OrigCharType) == 2) {
					if (name.prefix(u"!--", u"!--"_len)) { // process comment
						current.skipUntilString(u"-->", true);
						onCommentTag(StringReader(name.data() + u"!--"_len,
								current.data() - name.data() - u"!--"_len));
						current += u"!--"_len;
						continue;
					}
				} else {
					if (name.prefix("!--", "!--"_len)) { // process comment
						current.skipUntilString("-->", true);
						auto tmp = StringReader(name.data() + "!--"_len,
								current.data() - name.data() - "!--"_len);
						onCommentTag(tmp);
						current += "!--"_len;
						continue;
					}
				}

				if (name.is('!') || name.is('?')) {
					StringReader cdata;
					if constexpr (sizeof(OrigCharType) == 2) {
						if (current.starts_with(u"CDATA[")) {
							cdata = current.readUntilString(u"]]>");
							cdata += "CDATA["_len;
							current += "]]>"_len;
						}
					} else {
						if (current.starts_with("CDATA[")) {
							cdata = current.readUntilString("]]>");
							cdata += "CDATA["_len;
							current += "]]>"_len;
						}
					}

					if (!cdata.empty()) {
						if (!tagStack.empty()) {
							tagStack.back().setHasContent(true);
							onTagContent(tagStack.back(), cdata);
						} else {
							StringReader r;
							Tag t(r);
							t.setHasContent(true);
							onTagContent(t, cdata);
						}
						continue;
					} else {
						current.template skipChars<typename StringReader::WhiteSpace>();
						auto tmp = current;
						while (!current.empty() && !current.is('>')) {
							current.template skipUntil<
									Chars<CharType('>'), CharType('"'), CharType('\'')>>();
							if (current.is('\'')) {
								skipQuoted<CharType('\'')>();
							} else if (current.is('"')) {
								skipQuoted<CharType('"')>();
							}
						}
						if (current.is('>')) {
							auto tag = StringReader(tmp.data(), current.data() - tmp.data());
							onSchemeTag(name, tag);
							++current;
						}
						continue;
					}
				}

				TagType tag(name);
				onBeginTag(tag);

				StringReader attrStart = current;
				StringReader attrName;
				StringReader attrValue;
				while (!current.empty() && !current.is('>') && !current.is('/')) {
					attrName.clear();
					attrValue.clear();

					attrName = onReadAttributeName(current);
					if (attrName.empty()) {
						continue;
					}

					attrValue = onReadAttributeValue(current);
					onTagAttribute(tag, attrName, attrValue);
				}

				attrStart = StringReader(attrStart.data(), current.data() - attrStart.data());
				attrStart.template trimChars<typename StringReader::WhiteSpace>();
				if (!attrStart.empty()) {
					onTagAttributeList(tag, attrStart);
				}

				if (current.is('/')) {
					tag.setClosable(false);
				}

				current.template skipUntil<Chars<CharType('>')>>();
				if (current.is('>')) {
					++current;
				}

				onEndTag(tag, !tag.isClosable());
				if (tag.isClosable()) {
					onPushTag(tag);
					tagStack.emplace_back(sp::move(tag));
					if (!shouldParseTag(tag)) {
						auto start = current;
						while (!current.empty()) {
							current.template skipUntil<Chars<CharType('<')>>();
							if (current.is('<')) {
								auto tmp = current.sub(1);
								if (tmp.is('/')) {
									++tmp;
									if (tmp.starts_with(tag.name)) {
										tmp += tag.name.size();
										tmp.template skipChars<Group<GroupId::WhiteSpace>>();
										if (tmp.is('>')) {
											StringReader content(start.data(),
													current.data() - start.data());
											if (!content.empty()) {
												onTagContent(tag, content);
											}
											onPopTag(tag);
											tagStack.pop_back();

											++tmp;
											current = tmp;
											break;
										}
									}
								}
								++current;
							}
						}
					}
				} else {
					onInlineTag(tag);
				}
			}
		}

		if (!tagStack.empty()) {
			auto nit = tagStack.end();
			do {
				nit--;
				onPopTag(*nit);
				tagStack.pop_back();
			} while (nit != tagStack.begin());
		}

		return !canceled;
	}

	StringReader readTagContent() {
		auto tmp = current;

		if constexpr (Traits::readTagContent) {
			if (!tagStack.empty()) {
				reader->readTagContent(*this, tagStack.back(), current);
				return StringReader(tmp.data(), current.data() - tmp.data());
			}
		}

		bool nestedAllowed = true;
		if (!tagStack.empty()) {
			nestedAllowed = tagStack.back().isNestedTagsAllowed();
		}

		enum class ParseMode {
			All,
			Single,
			Double,
			None
		} mode;

		if (hasFlag(flags, ParserFlags::IgnoreSingleQuote)
				&& hasFlag(flags, ParserFlags::IgnoreDoubleQuote)) {
			mode = ParseMode::None;
		} else if (hasFlag(flags, ParserFlags::IgnoreSingleQuote)) {
			mode = ParseMode::Double;
		} else if (hasFlag(flags, ParserFlags::IgnoreDoubleQuote)) {
			mode = ParseMode::Single;
		} else {
			mode = ParseMode::All;
		}

		while (!current.empty() && !current.is('<')) {
			switch (mode) {
			case ParseMode::All:
				current.template skipUntil< Chars<CharType('<'), CharType('\''), CharType('"')>>();
				break;
			case ParseMode::Single:
				current.template skipUntil< Chars<CharType('<'), CharType('\'')>>();
				break;
			case ParseMode::Double:
				current.template skipUntil< Chars<CharType('<'), CharType('"')>>();
				break;
			case ParseMode::None: current.template skipUntil< Chars<CharType('<')>>(); break;
			}

			if (current.is('\'') && !hasFlag(flags, ParserFlags::IgnoreSingleQuote)) {
				skipQuoted<CharType('\'')>();
			} else if (current.is('"') && !hasFlag(flags, ParserFlags::IgnoreDoubleQuote)) {
				skipQuoted<CharType('"')>();
			} else if (!nestedAllowed && current.is('<')) {
				if (current[1] == '/') {
					auto tag = current.sub(2);
					if (tag.starts_with(tagStack.back().name)
							&& tag[tagStack.back().name.size()] == '>') {
						break;
					}
				}
				++current;
			}
		}

		return StringReader(tmp.data(), current.data() - tmp.data());
	}

	inline StringReader onReadTagName(StringReader &str) {
		if constexpr (Traits::onReadTagName) {
			StringReader ret(str);
			reader->onReadTagName(*this, ret);
			return ret;
		} else {
			return Tag_readName(str);
		}
	}

	inline StringReader onReadAttributeName(StringReader &str) {
		if constexpr (Traits::onReadAttributeName) {
			StringReader ret(str);
			reader->onReadAttributeName(*this, ret);
			return ret;
		} else {
			return Tag_readAttrName(str);
		}
	}

	inline StringReader onReadAttributeValue(StringReader &str) {
		if constexpr (Traits::onReadAttributeValue) {
			StringReader ret(str);
			reader->onReadAttributeValue(*this, ret);
			return ret;
		} else {
			return Tag_readAttrValue(str);
		}
	}

	inline void onBeginTag(TagType &tag) {
		if constexpr (Traits::onBeginTag) {
			reader->onBeginTag(*this, tag);
		}
	}
	inline void onEndTag(TagType &tag, bool isClosed) {
		if constexpr (Traits::onEndTag) {
			reader->onEndTag(*this, tag, isClosed);
		}
	}
	inline void onTagAttribute(TagType &tag, StringReader &name, StringReader &value) {
		if constexpr (Traits::onTagAttribute) {
			reader->onTagAttribute(*this, tag, name, value);
		}
	}
	inline void onPushTag(TagType &tag) {
		if constexpr (Traits::onPushTag) {
			reader->onPushTag(*this, tag);
		}
	}
	inline void onPopTag(TagType &tag) {
		if constexpr (Traits::onPopTag) {
			reader->onPopTag(*this, tag);
		}
	}
	inline void onInlineTag(TagType &tag) {
		if constexpr (Traits::onInlineTag) {
			reader->onInlineTag(*this, tag);
		}
	}
	inline void onTagContent(TagType &tag, StringReader &s) {
		if constexpr (Traits::onTagContent) {
			reader->onTagContent(*this, tag, s);
		}
	}
	inline bool shouldParseTag(TagType &tag) {
		if constexpr (Traits::shouldParseTag) {
			return reader->shouldParseTag(*this, tag);
		}
		return true;
	}
	inline void onSchemeTag(StringReader &name, StringReader &value) {
		if constexpr (Traits::onSchemeTag) {
			return reader->onSchemeTag(*this, name, value);
		}
	}
	inline void onCommentTag(StringReader &comment) {
		if constexpr (Traits::onCommentTag) {
			return reader->onCommentTag(*this, comment);
		}
	}
	inline void onTagAttributeList(TagType &tag, StringReader &data) {
		if constexpr (Traits::onTagAttributeList) {
			reader->onTagAttributeList(*this, tag, data);
		}
	}

	bool canceled = false;
	ParserFlags flags = ParserFlags::None;
	ReaderType *reader;
	StringReader current;
	memory::vector<TagType> tagStack;
};

template <typename ReaderType, typename StringReader, typename TagType>
void parse(ReaderType &r, const StringReader &s, ParserFlags flags) {
	html::Parser<ReaderType, StringReader, TagType> p(r);
	p.parse(s, flags);
}

} // namespace stappler::html

#endif /* STAPPLER_CORE_UTILS_SPHTMLPARSER_H_ */
