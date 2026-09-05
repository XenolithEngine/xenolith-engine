
// Embox EL0 timezone backend.
//
// UTC, and not as a placeholder: Embox has no timezone database to read and no
// syscall that would report an offset. An application that needs local time has
// to carry its own rules.

#include "time.h"

int daylight = 0;
long timezone = 0;
char tzname[2][64] = {"UTC", "UTC"};

namespace sprt {

__SPRT_C_FUNC void tzset(void) __SPRT_NOEXCEPT {
	// Nothing to recompute; reassert the globals in case a caller wrote to them.
	daylight = 0;
	timezone = 0;
	__builtin_memcpy(tzname[0], "UTC", 4);
	__builtin_memcpy(tzname[1], "UTC", 4);
}

} // namespace sprt
