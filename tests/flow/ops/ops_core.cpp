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

// The standard library of nodes (module stappler_flow_ops).
//
// The library is a separate module so that layer L2 cannot come to depend on any particular
// operation. What this section checks is therefore mostly about the SHAPE of the library rather than
// about arithmetic: that registration is idempotent, that every signature is legal, and that the
// signature hashes - the numbers an asset stores to notice that an operation changed under it - are
// the same on every ABI.
//
// One entry deserves a name: convert.intToFloat. Int -> Float is a Narrow cell of the conversion
// matrix, and layer L2 lets only Same and Widen cross an edge implicitly, so without this node an
// integer cannot reach a float input at all. It is the explicit step the rule demands.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowOps.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../interp/interp_fixture.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::flow;
namespace ops = stappler::flow::ops;

// A pure node whose inputs are all parameters has no incoming edges and no exec input, which is
// exactly the definition of an entry node - so a one-node graph runs it and leaves the answer in its
// record. That is the cheapest possible way to ask an operation what it computes, and the reason it
// is worth having: `logic.and` and `logic.or` were registered with a Bool output pin and an `int`
// implementation, so every call returned ErrorInvalidArguemnt from ComponentType::setField. The
// signature was right, the hash was right, and nothing here ever invoked them.
bool evaluate(StringView op, mem_std::Value &&params, flow::value::Var &out) {
	stappler::test::interpfx::Fixture fx;
	mem_std::Value node(mem_std::Value::Type::DICTIONARY);
	node.setInteger(1, "id");
	node.setString(op, "op");
	node.setValue(sprt::move(params), "params");

	mem_std::Value graph(mem_std::Value::Type::DICTIONARY);
	graph.setInteger(1, "formatVersion");
	graph.newArray("nodes").addValue(sprt::move(node));
	graph.newArray("edges");

	if (!fx.prepare(data::toString<mem_std::Interface>(graph))) {
		return false;
	}
	if (fx.run() != Status::Ok || fx.report.outcome != RunOutcome::Completed) {
		return false;
	}
	return fx.output(1, 0, out);
}

bool evaluateBool(StringView op, mem_std::Value &&params, bool &out) {
	flow::value::Var v;
	if (!evaluate(op, sprt::move(params), v) || v.type != flow::value::VarType::Bool) {
		return false;
	}
	out = v.i != 0;
	return true;
}

bool evaluateInt(StringView op, mem_std::Value &&params, int64_t &out) {
	flow::value::Var v;
	if (!evaluate(op, sprt::move(params), v) || v.type != flow::value::VarType::Int) {
		return false;
	}
	out = v.i;
	return true;
}

mem_std::Value boolPair(bool lhs, bool rhs) {
	mem_std::Value params(mem_std::Value::Type::DICTIONARY);
	params.setBool(lhs, "lhs");
	params.setBool(rhs, "rhs");
	return params;
}

mem_std::Value intPair(int64_t lhs, int64_t rhs) {
	mem_std::Value params(mem_std::Value::Type::DICTIONARY);
	params.setInteger(lhs, "lhs");
	params.setInteger(rhs, "rhs");
	return params;
}

// Name and signature hash of every registered operation, in registration order. Compact enough to
// keep as a literal, and it covers the one number that has to be identical everywhere.
void dumpSignatures(const OpRegistry &reg, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::ARRAY);
	for (uint32_t i = 0; i < reg.getCount(); ++i) {
		auto op = reg.getAt(i);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setString(op->getName(), "name");
		entry.setInteger(int64_t(op->getSignatureHash()), "hash");
		out.addValue(sprt::move(entry));
	}
}

} // namespace

void performOpsCoreTests() {
	sprt::cout << "\n== flow ops: the node library ==\n";

	OpRegistry reg;
	check(reg.init(), "ops-core: the registry initialises");
	check(ops::registerCoreOps(reg) == Status::Ok, "ops-core: the library registers");

	auto count = reg.getCount();
	check(count == 161, "ops-core: the library holds a hundred and sixty-one operations");

	check(ops::registerCoreOps(reg) == Status::Ok && reg.getCount() == count,
			"ops-core: registering it twice changes nothing");

	// ---- the shape of each signature ---------------------------------------------------------------

	{
		bool schemas = true;
		for (uint32_t i = 0; i < reg.getCount(); ++i) {
			auto op = reg.getAt(i);
			// E5's rule: a schema exists exactly when there is something to remember.
			bool needsRecord = op->getDataOut().size() > 0;
			if (needsRecord != (op->getLocalSchema() != nullptr)) {
				schemas = false;
				sprt::cout << "       " << op->getName() << ": schema mismatch\n";
			}
		}
		check(schemas, "ops-core: an operation has a local schema exactly when it has outputs");
	}

	{
		auto convert = reg.get(StringView("convert.intToFloat"));
		check(convert != nullptr && convert->getDataIn().size() == 1
						&& convert->getDataIn()[0].type == flow::value::VarType::Int
						&& convert->getDataOut().size() == 1
						&& convert->getDataOut()[0].type == flow::value::VarType::Float,
				"ops-core: convert.intToFloat is the Int -> Float node the edge rule demands");

		auto branch = reg.get(StringView("flow.branch"));
		check(branch != nullptr && branch->hasExecIn() && branch->getExecOut().size() == 2
						&& branch->getExecOut()[0] == "true" && branch->getExecOut()[1] == "false",
				"ops-core: flow.branch has an exec input and two named exec outputs");

		auto event = reg.get(StringView("flow.event"));
		check(event != nullptr && !event->hasExecIn() && event->getExecOut().size() == 1,
				"ops-core: flow.event has no exec input, so a run can start there");

		auto concat = reg.get(StringView("string.concat"));
		check(concat != nullptr && concat->getDataOut().size() == 1
						&& concat->getDataOut()[0].type == flow::value::VarType::String,
				"ops-core: string.concat is the library's one container-valued operation");

		bool invokers = true;
		for (uint32_t i = 0; i < reg.getCount(); ++i) {
			if (reg.getAt(i)->getInvoke() == nullptr) {
				invokers = false;
			}
		}
		check(invokers, "ops-core: every operation carries an implementation");
	}

	// ---- what they compute --------------------------------------------------------------------------
	//
	// Not arithmetic for its own sake: these are the operations a graph decides with, and a decision
	// node that fails is indistinguishable from a branch that was not taken.

	{
		bool andTable = true, orTable = true;
		for (int i = 0; i < 4; ++i) {
			bool lhs = (i & 1) != 0, rhs = (i & 2) != 0, result = false;
			if (!evaluateBool(StringView("logic.and"), boolPair(lhs, rhs), result)
					|| result != (lhs && rhs)) {
				andTable = false;
			}
			if (!evaluateBool(StringView("logic.or"), boolPair(lhs, rhs), result)
					|| result != (lhs || rhs)) {
				orTable = false;
			}
		}
		check(andTable, "ops-core: logic.and computes conjunction over the whole truth table");
		check(orTable, "ops-core: logic.or computes disjunction over the whole truth table");

		bool notTrue = true, notFalse = false;
		mem_std::Value t(mem_std::Value::Type::DICTIONARY);
		t.setBool(true, "value");
		mem_std::Value f(mem_std::Value::Type::DICTIONARY);
		f.setBool(false, "value");
		check(evaluateBool(StringView("logic.not"), sprt::move(t), notTrue) && !notTrue
						&& evaluateBool(StringView("logic.not"), sprt::move(f), notFalse)
						&& notFalse,
				"ops-core: logic.not negates");
	}

	{
		// The boundaries and the sign, because a bounds check is what this node is for and because
		// widening to Float to get an ordering - the only way it could be spelled before - is exact
		// only below 2^53.
		bool ok = true;
		auto less = [&](int64_t a, int64_t b, bool expect) {
			bool result = false;
			if (!evaluateBool(StringView("compare.lessInt"), intPair(a, b), result)
					|| result != expect) {
				ok = false;
			}
		};
		less(0, 8, true);
		less(8, 8, false);
		less(9, 8, false);
		less(-1, 0, true);
		less(0, 0, false);
		less(-3, -4, false);
		less(9007199254740993ll, 9007199254740994ll, true); // two doubles cannot tell these apart
		check(ok, "ops-core: compare.lessInt orders integers, including past the exact range of a double");
	}

	{
		// Overflow is DEFINED: the sum and the product wrap modulo 2^64 (SPFlowOpsInline.h). A signed `a + b`
		// here was undefined behaviour, and a Fibonacci counter reaches it on its 93rd step.
		const int64_t max = maxOf<int64_t>();
		const int64_t min = minOf<int64_t>();
		int64_t value = 0;
		check(evaluateInt(StringView("math.addInt"), intPair(2, 3), value) && value == 5,
				"ops-core: math.addInt adds");
		check(evaluateInt(StringView("math.addInt"), intPair(max, 1), value) && value == min,
				"ops-core: ... and wraps past the largest integer to the smallest");
		check(evaluateInt(StringView("math.addInt"), intPair(min, -1), value) && value == max,
				"ops-core: ... and back the other way");
		check(evaluateInt(StringView("math.mulInt"), intPair(-4, 5), value) && value == -20,
				"ops-core: math.mulInt multiplies");
		check(evaluateInt(StringView("math.mulInt"), intPair(max, 2), value) && value == -2,
				"ops-core: ... and wraps modulo 2^64");
	}

	{
		auto items = [](std::initializer_list<int64_t> values, int64_t index) {
			mem_std::Value params(mem_std::Value::Type::DICTIONARY);
			auto &arr = params.newArray("items");
			for (auto &it : values) { arr.addInteger(it); }
			params.setInteger(index, "index");
			return params;
		};

		int64_t value = 0;
		check(evaluateInt(StringView("array.getInt"), items({10, 20, 30}, 0), value) && value == 10,
				"ops-core: array.getInt reads the first element");
		check(evaluateInt(StringView("array.getInt"), items({10, 20, 30}, 2), value) && value == 30,
				"ops-core: array.getInt reads the last element");
		check(!evaluateInt(StringView("array.getInt"), items({10, 20, 30}, 3), value),
				"ops-core: an index past the end fails rather than reading as zero");
		check(!evaluateInt(StringView("array.getInt"), items({10, 20, 30}, -1), value),
				"ops-core: and so does a negative one");
		check(!evaluateInt(StringView("array.getInt"), items({}, 0), value),
				"ops-core: an empty array has no element 0");
	}

	// ---- the numbers an asset stores ----------------------------------------------------------------

	{
		mem_std::Value dump;
		dumpSignatures(reg, dump);

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json([
			{"name": "flow.event", "hash": 2354902524142041077},
			{"name": "flow.sequence", "hash": -8178773161245269798},
			{"name": "flow.branch", "hash": 527311327428707722},
			{"name": "flow.forEach", "hash": 6594504761647998447},
			{"name": "math.addFloat", "hash": 2050205921249594405},
			{"name": "math.subFloat", "hash": 7129637929023444516},
			{"name": "math.mulFloat", "hash": 2235912393355980553},
			{"name": "math.addInt", "hash": -5332912646374363090},
			{"name": "math.mulInt", "hash": 800151708320226191},
			{"name": "logic.and", "hash": 4633991251256711699},
			{"name": "logic.or", "hash": 3673569553415840685},
			{"name": "logic.not", "hash": 4434156393025194118},
			{"name": "compare.lessFloat", "hash": 6486824197780031361},
			{"name": "compare.equalInt", "hash": 8702945649562220455},
			{"name": "compare.lessInt", "hash": 5753388644846733433},
			{"name": "convert.intToFloat", "hash": 6589780731066317815},
			{"name": "array.getInt", "hash": 4623319249182855143},
			{"name": "value.float", "hash": 7243517016834713108},
			{"name": "value.int", "hash": 232118240246703257},
			{"name": "value.bool", "hash": -8874648700335108429},
			{"name": "string.concat", "hash": -4168289060493493766},
			{"name": "value.string", "hash": -2017432891516817653},
			{"name": "debug.trace", "hash": -686064980508589355},
			{"name": "wait.timer", "hash": 9206444280194067418},
			{"name": "await.component", "hash": -4549174816699431167},
			{"name": "scene.global", "hash": 7501932075593457857},
			{"name": "scene.named", "hash": -7555814545785335118},
			{"name": "scene.has", "hash": -5628949854329338885},
			{"name": "scene.getInt", "hash": 5565224454762246883},
			{"name": "scene.setInt", "hash": 3164643834852790283},
			{"name": "scene.findByField", "hash": 1677393073467358146},
			{"name": "scene.addComponent", "hash": -6528631373055687327},
			{"name": "scene.removeComponent", "hash": 1531880060391735120},
			{"name": "scene.getInt32", "hash": 3288289629474863173},
			{"name": "scene.setInt32", "hash": 7361531914584812122},
			{"name": "scene.getUInt32", "hash": 3697398896859345503},
			{"name": "scene.setUInt32", "hash": 274042168937382235},
			{"name": "scene.getFloat32", "hash": 4338823022592177236},
			{"name": "scene.setFloat32", "hash": 3912798998439495011},
			{"name": "scene.getEnum", "hash": -179869230726396735},
			{"name": "scene.setEnum", "hash": 7241354244625657065},
			{"name": "math.subInt", "hash": 7338456286733965140},
			{"name": "math.divInt", "hash": 6335450553153115918},
			{"name": "math.minInt", "hash": -1400497286255818430},
			{"name": "math.maxInt", "hash": -9166530937252272980},
			{"name": "math.absInt", "hash": 6555623447952878356},
			{"name": "math.divFloat", "hash": -3373497050892011589},
			{"name": "math.minFloat", "hash": 2418871729017247633},
			{"name": "math.maxFloat", "hash": -2333277883880753440},
			{"name": "math.absFloat", "hash": -7046574342429435821},
			{"name": "compare.equalFloat", "hash": 5497795214758246646},
			{"name": "math.addInt32", "hash": -2182088826509722477},
			{"name": "math.subInt32", "hash": 6186538105845589909},
			{"name": "math.mulInt32", "hash": -4627380462961162405},
			{"name": "math.divInt32", "hash": -5599598620494640018},
			{"name": "math.minInt32", "hash": 2416457684837214292},
			{"name": "math.maxInt32", "hash": -8562542922877399821},
			{"name": "math.absInt32", "hash": -129132261139306801},
			{"name": "compare.lessInt32", "hash": -3320310166433034558},
			{"name": "compare.equalInt32", "hash": 6474370010235799505},
			{"name": "math.addUInt32", "hash": 2910252399552281623},
			{"name": "math.subUInt32", "hash": 2249748698597390123},
			{"name": "math.mulUInt32", "hash": -7127381238800736862},
			{"name": "math.divUInt32", "hash": 4600256526837783548},
			{"name": "math.minUInt32", "hash": 6956132035150477076},
			{"name": "math.maxUInt32", "hash": 8913168990587632505},
			{"name": "compare.lessUInt32", "hash": 3508617332456383465},
			{"name": "compare.equalUInt32", "hash": -1992381855228120393},
			{"name": "math.addFloat32", "hash": 9163694584309466019},
			{"name": "math.subFloat32", "hash": 4576283635256411683},
			{"name": "math.mulFloat32", "hash": 3439006209001601129},
			{"name": "math.divFloat32", "hash": 7466162269355405556},
			{"name": "math.minFloat32", "hash": -2583877874292211697},
			{"name": "math.maxFloat32", "hash": 2243762359700171886},
			{"name": "math.absFloat32", "hash": 1052618489232183956},
			{"name": "compare.lessFloat32", "hash": 2875743674321467714},
			{"name": "compare.equalFloat32", "hash": 7421702435500086781},
			{"name": "math.floorFloat", "hash": 3853002225040157411},
			{"name": "math.ceilFloat", "hash": -4235252006606519425},
			{"name": "math.sqrtFloat", "hash": 6637329321689420495},
			{"name": "math.sinFloat", "hash": 2778437386754660954},
			{"name": "math.cosFloat", "hash": -4829007009602924880},
			{"name": "math.tanFloat", "hash": 4084741318921309365},
			{"name": "math.powFloat", "hash": 3320133631795833597},
			{"name": "math.floorFloat32", "hash": -4902267009232519859},
			{"name": "math.ceilFloat32", "hash": -1444787308651107320},
			{"name": "math.sqrtFloat32", "hash": -6005685806693830743},
			{"name": "math.sinFloat32", "hash": -778320283660558604},
			{"name": "math.cosFloat32", "hash": 2181249622303261914},
			{"name": "math.tanFloat32", "hash": -2164474505912863026},
			{"name": "math.powFloat32", "hash": 9128436355706630747},
			{"name": "math.addVec2", "hash": 5629695629083561909},
			{"name": "math.subVec2", "hash": 4504035094244485713},
			{"name": "math.scaleVec2", "hash": 8091332269486432265},
			{"name": "math.dotVec2", "hash": -6052667275335630046},
			{"name": "math.lengthVec2", "hash": 4256397397209293537},
			{"name": "math.normalizeVec2", "hash": 1648653714279236377},
			{"name": "math.addVec3", "hash": 7933390975484445992},
			{"name": "math.subVec3", "hash": 984626691077368228},
			{"name": "math.scaleVec3", "hash": -136418237379359213},
			{"name": "math.dotVec3", "hash": -4676682204567670605},
			{"name": "math.lengthVec3", "hash": 3968346526843362902},
			{"name": "math.normalizeVec3", "hash": 2344120150510592535},
			{"name": "math.addVec4", "hash": -3510759129926255955},
			{"name": "math.subVec4", "hash": -2294690891463627691},
			{"name": "math.scaleVec4", "hash": 1992571646062983205},
			{"name": "math.dotVec4", "hash": 4978652206243890197},
			{"name": "math.lengthVec4", "hash": -2635122798129393802},
			{"name": "math.normalizeVec4", "hash": -9128025730771553141},
			{"name": "convert.floatToInt", "hash": -4149288511899528600},
			{"name": "convert.intToInt32", "hash": -152817291510657610},
			{"name": "convert.intToUInt32", "hash": 63336682795386433},
			{"name": "convert.int32ToUInt32", "hash": -1634097502195293830},
			{"name": "convert.uInt32ToInt32", "hash": 4897110258078319813},
			{"name": "convert.floatToFloat32", "hash": 4461646614341661268},
			{"name": "convert.float32ToInt32", "hash": 8919850549545831747},
			{"name": "convert.int32ToFloat32", "hash": -4530688716761055765},
			{"name": "enum.toInt32", "hash": -5547287690857347578},
			{"name": "enum.fromInt32", "hash": -1282073154873640788},
			{"name": "value.int32", "hash": -5764946368556957359},
			{"name": "value.uint32", "hash": -6516505435537786514},
			{"name": "value.float32", "hash": 7949407885358838404},
			{"name": "value.vec2", "hash": -5727206389190808728},
			{"name": "value.vec3", "hash": 5785524011923841597},
			{"name": "value.vec4", "hash": 1600737086189851082},
			{"name": "value.color", "hash": 3435611909622127995},
			{"name": "par.forEach", "hash": -1010492679977520635},
			{"name": "par.forEachWith", "hash": -6827504028259287422},
			{"name": "par.barrier", "hash": 467544257882145595},
			{"name": "par.gatherBool", "hash": -8866858985485920473},
			{"name": "par.gatherInt", "hash": 7901472087488333741},
			{"name": "par.gatherFloat", "hash": 6688829902164215591},
			{"name": "par.gatherInt32", "hash": -5498985923333441286},
			{"name": "par.gatherUInt32", "hash": -9102003102992831671},
			{"name": "par.gatherFloat32", "hash": -7307170428087319798},
			{"name": "par.gatherVec2", "hash": 7367798039000863161},
			{"name": "par.gatherVec3", "hash": 4354828779854028192},
			{"name": "par.gatherVec4", "hash": 2212435867084713106},
			{"name": "par.gatherColor", "hash": -7185253965897824530},
			{"name": "par.gatherEntityRef", "hash": -2551898080996892519},
			{"name": "par.sumInt", "hash": 2888198490572680545},
			{"name": "par.sumFloat", "hash": -5540146751196795269},
			{"name": "par.sumInt32", "hash": -995802498233808871},
			{"name": "par.sumUInt32", "hash": 3330045144365190224},
			{"name": "par.sumFloat32", "hash": -8970847963922842567},
			{"name": "par.minInt", "hash": 3755891596433271276},
			{"name": "par.minFloat", "hash": -6089708910444618650},
			{"name": "par.minInt32", "hash": 1795660712750208871},
			{"name": "par.minUInt32", "hash": 5224463392098531538},
			{"name": "par.minFloat32", "hash": 3169224161701958629},
			{"name": "par.maxInt", "hash": -7970634428262483223},
			{"name": "par.maxFloat", "hash": -1553552811065348587},
			{"name": "par.maxInt32", "hash": 823330187899180063},
			{"name": "par.maxUInt32", "hash": -6452003177225978625},
			{"name": "par.maxFloat32", "hash": 1301188392794894118},
			{"name": "par.sumVec2", "hash": 8706265012730322285},
			{"name": "par.sumVec3", "hash": -5131357344409148058},
			{"name": "par.sumVec4", "hash": -1922342093142443490},
			{"name": "par.count", "hash": 8197731525995899677},
			{"name": "par.any", "hash": -592182413182401375},
			{"name": "par.all", "hash": 283383631972839438}
		])json"));

		check(test::compareValues(dump, expect, StringView("library signatures")),
				"ops-core: names and signature hashes match the golden table");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
