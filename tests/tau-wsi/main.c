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

#include "../../runtime/toolchains/src/mesa/src/vulkan/wsi/wsi_common_tau.c"

enum {
	WIDTH = 13,
	HEIGHT = 9,
	STRIDE = 68,
	PITCH = 80
};
static uint8_t target[2][STRIDE * HEIGHT];
static uint8_t frames[3][PITCH * HEIGHT];
static uint8_t canvas[PITCH * HEIGHT];
static unsigned checks, failures, buffer_calls, shown, px, py, pw, ph;
static bool connected = true, fail_present;
static unsigned long long epoch = 1;

static void check(const char *name, bool ok) {
	checks++;
	if (!ok) {
		failures++;
		printf("FAIL %s\n", name);
		fflush(stdout);
	}
}
static int query(struct __sprt_tau_display_info *info, unsigned long long *generation) {
	if (!connected) {
		return -__SPRT_TAU_ECONNRESET;
	}
	if (generation) {
		*generation = epoch;
	}
	return 0;
}
static int buffer(unsigned index, void **map, unsigned long *size) {
	buffer_calls++;
	*map = target[index];
	*size = sizeof(target[index]);
	return 0;
}
static int present(unsigned index, unsigned x, unsigned y, unsigned w, unsigned h) {
	shown = index;
	px = x;
	py = y;
	pw = w;
	ph = h;
	return fail_present ? -__SPRT_TAU_ECONNRESET : 0;
}
static bool matches(unsigned index) {
	for (unsigned y = 0; y < HEIGHT; y++) {
		if (memcmp(target[index] + y * STRIDE, canvas + y * PITCH, WIDTH * 4)) {
			return false;
		}
		for (unsigned x = WIDTH * 4; x < STRIDE; x++) {
			if (target[index][y * STRIDE + x] != 0xcd) {
				return false;
			}
		}
	}
	return true;
}
static void paint(unsigned x, unsigned y, unsigned w, unsigned h, uint8_t color) {
	for (unsigned row = y; row < y + h; row++) {
		memset(canvas + row * PITCH + x * 4, color, w * 4);
	}
}

struct release_request {
	struct wsi_tau_swapchain *chain;
	uint32_t index;
};
static int release_worker(void *arg) {
	struct release_request *request = arg;
	struct timespec delay = {0, 5000000};
	thrd_sleep(&delay, NULL);
	return wsi_tau_swapchain_release_images(&request->chain->base, 1, &request->index);
}

int main(void) {
	struct wsi_tau_screen screen = {
		.opened = true,
		.generation = 1,
		.serial = 1,
		.width = WIDTH,
		.height = HEIGHT,
		.stride = STRIDE,
		.query = query,
		.buffer = buffer,
		.present = present,
	};
	mtx_init(&screen.mutex, mtx_plain);
	cnd_init(&screen.available);
	struct wsi_tau_swapchain *chain = calloc(1, sizeof(*chain) + 3 * sizeof(chain->images[0]));
	chain->base.image_count = 3;
	chain->screen = &screen;
	chain->extent = (VkExtent2D){WIDTH, HEIGHT};
	chain->generation = chain->serial = 1;
	for (unsigned i = 0; i < 3; i++) {
		chain->images[i].base.cpu_map = frames[i];
		chain->images[i].base.row_pitches[0] = PITCH;
	}
	for (unsigned i = 0; i < 2; i++) {
		chain->pending[i].x1 = WIDTH;
		chain->pending[i].y1 = HEIGHT;
	}
	memset(target, 0xcd, sizeof(target));
	memset(canvas, 0x12, sizeof(canvas));
	VkRectLayerKHR rect = {{0, 0}, {WIDTH, HEIGHT}, 0};
	VkPresentRegionKHR damage = {1, &rect};
	unsigned order[] = {2, 0, 1, 2, 1, 0};
	for (unsigned n = 0; n < 6; n++) {
		unsigned index = order[n];
		rect.offset = (VkOffset2D){(int)(n + 1), (int)n};
		rect.extent = (VkExtent2D){2, 1};
		paint(n + 1, n, 2, 1, n + 32);
		memcpy(frames[index], canvas, sizeof(canvas));
		chain->images[index].acquired = true;
		check("partial result",
				wsi_tau_swapchain_queue_present(&chain->base, index, 0, &damage) == VK_SUCCESS);
		check("independent display index", shown == n % 2);
		check("stride, damage history and padding", matches(shown));
		if (n < 2) {
			check("first use covers whole buffer",
					px == 0 && py == 0 && pw == WIDTH && ph == HEIGHT);
		}
	}
	damage.rectangleCount = 1;
	rect.offset = (VkOffset2D){-2, -1};
	rect.extent = (VkExtent2D){3, 2};
	paint(0, 0, 1, 1, 0x99);
	memcpy(frames[2], canvas, sizeof(canvas));
	check("clipped rectangle",
			wsi_tau_swapchain_queue_present(&chain->base, 2, 0, &damage) == VK_SUCCESS
					&& matches(shown));
	damage.rectangleCount = 0;
	paint(0, 0, WIDTH, HEIGHT, 0x77);
	memcpy(frames[0], canvas, sizeof(canvas));
	check("zero rectangles",
			wsi_tau_swapchain_queue_present(&chain->base, 0, 0, &damage) == VK_SUCCESS
					&& matches(shown));
	paint(0, 0, WIDTH, HEIGHT, 0x44);
	memcpy(frames[1], canvas, sizeof(canvas));
	check("absent regions",
			wsi_tau_swapchain_queue_present(&chain->base, 1, 0, NULL) == VK_SUCCESS
					&& matches(shown));
	for (unsigned i = 0; i < 3; i++) { chain->images[i].acquired = false; }
	VkAcquireNextImageInfoKHR acquire = {.timeout = 0};
	uint32_t indices[3], extra;
	for (unsigned i = 0; i < 3; i++) {
		check("acquire",
				wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &indices[i])
						== VK_SUCCESS);
	}
	check("unique images",
			indices[0] != indices[1] && indices[1] != indices[2] && indices[0] != indices[2]);
	check("not ready",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &extra) == VK_NOT_READY);
	acquire.timeout = 1000000;
	check("finite timeout",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &extra) == VK_TIMEOUT);
	struct release_request request = {chain, indices[0]};
	thrd_t worker;
	int status;
	check("release thread", thrd_create(&worker, release_worker, &request) == thrd_success);
	acquire.timeout = 1000000000;
	check("acquire wakes on release",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &extra) == VK_SUCCESS
					&& extra == indices[0]);
	check("release thread joined",
			thrd_join(worker, &status) == thrd_success && status == VK_SUCCESS);
	check("release", wsi_tau_swapchain_release_images(&chain->base, 1, indices + 1) == VK_SUCCESS);
	check("reacquire released",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &extra) == VK_SUCCESS
					&& extra == indices[1]);
	connected = false;
	unsigned calls = buffer_calls;
	check("lost acquire",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &extra)
					== VK_ERROR_OUT_OF_DATE_KHR);
	check("lost present",
			wsi_tau_swapchain_queue_present(&chain->base, 0, 0, NULL) == VK_ERROR_OUT_OF_DATE_KHR);
	check("gone buffers untouched", calls == buffer_calls);
	connected = true;
	epoch++;
	check("stale generation",
			wsi_tau_swapchain_queue_present(&chain->base, 0, 0, NULL) == VK_ERROR_OUT_OF_DATE_KHR
					&& calls == buffer_calls);
	chain->generation = epoch;
	screen.serial++;
	check("retired chain",
			wsi_tau_swapchain_queue_present(&chain->base, 0, 0, NULL) == VK_ERROR_OUT_OF_DATE_KHR
					&& calls == buffer_calls);
	chain->serial = screen.serial;
	screen.lost = false;
	fail_present = true;
	unsigned next = screen.next;
	check("loss during present",
			wsi_tau_swapchain_queue_present(&chain->base, 0, 0, NULL) == VK_ERROR_OUT_OF_DATE_KHR);
	check("failed present preserves buffer rotation", next == screen.next && screen.lost);
	check("failed chain stays out of date",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &extra)
					== VK_ERROR_OUT_OF_DATE_KHR);
	free(chain);
	cnd_destroy(&screen.available);
	mtx_destroy(&screen.mutex);
	printf("%u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;
}
