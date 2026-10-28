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
static int open_then_lose(int wait, struct __sprt_tau_display_info *info) {
	connected = false;
	return 0;
}
static void close_session(void) { connected = false; }
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

/* The planes of dev.display@2 for a GPU: stubs of its hooks and of the fence wait. */
static unsigned shows, last_shown, fence_waits;
static bool fail_show;
static int p_query(struct __sprt_tau_display2_info *info, unsigned long long *generation) {
	if (info) {
		*info = (struct __sprt_tau_display2_info){WIDTH, HEIGHT, 60'000, 2};
	}
	if (generation) {
		*generation = 7;
	}
	return 0;
}
static int p_open(int wait, struct __sprt_tau_display2_info *info) { return p_query(info, NULL); }
static int p_import(unsigned t, unsigned f, unsigned w, unsigned h, unsigned s, unsigned o,
		unsigned *b) {
	return 0;
}
static int p_release(unsigned buffer) { return 0; }
static int p_show(unsigned buffer, unsigned long long *blank) {
	if (fail_show) {
		return -__SPRT_TAU_ECONNRESET;
	}
	shows++;
	last_shown = buffer;
	*blank = shows;
	return 0;
}
static int buf_token(const struct __sprt_tau_gpu_buf *b, unsigned *slot) { return 0; }
static struct __sprt_tau_gpu_buf *dmabuf_buf(int fd) { return NULL; }
static VkResult VKAPI_PTR wait_fences(VkDevice d, uint32_t n, const VkFence *f, VkBool32 all,
		uint64_t t) {
	fence_waits++;
	return VK_SUCCESS;
}

static void planes(void) {
	struct wsi_tau_screen screen = {
		.query = query,
		.p_open = p_open,
		.p_query = p_query,
		.p_import = p_import,
		.p_release = p_release,
		.p_show = p_show,
		.buf_token = buf_token,
		.dmabuf_buf = dmabuf_buf,
	};
	mtx_init(&screen.mutex, mtx_plain);
	cnd_init(&screen.available);
	mtx_lock(&screen.mutex);
	check("a GPU takes the planes",
			wsi_tau_connect(&screen, true) && screen.planes && screen.width == WIDTH);
	check("the planes keep dev.display@1 away", !wsi_tau_connect(&screen, false));
	mtx_unlock(&screen.mutex);
	struct wsi_device wsi = {.sw = false};
	wsi.WaitForFences = wait_fences;
	VkFence fences[3] = {VK_NULL_HANDLE};
	struct wsi_tau_swapchain *chain = calloc(1, sizeof(*chain) + 3 * sizeof(chain->images[0]));
	chain->base.wsi = &wsi;
	chain->base.fences = fences;
	chain->base.image_count = 3;
	chain->screen = &screen;
	chain->extent = (VkExtent2D){WIDTH, HEIGHT};
	chain->generation = 7;
	chain->serial = screen.serial;
	for (unsigned i = 0; i < 3; i++) {
		chain->images[i].buffer = i + 1;
		chain->images[i].fd = -1;
	}
	VkAcquireNextImageInfoKHR acquire = {.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.timeout = 0};
	uint32_t a = 0, b = 0, c = 0;
	check("acquire on the planes",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &a) == VK_SUCCESS);
	check("present commits the image's buffer after its fence",
			wsi_tau_swapchain_queue_present(&chain->base, a, 0, NULL) == VK_SUCCESS && shows == 1
					&& last_shown == a + 1 && fence_waits == 1);
	check("the other two are free",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &b) == VK_SUCCESS
					&& wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &c)
							== VK_SUCCESS
					&& b != a && c != a);
	check("the shown one is not acquired",
			wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &b) == VK_NOT_READY);
	check("the next present frees it",
			wsi_tau_swapchain_queue_present(&chain->base, c, 0, NULL) == VK_SUCCESS
					&& last_shown == c + 1
					&& wsi_tau_swapchain_acquire_next_image(&chain->base, &acquire, &b)
							== VK_SUCCESS
					&& b == a);
	fail_show = true;
	check("a lost session is out of date",
			wsi_tau_swapchain_queue_present(&chain->base, b, 0, NULL) == VK_ERROR_OUT_OF_DATE_KHR
					&& screen.lost);
	free(chain);
	cnd_destroy(&screen.available);
	mtx_destroy(&screen.mutex);
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
	struct wsi_device wsi = {.sw = true};
	chain->base.wsi = &wsi;
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
	screen.open = open_then_lose;
	screen.close = close_session;
	screen.opened = connected = false;
	mtx_lock(&screen.mutex);
	check("connect observes immediate loss", !wsi_tau_connect(&screen, false));
	check("successful open stays owned until close", screen.opened);
	mtx_unlock(&screen.mutex);
	free(chain);
	cnd_destroy(&screen.available);
	mtx_destroy(&screen.mutex);
	planes();
	printf("%u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;
}
