/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPCore.h"
#include "SPFilepath.h"
#include "SPMakefileVariable.h"
#include "SPStringDetail.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

struct PatternInfo {
	StringView start;
	StringView end;
	bool isPattern = true;
};

static PatternInfo Makefile_getPatternComponents(StringView str) {
	str.trimChars<StringView::WhiteSpace>();

	auto r = str.readUntil<StringView::Chars<'\\', '%'>>();
	if (str.is('%') || str.empty() || (str.is('\\') && str.size() == 1)) {
		// simple case - no escapes
		return PatternInfo{r, str.sub(1), str.is('%')};
	}

	BufferTemplate<Interface> buf;
	if (!r.empty()) {
		buf.put(r.data(), r.size());
	}

	do {
		if (str.is('\\')) {
			if (str.size() > 1) {
				buf.putc(str.at(1));
				str += 2;
			} else {
				buf.putc('\\');
			}
		}
		r = str.readUntil<StringView::Chars<'\\', '%'>>();
		if (!r.empty()) {
			buf.put(r.data(), r.size());
		}
	} while (!str.is('%') && !str.empty());

	if (str.is('%') && str.size() > 1) {
		return PatternInfo{buf.get().pdup(), str.sub(1), true};
	} else {
		return PatternInfo{buf.get().pdup(), StringView(), false};
	}
}

static bool Function_subst(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto from = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	auto to = engine.resolve(args[1], 0, *engine.getCallContext()->err);
	auto text = engine.resolve(args[2], 0, *engine.getCallContext()->err);

	if (from.empty()) {
		engine.getCallContext()->err->reportWarning(toString("'from' component of subst is empty"));
		out << text;
		return true;
	}

	while (!text.empty()) {
		auto v = text.readUntilString(from);
		if (!v.empty()) {
			out << v;
		}
		if (text.is(from)) {
			if (!to.empty()) {
				out << to;
			}
			text += from.size();
		}
	}

	return true;
}

static bool Function_patsubst(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto pattern = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	auto replacement = engine.resolve(args[1], 0, *engine.getCallContext()->err);
	auto text = engine.resolve(args[2], 0, *engine.getCallContext()->err);

	if (pattern.empty() || replacement.empty()) {
		engine.getCallContext()->err->reportWarning(
				toString("'pattern' or 'replacement' components of patsubst is empty"));
		out << text.pdup(engine.getPool());
		return true;
	}

	auto patternPair = Makefile_getPatternComponents(pattern);
	auto replacementPair = Makefile_getPatternComponents(replacement);

	bool first = true;
	text.split<StringView::WhiteSpace>([&](StringView word) {
		if (first) {
			first = false;
		} else {
			out << ' ';
		}

		if (patternPair.isPattern) {
			bool start = patternPair.start.empty() || word.starts_with(patternPair.start);
			bool end = patternPair.end.empty() || word.ends_with(patternPair.end);
			if (start && end && word.size() > patternPair.start.size() + patternPair.end.size()) {
				out << replacementPair.start
					<< word.sub(patternPair.start.size(),
							   word.size() - patternPair.start.size() - patternPair.end.size())
					<< replacementPair.end;
			} else {
				out << word;
			}
		} else if (word == patternPair.start) {
			out << replacementPair.start << replacementPair.end;
		} else {
			out << word;
		}
	});
	return true;
}

static bool Function_strip(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto val = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	val.trimChars<StringView::WhiteSpace>();

	bool first = true;
	val.split<StringView::WhiteSpace>([&](StringView str) {
		if (first) {
			first = false;
		} else {
			out << ' ';
		}
		out << str;
	});

	return true;
}

static bool Function_findstring(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto strToFind = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	auto baseStr = engine.resolve(args[1], 0, *engine.getCallContext()->err);
	if (strToFind.empty()) {
		return false;
	}
	if (baseStr.find(strToFind) != maxOf<size_t>()) {
		out << strToFind;
	}
	return true;
}

static bool Function_filter(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto patterns = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	patterns.trimChars<StringView::WhiteSpace>();

	auto text = engine.resolve(args[1], 0, *engine.getCallContext()->err);
	if (patterns.empty()) {
		if (hasFlag(engine.getFlags(), EngineFlags::Pedantic)) {
			engine.getCallContext()->err->reportWarning(
					toString("'patterns' component of filter is empty"));
		}
		return true;
	}

	Vector<PatternInfo> patternPairs;
	patterns.split<StringView::WhiteSpace>([&](StringView pattern) {
		patternPairs.emplace_back(Makefile_getPatternComponents(pattern));
	});

	bool first = true;
	text.split<StringView::WhiteSpace>([&](StringView word) {
		for (auto &it : patternPairs) {
			if (it.isPattern) {
				bool start = it.start.empty() || word.starts_with(it.start);
				bool end = it.end.empty() || word.ends_with(it.end);
				if (start && end && word.size() > it.start.size() + it.end.size()) {
					if (first) {
						first = false;
					} else {
						out << ' ';
					}
					out << word;
					break;
				}
			} else {
				if (word == it.start) {
					if (first) {
						first = false;
					} else {
						out << ' ';
					}
					out << word;
					break;
				}
			}
		}
	});

	return true;
}

static bool Function_filter_out(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto patterns = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	patterns.trimChars<StringView::WhiteSpace>();

	auto text = engine.resolve(args[1], 0, *engine.getCallContext()->err);
	if (patterns.empty()) {
		if (hasFlag(engine.getFlags(), EngineFlags::Pedantic)) {
			engine.getCallContext()->err->reportWarning(
					toString("'patterns' component of filter is empty"));
		}
		out << text;
		return true;
	}

	Vector<PatternInfo> patternPairs;
	patterns.split<StringView::WhiteSpace>([&](StringView pattern) {
		patternPairs.emplace_back(Makefile_getPatternComponents(pattern));
	});

	bool first = true;
	text.split<StringView::WhiteSpace>([&](StringView word) {
		bool found = false;
		for (auto &it : patternPairs) {
			if (it.isPattern) {
				bool start = it.start.empty() || word.starts_with(it.start);
				bool end = it.end.empty() || word.ends_with(it.end);
				if (start && end && word.size() > it.start.size() + it.end.size()) {
					found = true;
					break;
				}
			} else {
				if (word == it.start) {
					found = true;
					break;
				}
			}
		}
		if (!found) {
			if (first) {
				first = false;
			} else {
				out << ' ';
			}
			out << word;
		}
	});

	return true;
}

// Not optimal, but any other algorithm requires pre-reading of input string
struct SortNode : AllocBase {
	StringView value;
	SortNode *left = nullptr;
	SortNode *right = nullptr;

	SortNode(StringView s) : value(s) { }

	void insert(StringView str) {
		auto v = string::detail::compare_c(value, str);
		if (v < 0) {
			// add to left
			if (!left) {
				left = new (std::nothrow) SortNode(str);
			} else {
				left->insert(str);
			}
		} else if (v > 0) {
			// add to right
			if (!right) {
				right = new (std::nothrow) SortNode(str);
			} else {
				right->insert(str);
			}
		}
	}

	void foreach (const Callback<void(StringView)> &out) {
		if (right) {
			right->foreach (out);
		}
		out << value;
		if (left) {
			left->foreach (out);
		}
	}
};

static bool Function_sort(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	SortNode *node = nullptr;

	auto text = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	if (hasFlag(engine.getFlags(), EngineFlags::Pedantic)) {
		if (text.empty()) {
			engine.getCallContext()->err->reportWarning("'sort' called with empty argument");
		}
	}
	text.split<StringView::WhiteSpace>([&](StringView word) {
		if (!node) {
			node = new (std::nothrow) SortNode(word);
		} else {
			node->insert(word);
		}
	});

	if (node) {
		bool first = true;
		node->foreach ([&](StringView str) {
			if (first) {
				first = false;
			} else {
				out << ' ';
			}
			out << str;
		});
	}
	return true;
}

static bool Function_word(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	engine.getCallContext()->err->reportError("Function not implemented");
	return false; // not implemented
}

static bool Function_wordlist(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto s = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	s.trimChars<StringView::WhiteSpace>();

	auto e = engine.resolve(args[1], 0, *engine.getCallContext()->err);
	e.trimChars<StringView::WhiteSpace>();

	auto text = engine.resolve(args[2], 0, *engine.getCallContext()->err);

	uint32_t sNum = uint32_t(s.readInteger(10).get(0));
	uint32_t eNum = uint32_t(e.readInteger(10).get(0));

	if (sNum == 0 || eNum == 0 || eNum < sNum) {
		return true;
	}

	uint32_t counter = 0;

	bool first = true;
	StringView str(text);
	while (!str.empty()) {
		str.skipChars<StringView::WhiteSpace>();
		auto tmp = str.readUntil<StringView::WhiteSpace>();
		if (!tmp.empty()) {
			++counter;
			if (counter >= sNum) {
				if (first) {
					first = false;
				} else {
					out << ' ';
				}
				out << tmp;
			}
			if (counter > eNum) {
				return true;
			}
		}
	}

	return true;
}

static bool Function_words(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto text = engine.resolve(args[0], 0, *engine.getCallContext()->err);

	uint32_t ret = 0;
	text.split<StringView::WhiteSpace>([&](StringView) { ++ret; });
	out << ret;
	return true;
}

static bool Function_firstword(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto content = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	out << content.readUntil<StringView::WhiteSpace>();
	return true;
}

static bool Function_lastword(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto content = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	out << content.backwardReadUntil<StringView::WhiteSpace>();
	return true;
}

} // namespace stappler::makefile
