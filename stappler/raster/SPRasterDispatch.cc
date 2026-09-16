/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "SPRasterKernel.h"

// Which pixel loops this process runs. Resolved once, from what the CPU can do and from an
// override, and never per frame - let alone per pixel.

namespace STAPPLER_VERSIONIZED stappler::raster {

StringView getKernelSetName(KernelSet set) {
	switch (set) {
	case KernelSet::Scalar: return StringView("scalar");
	case KernelSet::Swar: return StringView("swar");
	case KernelSet::Sse2: return StringView("sse2");
	case KernelSet::Sse41: return StringView("sse41");
	case KernelSet::Avx2: return StringView("avx2");
	case KernelSet::Neon: return StringView("neon");
	}
	return StringView("unknown");
}

// Best first. A set whose accessor returns null is not implemented for this architecture, and one
// whose CPU requirement is unmet is filtered out by the accessor itself.
static Vector<const KernelTable *> Dispatch_collect() {
	Vector<const KernelTable *> tables;

	// Widest first. Each accessor answers null when this build has no such kernels or this CPU
	// cannot run them, so the order here is preference, not availability.
	if (auto table = getAvx2Kernels()) {
		tables.emplace_back(table);
	}

	if (auto table = getSse41Kernels()) {
		tables.emplace_back(table);
	}

	if (auto table = getSse2Kernels()) {
		tables.emplace_back(table);
	}

	if (auto table = getNeonKernels()) {
		tables.emplace_back(table);
	}

	// SWAR needs nothing from the CPU - it is 64-bit integer arithmetic - so it is always here and
	// always ahead of scalar.
	if (auto table = getSwarKernels()) {
		tables.emplace_back(table);
	}

	// Scalar last: it is the fallback and the reference, never the fastest.
	if (auto table = getScalarKernels()) {
		tables.emplace_back(table);
	}

	return tables;
}

SpanView<const KernelTable *> getAvailableKernels() {
	static const Vector<const KernelTable *> s_tables = Dispatch_collect();
	return s_tables;
}

static const KernelTable *Dispatch_resolve() {
	auto tables = getAvailableKernels();
	if (tables.empty()) {
		// getScalarKernels() cannot return null, so this is unreachable rather than a fallback.
		return nullptr;
	}

	// The override exists for two callers that both need it: the parity gate, which runs every set
	// against the scalar one, and the benchmark, which times them separately.
	if (auto value = ::getenv("SP_RASTER_KERNELS")) {
		auto name = StringView(value);
		for (auto &it : tables) {
			if (getKernelSetName(it->set) == name) {
				return it;
			}
		}

		// Not a silent fallback: a typo here would otherwise look like "the vector set is no
		// faster", which is exactly the conclusion that is hard to un-draw.
		StringStream available;
		for (auto &it : tables) { available << " " << getKernelSetName(it->set); }
		log::source().error("raster",
				"SP_RASTER_KERNELS=", name, " is not available here; available:", available.str());
	}

	return tables.front();
}

const KernelTable &getKernels() {
	static const KernelTable *s_table = [] {
		auto table = Dispatch_resolve();

		// Which set is in use has to be visible: a benchmark that silently measured the scalar
		// path reports a real number for the wrong thing. The list is logged too, because that is
		// how a harness learns what it can ask for on this machine.
		StringStream available;
		for (auto &it : getAvailableKernels()) { available << " " << getKernelSetName(it->set); }
		log::source().debug("raster", "available kernel sets:", available.str());
		log::source().debug("raster", "using kernel set: ", getKernelSetName(table->set));
		return table;
	}();
	return *s_table;
}

StringView getActiveKernelSetName() { return getKernelSetName(getKernels().set); }

} // namespace stappler::raster
