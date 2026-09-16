// "Боевые данные": open a real external file the browser pulls via fetch (Bundled mount).
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
	(void)argc; (void)argv;
	FILE *f = fopen("/rom/data.txt", "rb");
	printf("fopen(/rom/data.txt)=%s\n", f ? "ok" : "null");
	if (!f) return 1;
	char buf[256]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); buf[n] = 0;
	printf("fread=%zu bytes, content:\n%s", n, buf);
	fclose(f);
	return 0;
}
