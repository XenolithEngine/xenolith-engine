// Minimal WebGPU driver: the engine logic runs in wasm (inside a Web Worker) and drives
// per-frame rendering to an OffscreenCanvas through the `gpu` host-import ABI. JS owns the
// GPU plumbing (adapter/device/pipeline over navigator.gpu); wasm owns the frame logic.
#include <math.h>

extern "C" __attribute__((import_module("gpu"), import_name("render")))
void gpu_render(float r, float g, float b, float tri_r, float tri_g, float tri_b);

// Called by the worker's requestAnimationFrame loop, t in milliseconds.
extern "C" void xl_frame(double t) {
	float p = (float)(t * 0.0012);
	// animated background + a colour-cycling triangle, all computed in wasm
	float br = 0.10f + 0.06f * sinf(p);
	float bg = 0.12f + 0.06f * sinf(p + 2.094f);
	float bb = 0.18f + 0.06f * sinf(p + 4.188f);
	float tr = 0.5f + 0.5f * sinf(p * 1.7f);
	float tg = 0.5f + 0.5f * sinf(p * 1.7f + 2.094f);
	float tb = 0.5f + 0.5f * sinf(p * 1.7f + 4.188f);
	gpu_render(br, bg, bb, tr, tg, tb);
}

int main(int, char **) { return 0; }
