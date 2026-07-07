// WebGPU triangle through the real webgpu.h C ABI (host-bound to navigator.gpu in JS).
// Proves the binding foundation: handle table, descriptor marshalling, async bootstrap.
#include "webgpu.h"
#include <math.h>

static WGPUDevice g_device = 0;
static WGPUQueue g_queue = 0;
static WGPUSurface g_surface = 0;
static WGPURenderPipeline g_pipeline = 0;

static WGPUStringView sv(const char *s) { WGPUStringView v; v.data = s; v.length = WGPU_STRLEN; return v; }

static const char *WGSL =
	"@vertex fn vs(@builtin(vertex_index) i:u32) -> @builtin(position) vec4f {\n"
	"  var p = array<vec2f,3>(vec2f(0.0,0.65), vec2f(-0.65,-0.55), vec2f(0.65,-0.55));\n"
	"  return vec4f(p[i], 0.0, 1.0);\n"
	"}\n"
	"@fragment fn fs() -> @location(0) vec4f { return vec4f(0.95, 0.55, 0.12, 1.0); }\n";

static void onDevice(uint32_t st, WGPUDevice d, const char *md, size_t ml, void *u) { g_device = d; }
static void onAdapter(uint32_t st, WGPUAdapter a, const char *md, size_t ml, void *u) {
	wgpuAdapterRequestDevice(a, 0, onDevice, 0); // resolves synchronously (bootstrap pre-done)
}

static void xl_init(void) {
	WGPUInstance inst = wgpuCreateInstance(0);
	wgpuInstanceRequestAdapter(inst, 0, onAdapter, 0);
	g_queue = wgpuDeviceGetQueue(g_device);
	g_surface = wgpuGetCanvasSurface(inst);

	WGPUShaderSourceWGSL wgsl; __builtin_memset(&wgsl, 0, sizeof(wgsl));
	wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
	wgsl.code = sv(WGSL);
	WGPUShaderModuleDescriptor smd; __builtin_memset(&smd, 0, sizeof(smd));
	smd.nextInChain = &wgsl.chain;
	WGPUShaderModule sm = wgpuDeviceCreateShaderModule(g_device, &smd);

	WGPUColorTargetState target; __builtin_memset(&target, 0, sizeof(target));
	target.format = WGPUTextureFormat_BGRA8Unorm;
	target.writeMask = 0xF;
	WGPUFragmentState frag; __builtin_memset(&frag, 0, sizeof(frag));
	frag.module = sm; frag.entryPoint = sv("fs"); frag.targetCount = 1; frag.targets = &target;

	WGPURenderPipelineDescriptor pd; __builtin_memset(&pd, 0, sizeof(pd));
	pd.vertex.module = sm; pd.vertex.entryPoint = sv("vs");
	pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	pd.multisample.count = 1; pd.multisample.mask = 0xFFFFFFFF;
	pd.fragment = &frag;
	g_pipeline = wgpuDeviceCreateRenderPipeline(g_device, &pd);
}

extern "C" void xl_frame(double t) {
	if (!g_device) { xl_init(); }
	float p = (float)(t * 0.0012);
	WGPUTexture tex = wgpuSurfaceGetCurrentTexture(g_surface);
	WGPUTextureView view = wgpuTextureCreateView(tex, 0);
	WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(g_device, 0);

	WGPURenderPassColorAttachment att; __builtin_memset(&att, 0, sizeof(att));
	att.view = view; att.loadOp = WGPULoadOp_Clear; att.storeOp = WGPUStoreOp_Store;
	att.clearValue.r = 0.10 + 0.06 * sin(p);
	att.clearValue.g = 0.12 + 0.06 * sin(p + 2.094);
	att.clearValue.b = 0.18 + 0.06 * sin(p + 4.188);
	att.clearValue.a = 1.0;
	WGPURenderPassDescriptor rp; __builtin_memset(&rp, 0, sizeof(rp));
	rp.colorAttachmentCount = 1; rp.colorAttachments = &att;

	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
	wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
	wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
	wgpuRenderPassEncoderEnd(pass);
	WGPUCommandBuffer cb = wgpuCommandEncoderFinish(enc, 0);
	wgpuQueueSubmit(g_queue, 1, &cb);
	wgpuSurfacePresent(g_surface);

	wgpuCommandBufferRelease(cb); wgpuRenderPassEncoderRelease(pass);
	wgpuCommandEncoderRelease(enc); wgpuTextureViewRelease(view); wgpuTextureRelease(tex);
}

int main(int, char **) { return 0; }
