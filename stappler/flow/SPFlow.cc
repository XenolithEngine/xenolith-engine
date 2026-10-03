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

// The diagnostic accumulator, shared by operation registration, graph validation, the build and
// the run.

#include "SPFlow.h"
#include "SPFlowEnv.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

void DiagReport::add(const Diag &d) {
	note(d.severity);
	value::report(_sink, d);
}

void DiagReport::note(DiagSeverity severity) {
	switch (severity) {
	case DiagSeverity::Error: ++_errors; break;
	case DiagSeverity::Warning: ++_warnings; break;
	case DiagSeverity::Advice: ++_advice; break;
	}
}

static Diag makeGraphDiag(DiagSeverity severity, DiagCode code, const DiagText &text,
		DiagLocus locus) {
	Diag d;
	d.domain = value::DiagDomain::Graph;
	d.code = uint16_t(code);
	d.detail = uint16_t(text.detail);
	d.severity = severity;
	d.locus = uint16_t(locus);
	for (uint32_t i = 0; i < text.count; ++i) { d.args[i] = text.args[i]; }
	d.argCount = text.count;
	return d;
}

void DiagReport::report(DiagSeverity severity, DiagCode code, const DiagText &text,
		const Diag *inner) {
	auto d = makeGraphDiag(severity, code, text, DiagLocus::None);
	d.inner = inner;
	add(d);
}

void DiagReport::reportNode(DiagSeverity severity, DiagCode code, NodeId node,
		const DiagText &text) {
	auto d = makeGraphDiag(severity, code, text, DiagLocus::Node);
	d.locusValue[0] = int64_t(node);
	add(d);
}

void DiagReport::reportPin(DiagSeverity severity, DiagCode code, NodeId node, StringView pin,
		const DiagText &text, const Diag *inner) {
	auto d = makeGraphDiag(severity, code, text, DiagLocus::Pin);
	d.locusValue[0] = int64_t(node);
	d.locusName[0] = pin;
	d.inner = inner;
	add(d);
}

void DiagReport::reportSetting(DiagSeverity severity, DiagCode code, NodeId node,
		StringView setting, const DiagText &text) {
	auto d = makeGraphDiag(severity, code, text, DiagLocus::Setting);
	d.locusValue[0] = int64_t(node);
	d.locusName[0] = setting;
	add(d);
}

void DiagReport::reportEdge(DiagSeverity severity, DiagCode code, NodeId from, StringView fromPin,
		NodeId to, StringView toPin, const DiagText &text) {
	auto d = makeGraphDiag(severity, code, text, DiagLocus::Edge);
	d.locusValue[0] = int64_t(from);
	d.locusValue[1] = int64_t(to);
	d.locusName[0] = fromPin;
	d.locusName[1] = toPin;
	add(d);
}

void DiagReport::reportAt(DiagSeverity severity, DiagCode code, const DiagText &text,
		DiagLocus locus, SpanView<int64_t> values, SpanView<StringView> names) {
	auto d = makeGraphDiag(severity, code, text, locus);
	for (uint32_t i = 0; i < values.size() && i < 4; ++i) { d.locusValue[i] = values[i]; }
	for (uint32_t i = 0; i < names.size() && i < 2; ++i) { d.locusName[i] = names[i]; }
	add(d);
}

void writeDiagNumbers(mem_std::Value *out, const Diag &d) {
	if (!out) {
		return;
	}
	if (!out->isArray()) {
		*out = mem_std::Value(mem_std::Value::Type::ARRAY);
	}
	mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
	entry.setInteger(int64_t(d.domain), "domain");
	entry.setInteger(int64_t(d.code), "code");
	entry.setInteger(int64_t(d.detail), "detail");
	entry.setInteger(int64_t(d.severity), "severity");
	entry.setInteger(int64_t(d.locus), "locus");
	auto &at = entry.newArray("at");
	for (auto v : d.locusValue) { at.addInteger(v); }
	auto &names = entry.newArray("names");
	for (auto &n : d.locusName) { names.addString(n); }
	auto &args = entry.newArray("args");
	for (uint32_t i = 0; i < d.argCount; ++i) {
		mem_std::Value arg(mem_std::Value::Type::DICTIONARY);
		arg.setInteger(int64_t(d.args[i].kind), "kind");
		if (d.args[i].kind == value::DiagArgKind::Name) {
			arg.setString(d.args[i].name, "name");
		} else {
			arg.setInteger(d.args[i].number, "number");
		}
		args.addValue(sprt::move(arg));
	}
	out->addValue(sprt::move(entry));
}

} // namespace stappler::flow
