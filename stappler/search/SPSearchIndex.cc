/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPSearchIndex.h"
#include "SPString.h"

namespace STAPPLER_VERSIONIZED stappler::search {

bool SearchIndex::init(const TokenizerCallback &tcb) {
	_tokenizer = tcb;
	return true;
}

void SearchIndex::reserve(size_t s) { _nodes.reserve(s); }

void SearchIndex::add(const StringView &v, int64_t id, int64_t tag) {
	String origin(string::tolower<Interface>(v));

	_nodes.emplace_back(Node{id, tag});

	Node &node = _nodes.back();
	uint32_t idx = uint32_t(_nodes.size() - 1);
	auto &canonical = node.canonical;

	auto tokenFn = [&, this](const StringView &str) {
		if (!str.empty()) {
			if (!canonical.empty()) {
				canonical.append(" ");
			}
			auto s = canonical.size();
			canonical.append(str.str<Interface>());
			onToken(_tokens, str, idx, Slice{uint16_t(s), uint16_t(str.size())});
		}
	};

	if (_tokenizer) {
		_tokenizer(origin, tokenFn, SearchNode);
	} else {
		StringView r(origin);
		r.split<DefaultSep>(tokenFn);
	}

	if (canonical.empty()) {
		_nodes.pop_back();
	} else if (canonical != origin) {
		node.alignment = Distance(origin, canonical);
	}
}

SearchIndex::Result SearchIndex::performSearch(const StringView &v, size_t minMatch,
		const HeuristicCallback &cb, const FilterCallback &filter) {
	String origin(string::tolower<Interface>(v));

	SearchIndex::Result res{this};

	uint32_t wordIndex = 0;

	auto tokenFn = [&, this](const StringView &str) {
		auto lb = std::lower_bound(_tokens.begin(), _tokens.end(), str,
				[&, this](const Token &l, const StringView &r) {
			return sprt::detail::compare_c(makeStringView(l.index, l.slice), r) < 0;
		});

		if (lb != _tokens.end()) {
			auto node = &_nodes.at(lb->index);
			StringView value = makeStringView(*lb);

			while (lb != _tokens.end() && value.size() >= str.size()
					&& sprt::__constexpr_strcompare(value.data(), str.data(), str.size()) == 0) {
				if (!filter || filter(node)) {
					auto ret_it = std::lower_bound(res.nodes.begin(), res.nodes.end(), node,
							[&](const ResultNode &l, const Node *r) { return l.node < r; });
					if (ret_it == res.nodes.end() || ret_it->node != node) {
						res.nodes.emplace(ret_it,
								ResultNode{0.0f, node,
									{ResultToken{wordIndex, uint16_t(str.size()), lb->slice}}});
					} else {
						ret_it->matches.emplace_back(
								ResultToken{wordIndex, uint16_t(str.size()), lb->slice});
					}
				}
				++lb;
				if (lb != _tokens.end()) {
					value = makeStringView(*lb);
					node = &_nodes.at(lb->index);
				}
			}
		}
		wordIndex++;
	};

	if (_tokenizer) {
		_tokenizer(origin, tokenFn, SearchRequest);
	} else {
		StringView r(origin);
		r.split<DefaultSep>(tokenFn);
	}

	if (cb) {
		for (auto &it : res.nodes) { it.score = cb(*this, it); }

		std::sort(res.nodes.begin(), res.nodes.end(),
				[](const ResultNode &l, const ResultNode &r) { return l.score > r.score; });
	}

	return res;
}

StringView SearchIndex::resolveToken(const Node &node, const ResultToken &token) const {
	return StringView(node.canonical.data() + token.slice.start, token.match);
}

SearchIndex::Slice SearchIndex::convertToken(const Node &node, const ResultToken &ret) const {
	if (node.alignment.empty()) {
		return Slice{ret.slice.start, ret.match};
	} else {
		auto start = ret.slice.start + node.alignment.diff_original(ret.slice.start);
		auto end = ret.slice.start + ret.match;
		end += node.alignment.diff_original(end, true);
		return Slice{uint16_t(start), uint16_t(end - start)};
	}
}

void SearchIndex::print(const Callback<void(StringView)> &out) const {
	for (auto &it : _tokens) {
		out << it.index << " " << makeStringView(it) << " " << _nodes.at(it.index).id << "\n";
	}
}

StringView SearchIndex::makeStringView(const Token &t) const {
	return makeStringView(t.index, t.slice);
}

StringView SearchIndex::makeStringView(uint32_t idx, const Slice &sl) const {
	const Node &node = _nodes.at(idx);
	return StringView(node.canonical.data() + sl.start, sl.size);
}

void SearchIndex::onToken(Vector<Token> &vec, const StringView &rep, uint32_t idx,
		const Slice &sl) {
	auto insert_it = std::lower_bound(vec.begin(), vec.end(), rep,
			[&, this](const Token &l, const StringView &r) {
		return sprt::detail::compare_c(makeStringView(l.index, l.slice), r) < 0;
	});
	if (insert_it == vec.end()) {
		vec.emplace_back(Token{idx, sl});
	} else {
		vec.emplace(insert_it, Token{idx, sl});
	}
}

float SearchIndex::Heuristic::operator()(const SearchIndex &index,
		const SearchIndex::ResultNode &node) {
	float score = 0.0f;
	uint32_t idx = maxOf<uint32_t>();

	Vector<StringView> matches;

	float mod = tagScore(node.node->tag);

	float fullMatchCost = fullMatchScore / float(node.matches.size());
	for (const SearchIndex::ResultToken &token_it : node.matches) {
		if (excludeEqualMatches) {
			auto t = index.resolveToken(*node.node, token_it);
			if (std::find(matches.begin(), matches.end(), t) == matches.end()) {
				matches.push_back(t);
			} else {
				continue;
			}
		}

		if (token_it.match == token_it.slice.size) {
			score += mod * fullMatchCost;
		}

		score += mod * wordScore(token_it.match, token_it.slice.size);
		score += mod * positionScore(idx, token_it.word);
		idx = token_it.word;
	}
	return score;
}

} // namespace stappler::search
