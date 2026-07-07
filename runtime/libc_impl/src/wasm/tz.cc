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

// WebAssembly timezone backend (wasm-port-draft.adoc §3.5).
//
// The C-library timezone globals default to UTC. Wiring the real host timezone
// (via the tz_offset/tz_name Intl imports, cached per year) is a later milestone.

#include "time.h"

int daylight = 0;
long timezone = 0;
char tzname[2][64] = {"UTC", "UTC"};

namespace sprt {

__SPRT_C_FUNC void tzset(void) __SPRT_NOEXCEPT {
	// UTC only for now: nothing to recompute.
	daylight = 0;
	timezone = 0;
	tzname[0][0] = 'U';
	tzname[0][1] = 'T';
	tzname[0][2] = 'C';
	tzname[0][3] = 0;
	tzname[1][0] = 'U';
	tzname[1][1] = 'T';
	tzname[1][2] = 'C';
	tzname[1][3] = 0;
}

} // namespace sprt
