# Copyright (c) 2026 Stappler Team <admin@stappler.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# The executors of a run's parallel blocks: `threads` over the engine's task pool, `gpu` over a
# device of bytes (SPFlowExecutorGpu.h), and the `choice` that arbitrates between them and the
# machine's own front on a measured cost model. Kept out of the kernel for the same reason as the
# operations library: the kernel defines what an executor is asked (SPFlowMachine.h), and no
# particular answer to that question belongs in it. Nothing here knows a graphics API - a Vulkan or
# WebGPU device implements GpuDevice in the host that has one.

MODULE_STAPPLER_FLOW_EXECUTORS_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FLOW_EXECUTORS_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_FLOW_EXECUTORS_SRCS_DIRS :=
MODULE_STAPPLER_FLOW_EXECUTORS_SRCS_OBJS := $(STAPPLER_MODULE_DIR)/flow/executors/SPFlowExecutors.scu.cpp
MODULE_STAPPLER_FLOW_EXECUTORS_INCLUDES_DIRS :=
MODULE_STAPPLER_FLOW_EXECUTORS_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/flow/executors
# Direct users, each named: the task pool of `runtime` (sprt::dispatch::TaskQueue), the kernel's
# own headers and memory of `stappler_core`, and the machine contract of `stappler_flow`.
MODULE_STAPPLER_FLOW_EXECUTORS_DEPENDS_ON := runtime stappler_core stappler_flow

MODULE_STAPPLER_FLOW_EXECUTORS_SHARED_SPEC_SUMMARY := libstappler flow block executors: task pool, device and cost model

define MODULE_STAPPLER_FLOW_EXECUTORS_SHARED_SPEC_DESCRIPTION
Module libstappler-flow-executors implements BlockExecutor answers for stappler_flow parallel blocks
- the threads executor: batches of branch runs on the engine's task pool
- the gpu executor: a block's dispatch over a byte-level GpuDevice, with silence and loss breaks
- the choice executor: the author's demand, what is there, and a measured cost model
endef

$(call define_module, stappler_flow_executors, MODULE_STAPPLER_FLOW_EXECUTORS)
