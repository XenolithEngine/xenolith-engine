/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_FLOW_CODEGEN_SYNTH_CASES_H_
#define TESTS_FLOW_CODEGEN_SYNTH_CASES_H_

#include "codegen_fixture.h"

#include "synth_graph.h"

// The generated half of the differential corpus.
//
// The hand-written corpus is a list of shapes somebody thought of. This is a list of seeds, and the
// shapes are whatever the generator makes of them - an assertion in the generator about the order
// of nodes is wrong first on a shape nobody drew.
//
// A seed is a `corpus::Case` like any other, so every consumer of the corpus - the fixture, the
// oracle, the emitter - takes one without knowing where it came from.
namespace STAPPLER_VERSIONIZED stappler::test::codegenfx {

namespace synth = stappler::test::synth;

// The seeds. Each is a configuration rather than a bare number so that the set can be steered - a
// seed that only ever produced a chain would be build time buying nothing - and `codegen-synth`
// checks the steering: every construct the generator can make has to turn up somewhere in the set.
inline constexpr synth::RunConfig SynthSeeds[] = {
	{.seed = 0x1001u, .budget = 10, .maxDepth = 2},
	{.seed = 0x1002u, .budget = 12, .maxDepth = 2},
	{.seed = 0x1003u, .budget = 12, .maxDepth = 1},
	{.seed = 0x1004u, .budget = 14, .maxDepth = 2},
	{.seed = 0x1005u, .budget = 9, .maxDepth = 2, .allowRepeat = false},
	{.seed = 0x1006u, .budget = 14, .maxDepth = 2, .allowRepeat = false},
	{.seed = 0x1007u, .budget = 11, .maxDepth = 2},
	{.seed = 0x1008u, .budget = 13, .maxDepth = 2, .allowRepeat = false},
	{.seed = 0x1009u, .budget = 10, .maxDepth = 1, .allowStrings = false},
	{.seed = 0x100au, .budget = 15, .maxDepth = 2},
	{.seed = 0x100bu, .budget = 12, .maxDepth = 2, .allowRepeat = false},
	{.seed = 0x100cu, .budget = 16, .maxDepth = 2, .allowRepeat = false},
};

inline constexpr uint32_t SynthSeedCount = sizeof(SynthSeeds) / sizeof(SynthSeeds[0]);

// One seed, materialised: the graph as the text a case carries, and what the generator says it
// built. The strings are held here because `corpus::Case` addresses them by view.
struct SynthCase {
	mem_std::String name;
	mem_std::String json;
	synth::Shape shape;

	corpus::Case asCase() const {
		corpus::Case c;
		c.name = StringView(name);
		c.json = StringView(json);
		return c;
	}
};

// Built once, in seed order, and stable for the life of the process - the views a `corpus::Case`
// hands out point into it.
inline const mem_std::Vector<SynthCase> &synthCases() {
	static mem_std::Vector<SynthCase> cases = [] {
		mem_std::Vector<SynthCase> out;
		out.reserve(SynthSeedCount);
		for (auto &cfg : SynthSeeds) {
			SynthCase c;
			c.name = mem_std::toString("synth-", cfg.seed);
			auto value = synth::makeRunGraph(cfg, &c.shape);
			c.json = data::toString<mem_std::Interface>(value, false);
			out.emplace_back(sprt::move(c));
		}
		return out;
	}();
	return cases;
}

// `synth-4097` -> `synth_4097`, the same rule `unitName` uses for the hand-written half.
inline mem_std::String synthUnitName(StringView caseName) {
	mem_std::String out;
	for (auto ch : caseName) { out.push_back(ch == '-' ? '_' : ch); }
	return out;
}

} // namespace stappler::test::codegenfx

#endif /* TESTS_FLOW_CODEGEN_SYNTH_CASES_H_ */
