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

#ifndef CORE_CORE_UTILS_SPCOMMANDLINEPARSER_H_
#define CORE_CORE_UTILS_SPCOMMANDLINEPARSER_H_

#include "SPString.h"
#include "SPSpanView.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler {

/* Declarative-style command line option parser
 *
 * Examples:

CommandLineOption<Value> {
	.patterns = {
		"-v", "--verbose"
	},
	.description = "Produce more verbose output",
	.callback = [] (Value &target, StringView pattern, SpanView<StringView> args) -> bool { return true; }
},
CommandLineOption<Value> {
	.patterns = {
		"-j<#>", "--jobs <#>"
	},
	.description = StringView("Specify number of jobs"),
	.callback = [] (Value &target, StringView pattern, SpanView<StringView> args) -> bool { return true; }
},
CommandLineOption<Value> {
	.patterns = {
		"-r<#>x<#>", "--resolution <#>x<#>"
	},
	.description = StringView("Screen resolution"),
	.callback = [] (Value &target, StringView pattern, SpanView<StringView> args) -> bool {
		auto width = args[0];
		auto height = args[1];
		// ...
		return true;
	}
}

 * Available patterns:
 *
 * -a, -b, -c - simple switches, can be combined like -abc (acts like -a -b -c)
 * -j<#>, -n<Name> - parameterized switches (-j12, -norg.stappler.testapp), can be combined as last switch in line (-abj12)
 * -v <value> - switch with extra parameter (-v TestValue), can not be combined
 * -r<#>x<#> - structured parameters (-r1024x768)
 * --verbose - simple full text option
 * --name <name> - full text option with argument (--name org.stappler.testapp or --name=org.stappler.testapp)
 * --value <val1> <val2> - multiple arguments (--value V1 V2 or --value="V1 V2")
 * --resolution <#>x<#> - structured parameters (--resolution 1024x768)
 *
 * arguments passed as StringView directly to callback in order of appearance in pattern
 *
 * <#> is a special argument type, that match only decimal digits
 * <#.#> is a special argument type, that match only floats
 */

namespace detail {

struct SP_PUBLIC CommandLineParamData final : public mem_pool::AllocBase {
	mem_pool::Vector<StringView> patterns;
	StringView description;
	bool (*callback)(void *, StringView pattern, SpanView<StringView> args);
};

struct SP_PUBLIC CommandLinePatternData final {
	StringView pattern;
	StringView args;
	CommandLineParamData *target;

	bool operator<(const StringView &other) const { return pattern < other; }

	bool operator<(const CommandLinePatternData &other) const { return pattern < other.pattern; }

	bool operator==(const CommandLinePatternData &other) const = default;
	bool operator!=(const CommandLinePatternData &other) const = default;
};

struct SP_PUBLIC CommandLinePatternParsingData {
	const CommandLinePatternData *pattern;
	SpanView<StringView> argv;

	VectorAdapter<StringView> result;
	size_t offset;

	StringView args;
	StringView target;
	StringView type = StringView("--");

	bool parse();
	bool parsePatternString();
	bool parseWhitespace();
};

class SP_PUBLIC CommandLineParserBase : public InterfaceObject<memory::PoolInterface> {
public:
	~CommandLineParserBase();
	CommandLineParserBase();

	bool parse(void *output, int argc, const char *argv[],
			const Callback<void(void *, StringView)> &argCallback) const;

	void describe(const Callback<void(StringView)> &) const;

	void swap(CommandLineParserBase &);

protected:
	size_t parseStringPattern(void *output, const CommandLinePatternData &pattern, StringView str,
			bool &success) const;
	size_t parseStringPattern(void *output, const CommandLinePatternData &pattern,
			SpanView<StringView> argv, bool &success) const;
	size_t parseCharPattern(void *output, const CommandLinePatternData &pattern, StringView &input,
			SpanView<StringView> argv, bool &success) const;

	memory::pool_t *_pool = nullptr;
	memory::allocator_t *_alloc = nullptr;

	Vector<CommandLinePatternData> *_stringPatterns = nullptr;
	Vector<CommandLinePatternData> *_charPatterns = nullptr;
	Vector<CommandLineParamData *> *_options = nullptr;

	bool (*invoke)(const CommandLineParamData *, void *, StringView pattern,
			SpanView<StringView> args) = nullptr;
};

} // namespace detail

template <typename Output>
struct SP_PUBLIC CommandLineOption {
	using CallbackFn = bool (*)(Output &target, StringView pattern, SpanView<StringView> args);

	// List of command line patterns
	InitializerList<const char *> &&patterns;

	// Description for 'help' command
	StringView description;

	// Callback for parsing result
	CallbackFn callback;
};

template <typename Output>
class SP_PUBLIC CommandLineParser final : protected detail::CommandLineParserBase {
public:
	using CallbackFn = typename CommandLineOption<Output>::CallbackFn;

	CommandLineParser(InitializerList<CommandLineOption<Output>> params);

	void add(InitializerList<CommandLineOption<Output>> params);

	bool parse(Output &output, int argc, const char *argv[],
			const Callback<void(Output &, StringView)> &argCallback = nullptr) const;

	using CommandLineParserBase::describe;

	CommandLineParser(const CommandLineParser &other) = delete;
	CommandLineParser &operator=(const CommandLineParser &other) = delete;

	CommandLineParser(CommandLineParser &&other) { swap(other); }
	CommandLineParser &operator=(CommandLineParser &&other) {
		swap(other);
		return *this;
	}
};

template <typename Output>
inline CommandLineParser<Output>::CommandLineParser(
		InitializerList<CommandLineOption<Output>> params)
: CommandLineParserBase() {
	add(params);

	invoke = [](const detail::CommandLineParamData *data, void *ptr, StringView pattern,
					 SpanView<StringView> args) -> bool {
		return CallbackFn(data->callback)(*(Output *)ptr, pattern, args);
	};
}

template <typename Output>
inline void CommandLineParser<Output>::add(InitializerList<CommandLineOption<Output>> params) {
	mem_pool::perform([&] {
		for (auto &it : params) {
			auto data = new (_pool) detail::CommandLineParamData;
			data->description = it.description.pdup();
			data->callback = (decltype(data->callback))it.callback;

			for (auto p : it.patterns) {
				auto pattern = StringView(p).pdup();
				if (StringView(pattern).starts_with("--")) {
					auto args = StringView(pattern).sub(2);
					auto patternInit =
							args.readUntil<StringView::WhiteSpace, StringView::Chars<'<'>>();
					args.backwardSkipChars<StringView::WhiteSpace>();
					if (!mem_pool::emplace_ordered(*_stringPatterns,
								detail::CommandLinePatternData{patternInit, args, data})) {
						log::source().error("CommandLineParser", "Duplicate string pattern: '",
								patternInit, "'");
					}
				} else if (StringView(pattern).starts_with("-")) {
					auto args = StringView(pattern).sub(1);
					auto patternInit =
							args.readUntil<StringView::WhiteSpace, StringView::Chars<'<'>>();
					args.backwardSkipChars<StringView::WhiteSpace>();

					auto prev = std::lower_bound(_charPatterns->begin(), _charPatterns->end(),
							patternInit.sub(0, 1));
					if (prev != _charPatterns->end()
							&& prev->pattern.starts_with(patternInit.sub(0, 1))) {
						log::source().error("CommandLineParser", "Duplicate char pattern: '",
								patternInit, "'; previosely defined as '", prev->pattern, "'");
					} else {
						mem_pool::emplace_ordered(*_charPatterns,
								detail::CommandLinePatternData{patternInit, args, data});
					}
				}
				data->patterns.emplace_back(pattern);
			}

			_options->emplace_back(data);
		}
	}, _pool);
}

template <typename Output>
inline bool CommandLineParser<Output>::parse(Output &output, int argc, const char *argv[],
		const Callback<void(Output &, StringView)> &argCallback) const {
	void *outputPtr = (void *)&output;

	if (argCallback) {
		return CommandLineParserBase::parse(outputPtr, argc, argv,
				[&](void *ptr, StringView str) { argCallback(*(Output *)ptr, str); });
	} else {
		return CommandLineParserBase::parse(outputPtr, argc, argv, nullptr);
	}
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* CORE_CORE_UTILS_SPCOMMANDLINEPARSER_H_ */
