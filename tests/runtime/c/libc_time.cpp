#include <time.h>
#include <stdio.h>

namespace sprt {

void performLibcTimeTest() {
	auto now = time(nullptr);

	tm gmt, local;
	gmtime_r(&now, &gmt);
	localtime_r(&now, &local);

	char buf[30] = {0};
	printf("UTC (asctime_r): %s\n", asctime_r(&gmt, buf));
	printf("Local (asctime_r): %s\n", asctime_r(&local, buf));

	strftime(buf, 29, "%c", &local);
	printf("Local (strftime): %s\n", buf);
}

} // namespace sprt
