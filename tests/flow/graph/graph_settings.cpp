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

// Node settings: a property of a node that is not a pin. Declared by the operation, written
// in the node's `settings` section, resolved once by the build, carried by the editor.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "graph_fixture.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;
using stappler::test::hasDiag;

namespace {

using namespace flow;
using stappler::test::graphfx::diagLines;
namespace vstore = flow::value;
using flow::value::VarType;

Status noop(OpContext &) { return Status::Ok; }

constexpr StringView Modes[] = {StringView("fast"), StringView("exact")};

// A probe with one setting of each role.
OpDef probeDef(SpanView<SettingDesc> settings, StringView name = StringView("probe.configured")) {
	OpDef def;
	def.name = name;
	def.settings = settings;
	def.invoke = &noop;
	return def;
}

mem_std::Vector<SettingDesc> probeSettings() {
	mem_std::Vector<SettingDesc> out;
	SettingDesc mode;
	mode.name = StringView("mode");
	mode.type = VarType::String;
	mode.role = SettingRole::Choice;
	mode.choices = SpanView<StringView>(Modes, 2);
	out.emplace_back(sprt::move(mode));

	SettingDesc limit;
	limit.name = StringView("limit");
	limit.type = VarType::Int32;
	limit.def = mem_std::Value(8);
	out.emplace_back(sprt::move(limit));

	SettingDesc tags;
	tags.name = StringView("tags");
	tags.type = VarType::Array;
	tags.element = flow::value::makeChain(VarType::String);
	tags.role = SettingRole::ComponentNames;
	out.emplace_back(sprt::move(tags));
	return out;
}

bool refused(const OpDef &def) {
	OpRegistry reg;
	reg.init();
	mem_std::Value diag;
	return reg.createNative(def, &diag) == nullptr && hasDiag(diag, "op-setting-invalid");
}

mem_std::String node(StringView settings) {
	return mem_std::toString(R"({"formatVersion": 1, "nodes": [
		{"id": 1, "op": "probe.configured")", settings.empty() ? StringView() : StringView(", "),
			settings, R"(}
	], "edges": []})");
}

} // namespace

void performGraphSettingsTests() {
	sprt::cout << "\n== flow graph: node settings ==\n";

	auto settings = probeSettings();
	auto span = SpanView<SettingDesc>(settings.data(), settings.size());

	// ---- the declaration ------------------------------------------------------------------------------

	{
		OpRegistry reg;
		reg.init();
		auto desc = reg.createNative(probeDef(span));
		check(desc && desc->getSettings().size() == 3, "graph-settings: an operation declares settings");
		uint32_t index = 0;
		check(desc && desc->findSetting(StringView("limit"), index) && index == 1,
				"graph-settings: and finds one by name");

		mem_std::Value described;
		if (desc) {
			desc->describe(described);
		}
		auto &list = described.getValue("settings");
		check(list.size() == 3 && list.getValue(0).getString("role") == StringView("choice")
						&& list.getValue(1).getInteger("default") == 8
						&& list.getValue(2).getString("element") == StringView("string"),
				"graph-settings: describe names them, with role, element and default");

		OpRegistry bare;
		bare.init();
		auto plain = bare.createNative(probeDef(SpanView<SettingDesc>(), StringView("probe.plain")));
		mem_std::Value plainDescribed;
		plain->describe(plainDescribed);
		check(plainDescribed.getValue("settings").isNull(),
				"graph-settings: and an operation with none says nothing");
	}

	{
		auto hashOf = [](SpanView<SettingDesc> s) {
			OpRegistry reg;
			reg.init();
			auto desc = reg.createNative(probeDef(s));
			return desc ? desc->getSignatureHash() : 0;
		};
		auto base = hashOf(span);

		OpRegistry bare;
		bare.init();
		auto none = bare.createNative(probeDef(SpanView<SettingDesc>()));
		OpDef empty;
		empty.name = StringView("probe.configured");
		empty.invoke = &noop;
		OpRegistry other;
		other.init();
		check(none && other.createNative(empty)->getSignatureHash() == none->getSignatureHash()
						&& base != none->getSignatureHash(),
				"graph-settings: declaring settings moves the hash, declaring none does not");

		auto retuned = probeSettings();
		retuned[1].def = mem_std::Value(16);
		check(hashOf(SpanView<SettingDesc>(retuned.data(), retuned.size())) == base,
				"graph-settings: a default is not in the hash");

		auto renamed = probeSettings();
		renamed[0].name = StringView("style");
		auto rechosen = probeSettings();
		static constexpr StringView fewer[] = {StringView("fast")};
		rechosen[0].choices = SpanView<StringView>(fewer, 1);
		check(hashOf(SpanView<SettingDesc>(renamed.data(), renamed.size())) != base
						&& hashOf(SpanView<SettingDesc>(rechosen.data(), rechosen.size())) != base,
				"graph-settings: a name or a choice is");
	}

	{
		auto one = [](SettingDesc s) {
			static mem_std::Vector<SettingDesc> held;
			held.clear();
			held.emplace_back(sprt::move(s));
			return probeDef(SpanView<SettingDesc>(held.data(), held.size()));
		};

		SettingDesc noChoices;
		noChoices.name = StringView("mode");
		noChoices.type = VarType::String;
		noChoices.role = SettingRole::Choice;
		check(refused(one(noChoices)), "graph-settings: a choice with no choices is refused");

		SettingDesc intChoice;
		intChoice.name = StringView("mode");
		intChoice.type = VarType::Int;
		intChoice.role = SettingRole::Choice;
		intChoice.choices = SpanView<StringView>(Modes, 2);
		check(refused(one(intChoice)), "graph-settings: a choice that is not a String is refused");

		SettingDesc badDefault;
		badDefault.name = StringView("mode");
		badDefault.type = VarType::String;
		badDefault.role = SettingRole::Choice;
		badDefault.choices = SpanView<StringView>(Modes, 2);
		badDefault.def = mem_std::Value("slow");
		check(refused(one(badDefault)), "graph-settings: a default outside the choices is refused");

		SettingDesc stray;
		stray.name = StringView("limit");
		stray.type = VarType::Int32;
		stray.choices = SpanView<StringView>(Modes, 2);
		check(refused(one(stray)), "graph-settings: choices on a plain setting are refused");

		SettingDesc names;
		names.name = StringView("tags");
		names.type = VarType::String;
		names.role = SettingRole::ComponentNames;
		check(refused(one(names)), "graph-settings: component names that are not a list are refused");

		auto twice = probeSettings();
		twice[2].name = StringView("mode");
		check(refused(probeDef(SpanView<SettingDesc>(twice.data(), twice.size()))),
				"graph-settings: two settings of one name are refused");
	}

	// ---- the asset --------------------------------------------------------------------------------------

	{
		GraphAsset asset;
		asset.init();
		mem_std::Value report;
		auto json = node(R"("settings": {"mode": "exact", "tags": ["Unit"]})");
		check(asset.load(data::read<mem_std::Interface>(json), &report) == Status::Ok,
				"graph-settings: a node with settings loads");
		mem_std::Value saved;
		asset.save(saved);
		auto &entry = saved.getValue("nodes").getValue(0);
		check(entry.getValue("settings").getString("mode") == StringView("exact")
						&& entry.getValue("settings").getValue("tags").size() == 1,
				"graph-settings: and saves them back");

		GraphAsset bad;
		bad.init();
		mem_std::Value badReport;
		bad.load(data::read<mem_std::Interface>(node(R"("settings": [1])")), &badReport);
		check(hasDiag(badReport, "asset-malformed"), "graph-settings: settings that are not a dictionary");
	}

	// ---- the build --------------------------------------------------------------------------------------

	OpRegistry ops;
	ops.init();
	ops.createNative(probeDef(span));
	flow::ops::registerCoreOps(ops);

	auto build = [&](StringView json, mem_std::Value &report, RuntimeGraph &graph,
						 const flow::value::TypeRegistry *scene = nullptr) {
		GraphAsset asset;
		asset.init();
		graph.init();
		report = mem_std::Value();
		if (asset.load(data::read<mem_std::Interface>(json), &report) != Status::Ok) {
			return Status::ErrorInvalidArguemnt;
		}
		return scene ? graph.build(asset, ops, *scene, &report) : graph.build(asset, ops, &report);
	};

	{
		mem_std::Value report;
		RuntimeGraph graph;
		check(build(node(StringView()), report, graph) == Status::Ok,
				"graph-settings: a node naming no settings builds");
		auto limit = graph.getSetting(0, 1);
		auto mode = graph.getSetting(0, 0);
		auto tags = graph.getSetting(0, 2);
		check(mode && mode->getString() == StringView("fast") && limit && limit->getInteger() == 8
						&& tags && tags->isArray() && tags->size() == 0,
				"graph-settings: and takes the first choice, the default and an empty list");

		mem_std::Value described;
		describeGraph(graph, described);
		check(described.getValue("nodes").getValue(0).getValue("settings").getInteger("limit") == 8,
				"graph-settings: the dump shows what the node resolved to");
	}
	{
		mem_std::Value report;
		RuntimeGraph graph;
		build(node(R"("settings": {"mode": "slow", "limit": "many", "tags": ["A", "A"], "colour": 1})"),
				report, graph);
		checkEq(StringView(diagLines(report)),
				StringView("setting-invalid@1#mode setting-invalid@1#limit setting-invalid@1#tags "
						   "setting-unknown@1#colour"),
				"graph-settings: a choice outside, a wrong type, a name twice, a setting that does not exist");
	}
	{
		mem_std::Value report;
		RuntimeGraph graph;
		auto json = mem_std::toString(R"({"formatVersion": 1, "scene": [{"component": "Unit"}], "nodes": [
			{"id": 1, "op": "probe.configured", "settings": {"tags": ["Unit", "Ghost"]}}
		], "edges": []})");
		build(json, report, graph);
		check(StringView(diagLines(report)).starts_with("scene-undeclared@1#tags"),
				mem_std::toString("graph-settings: a component name the scene section does not declare (",
						diagLines(report), ")"));

		flow::value::TypeRegistry scene;
		scene.init();
		scene.createNative(StringView("Unit"), SpanView<flow::value::FieldDef>());
		RuntimeGraph bound;
		build(node(R"("settings": {"tags": ["Unit", "Ghost"]})"), report, bound, &scene);
		check(StringView(diagLines(report)).starts_with("scene-unknown-component@1#tags"),
				mem_std::toString("graph-settings: and one the scene's registry does not have (",
						diagLines(report), ")"));
	}

}

} // namespace STAPPLER_VERSIONIZED stappler