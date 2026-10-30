# Tau WSI native contracts

This test includes Mesa's Tau WSI backend and supplies display hooks with two guarded buffers.
It checks independent image acquisition, damage across alternating buffers, different row
strides, full presents, timeouts and loss of a display generation without a Vulkan device.

Build the Tau Mesa sources and x86_64 sysroot first. The generated Mesa header directory can be
selected with `TAU_WSI_BUILD`; its default is the x86_64 Tau Mesa build under `runtime/toolchains`.
Build this project through `xenolith-cli`, then run `tauwsitest`. The engine's full check runner
includes it when built; Tau also registers it as `vk-wsi-unit-test`.
