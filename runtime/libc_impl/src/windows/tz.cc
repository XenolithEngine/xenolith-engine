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

#include "time.h"
#include "specific.h"

#include <sprt/wrappers/windows/time_api.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/windows.h>

extern "C" {
int daylight = 0;
long timezone = 0;
char tzname[2][64];
int dst_off = 0;
}

namespace sprt {

__SPRT_C_FUNC void tzset(void) __SPRT_NOEXCEPT {

	DYNAMIC_TIME_ZONE_INFORMATION dtzi;
	DWORD result;

	// Get dynamic timezone information (includes DST rules)
	result = GetDynamicTimeZoneInformation(&dtzi);

	if (result == TIME_ZONE_ID_INVALID) {
		// Fall back to basic timezone info
		TIME_ZONE_INFORMATION tzi;
		result = GetTimeZoneInformation(&tzi);
		if (result != TIME_ZONE_ID_INVALID) {
			timezone = (long)tzi.Bias * 60; // Convert minutes to seconds
			daylight = (result == TIME_ZONE_ID_DAYLIGHT) ? 1 : 0;

			// Calculate DST offset: when DST is active, the DaylightBias is applied
			// dst_off stores the timezone offset used during daylight saving time
			if (result == TIME_ZONE_ID_DAYLIGHT) {
				dst_off = (long)tzi.DaylightBias * 60; // Convert minutes to seconds
			} else {
				dst_off = timezone; // Standard time offset
			}

			// On success WideCharToMultiByte NUL-terminates (source length -1);
			// on failure (e.g. name too long) it returns 0 and may leave the
			// buffer non-terminated, so force termination in that case.
			if (WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, tzname[0], 64, NULL, NULL)
					== 0) {
				tzname[0][63] = '\0';
			}
			if (WideCharToMultiByte(CP_UTF8, 0, tzi.DaylightName, -1, tzname[1], 64, NULL, NULL)
					== 0) {
				tzname[1][63] = '\0';
			}
		}
		return;
	}

	// Set global variables
	timezone = (long)dtzi.Bias * 60; // Convert minutes to seconds
	daylight = (result == TIME_ZONE_ID_DAYLIGHT) ? 1 : 0;

	// Calculate DST offset: when DST is active, the DaylightBias is applied
	// dst_off stores the timezone offset used during daylight saving time
	if (result == TIME_ZONE_ID_DAYLIGHT) {
		dst_off = (long)dtzi.DaylightBias * 60; // Convert minutes to seconds
	} else {
		dst_off = timezone; // Standard time offset
	}

	// Copy timezone names. On success WideCharToMultiByte NUL-terminates
	// (source length -1); on failure (e.g. name too long) it returns 0 and may
	// leave the buffer non-terminated, so force termination in that case.
	if (WideCharToMultiByte(CP_UTF8, 0, dtzi.StandardName, -1, tzname[0], 64, NULL, NULL) == 0) {
		tzname[0][63] = '\0';
	}
	if (WideCharToMultiByte(CP_UTF8, 0, dtzi.DaylightName, -1, tzname[1], 64, NULL, NULL) == 0) {
		tzname[1][63] = '\0';
	}
}

} // namespace sprt
