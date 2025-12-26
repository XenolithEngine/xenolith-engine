/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "SPCommandLineParser.h"

namespace STAPPLER_VERSIONIZED stappler::detail {

bool CommandLinePatternParsingData::parse() {
	while (!args.empty()) {
		if (!target.empty()) {
			if (!parsePatternString()) {
				return false;
			}
		}

		if (!parseWhitespace()) {
			return false;
		}
	}
	return true;
}

static StringView CommandLinePatternParsingData_parseInteger(StringView &str) {
	str.skipChars<StringView::WhiteSpace>();

	auto tmp = str;
	auto v = str.readInteger(10);
	if (v) {
		if (str.empty() || str.is<StringView::WhiteSpace>()) {
			return StringView(tmp.data(), str.data() - tmp.data());
		}
	}

	return StringView();
}

static StringView CommandLinePatternParsingData_parseFloat(StringView &str) {
	str.skipChars<StringView::WhiteSpace>();

	auto tmp = str;
	auto v = str.readFloat();
	if (v) {
		if (str.empty() || str.is<StringView::WhiteSpace>()) {
			return StringView(tmp.data(), str.data() - tmp.data());
		}
	}

	return StringView();
}

bool CommandLinePatternParsingData::parsePatternString() {
	auto str = args.readUntil<StringView::WhiteSpace, StringView::Chars<'<'>>();
	if (!str.empty()) {
		// match fixed block
		if (!target.starts_with(str)) {
			log::source().error("CommandLine",
					"Invalid option input: ", (offset > 0) ? argv[offset - 1] : target, " for ",
					type, pattern->pattern, pattern->args);
			return false;
		} else {
			target += str.size();
		}
	}
	if (args.is('<')) {
		++args;
		auto tpl = args.readUntil<StringView::Chars<'>'>>();
		if (args.is('>')) {
			++args;
			if (tpl == "#.#") {
				auto num = CommandLinePatternParsingData_parseFloat(target);
				if (!num.empty()) {
					result.emplace_back(StringView(num));
				} else {
					log::source().error("CommandLine",
							"Invalid option input: ", (offset > 0) ? argv[offset - 1] : target,
							" for ", type, pattern->pattern, pattern->args);
					return false;
				}
			} else if (tpl == "#") {
				auto num = CommandLinePatternParsingData_parseInteger(target);
				if (!num.empty()) {
					result.emplace_back(StringView(num));
				} else {
					log::source().error("CommandLine",
							"Invalid option input: ", (offset > 0) ? argv[offset - 1] : target,
							" for ", type, pattern->pattern, pattern->args);
					return false;
				}
			} else {
				StringView data;
				if (args.empty() || args.is<StringView::WhiteSpace>()) {
					data = target.readUntil<StringView::WhiteSpace>();
				} else {
					data = target.readUntilString(args.sub(0, 1));
				}
				if (!data.empty()) {
					result.emplace_back(StringView(data));
				} else {
					log::source().error("CommandLine",
							"Invalid option input: ", (offset > 0) ? argv[offset - 1] : target,
							" for ", type, pattern->pattern, pattern->args);
					return false;
				}
			}
		} else {
			log::source().error("CommandLine", "Invalid pattern: ", pattern->args, " for ", type,
					pattern->pattern);
			return false;
		}
	}
	return true;
}

bool CommandLinePatternParsingData::parseWhitespace() {
	if (args.is<StringView::WhiteSpace>()) {
		args.skipChars<StringView::WhiteSpace>();
		if (target.empty() || target.is<StringView::WhiteSpace>()) {
			target.skipChars<StringView::WhiteSpace>();
			if (target.empty()) {
				++offset;
				target = StringView(argv[offset - 1]);
				if (offset > argv.size() && !args.empty()) {
					log::source().error("CommandLine", "Not enough arguments for ", type,
							pattern->pattern, pattern->args);
					return false;
				}
			}
		} else {
			log::source().error("CommandLine",
					"Invalid option input: ", (offset > 0) ? argv[offset - 1] : target, " for ",
					type, pattern->pattern, pattern->args);
			return false;
		}
	} else if (!args.empty() && target.empty()) {
		return false;
	}
	return true;
}

CommandLineParserBase::~CommandLineParserBase() {
	if (_pool) {
		memory::pool::destroy(_pool);
	}
	if (_alloc) {
		memory::allocator::destroy(_alloc);
	}
	memory::pool::terminate();
}

CommandLineParserBase::CommandLineParserBase() {
	memory::pool::initialize();
	_alloc = memory::allocator::create();
	_pool = memory::pool::create(_alloc);

	mem_pool::perform([&] {
		_stringPatterns = new (std::nothrow) Vector<CommandLinePatternData>;
		_charPatterns = new (std::nothrow) Vector<CommandLinePatternData>;
		_options = new (std::nothrow) Vector<CommandLineParamData *>;
	}, _pool);
}

bool CommandLineParserBase::parse(void *output, int argc, const char *argv[],
		const Callback<void(void *, StringView)> &argCallback) const {
	if (argc == 0) {
		return false;
	}

	bool success = true;

	mem_pool::perform_temporary([&]() {
		Vector<StringView> argsVec;
		while (argc > 0) {
			argsVec.emplace_back(argv[0]);
			--argc;
			++argv;
		}

		size_t i = 0;
		while (i < argsVec.size()) {
			auto value = argsVec[i];
			if (value[0] == '-') {
				if (value[1] == '-') {
					StringView tmp = value.sub(2);
					auto init = tmp.readUntil<StringView::WhiteSpace, StringView::Chars<'='>>();

					auto it = std::lower_bound(_stringPatterns->begin(), _stringPatterns->end(),
							init);
					if (it != _stringPatterns->end() && it->pattern == init) {
						if (tmp.is('=')) {
							++tmp;
							i += parseStringPattern(output, *it, tmp, success);
						} else {
							i += parseStringPattern(output, *it, makeSpanView(argsVec).sub(i + 1),
									success);
						}
					} else {
						log::source().error("CommandLine", "Unknown command line option: --", init);
						success = false;
					}
				} else {
					StringView init = value.sub(1);
					while (!init.empty()) {
						auto it = std::lower_bound(_charPatterns->begin(), _charPatterns->end(),
								init.sub(0, 1));

						if (it != _charPatterns->end() && init.starts_with(it->pattern)) {
							init += it->pattern.size();
							i += parseCharPattern(output, *it, init,
									makeSpanView(argsVec).sub(i + 1), success);
						} else {
							log::source().error("CommandLine", "Unknown command line option: -",
									init.sub(0, 1));
							++init;
							success = false;
						}
					}
				}
			} else if (argCallback) {
				if (i == 0) {
					// first arg
#ifdef MODULE_COMMON_FILESYSTEM
					argCallback(output, filesystem::native::nativeToPosix<Interface>(value));
#else
					argCallback(output, value);
#endif
				} else {
					argCallback(output, value);
				}
			}
			++i;
		}
	}, _pool);

	return success;
}

void CommandLineParserBase::describe(const Callback<void(StringView)> &out) const {
	out << "Options:\n";

	for (auto &it : *_options) {
		out << "  ";
		bool front = true;
		for (auto &p : it->patterns) {
			if (!front) {
				out << ", ";
			} else {
				front = false;
			}
			out << p;
		}

		out << "\n     - " << it->description << "\n";
	}
}

void CommandLineParserBase::swap(CommandLineParserBase &other) {
	std::swap(_alloc, other._alloc);
	std::swap(_pool, other._pool);
	std::swap(_stringPatterns, other._stringPatterns);
	std::swap(_charPatterns, other._charPatterns);
	std::swap(_options, other._options);
}

template <char C>
struct CommandLineQuoteReader {
	static void read(mem_pool::StringStream &out, StringView &str) {
		while (!str.empty() && !str.is<C>()) {
			out << str.readUntil<StringView::WhiteSpace, StringView::Chars<C, '\\'>>();
			if (str.is<'\\'>()) {
				++str;
				out << str.at(0);
				++str;
			}
		}
	}
};

size_t CommandLineParserBase::parseStringPattern(void *output,
		const CommandLinePatternData &pattern, StringView str, bool &success) const {
	Vector<StringView> args;

	StringStream out;

	while (!str.empty()) {
		out << str.readUntil<StringView::WhiteSpace, StringView::Chars<'"', '\''>>();
		if (str.is<StringView::WhiteSpace>()) {
			if (!out.empty()) {
				args.emplace_back(StringView(out.weak()).pdup());
				out.clear();
			}
			str.skipChars<StringView::WhiteSpace>();
		} else if (str.is<'"'>()) {
			++str;
			CommandLineQuoteReader<'"'>::read(out, str);
		} else if (str.is<'\''>()) {
			++str;
			CommandLineQuoteReader<'\''>::read(out, str);
		}
	}

	if (!out.empty()) {
		args.emplace_back(StringView(out.weak()).pdup());
	}

	parseStringPattern(output, pattern, args, success);
	return 0;
}

size_t CommandLineParserBase::parseStringPattern(void *output,
		const CommandLinePatternData &pattern, SpanView<StringView> argv, bool &success) const {
	if (pattern.args.empty()) {
		pattern.target->callback(output, pattern.pattern, SpanView<StringView>());
		return 0;
	} else if (argv.size() == 0) {
		log::source().error("CommandLine", "Not enough arguments for --", pattern.pattern);
		return 0;
	}

	auto args = pattern.args;
	args.trimChars<StringView::WhiteSpace>();

	Vector<StringView> result;

	CommandLinePatternParsingData data = {.pattern = &pattern,
		.argv = argv,
		.result = result,

		.offset = 1,
		.args = args,
		.target = argv[data.offset - 1]};

	if (!data.parse()) {
		success = false;
		return data.offset;
	}

	if (invoke) {
		invoke(pattern.target, output, pattern.pattern, result);
	}
	return data.offset;
}

size_t CommandLineParserBase::parseCharPattern(void *output, const CommandLinePatternData &pattern,
		StringView &input, SpanView<StringView> argv, bool &success) const {
	if (pattern.args.empty()) {
		pattern.target->callback(output, pattern.pattern, SpanView<StringView>());
		return 0;
	}

	Vector<StringView> result;

	CommandLinePatternParsingData data = {.pattern = &pattern,
		.argv = argv,
		.result = result,
		.offset = 0,
		.args = StringView(pattern.args),
		.target = input,
		.type = StringView("-")};

	if (!data.parse()) {
		success = false;
		return data.offset;
	}

	if (data.offset > 0) {
		input = StringView();
	} else {
		input = data.target;
	}

	if (invoke) {
		invoke(pattern.target, output, pattern.pattern, result);
	}
	return data.offset;
}

} // namespace stappler::detail
