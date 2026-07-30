# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

#
# sprt.dll + sprt.lib for Windows (SPRT_SHARED=1).
#
# Produced in $(dir $(BUILD_SHARED_LIBRARY)):
#   sprt.dll             the runtime
#   sprt_import.lib      raw import library, written by the linker
#   sprt_app_startup.lib the consumer-side startup stub
#   sprt.lib             the two merged - what consumers link
#
# Included twice by runtime/Makefile, because the two halves straddle universal.mk:
# link flags must be set before it, and the rules need BUILD_SHARED_LIBRARY, which only
# exists after it. SPRT_SHARED_SECTION selects which half.
#

ifeq ($(SPRT_SHARED_SECTION),flags)

# Everything a consumer names in source is covered by annotations: SPRT_API for the C
# and sprt C++ surface, _LIBCPP_EXPORTED_FROM_ABI / _LIBCPP_OVERRIDABLE_FUNC_VIS for
# libc++ (see include_libc/cxx/__config_site - it keys them on SPRT_BUILD_SHARED_RUNTIME
# exactly as SPRT_API is keyed).
#
# What annotations cannot cover is what the *compiler* emits references to on its own,
# with no declaration in scope. Those are listed here.
#
# All are functions, so an unannotated reference in the consumer resolves through the
# import thunk - no dllimport needed on the other side. Data symbols could not work this
# way; the ones that must be per-image live in libc_impl/app/windows/app_startup.cpp.
#
# __tlregdtor registers a thread_local destructor. Its list is thread-local state inside
# the runtime, so unlike the rest of the TLS apparatus (per-image) it must be shared.
SPRT_ABI_EXPORTS := \
	atexit \
	__tlregdtor \
	__chkstk \
	_purecall \
	_beginthreadex \
	_Init_thread_header \
	_Init_thread_footer \
	_Init_thread_abort \
	__sprt_malloc_usage \
	_CxxThrowException \
	__CxxFrameHandler3 \
	__std_terminate

# MSVC RTTI back-ends (libc_impl/src/windows/libcxx.cc). dynamic_cast and typeid lower
# straight to these; nothing declares them, so nothing can annotate them.
SPRT_ABI_EXPORTS += \
	__RTDynamicCast \
	__RTtypeid \
	__RTCastToVoid

# Emitted by clang's SimplifyLibCalls, which rewrites printf/fprintf families into these
# by plain name - bypassing the umbrella inline the source actually called.
SPRT_ABI_EXPORTS += \
	fwrite fputs fputc puts putchar memchr

# Math libcalls
SPRT_ABI_EXPORTS += \
	ceil ceilf floor floorf trunc truncf round roundf \
	rint rintf nearbyint nearbyintf \
	sqrt sqrtf fabs fabsf copysign copysignf fmod fmodf fma fmaf \
	pow powf exp expf exp2 exp2f log logf log2 log2f log10 log10f \
	sin sinf cos cosf tan tanf ldexp ldexpf

# String libcalls
SPRT_ABI_EXPORTS += \
	_alloca_probe \
	strcat strcspn strpbrk strrchr strspn

# The replaceable operators, by mangled name - the one part of the C++ surface an
# annotation cannot reach. The compiler implicitly declares operator new/delete in every
# TU, so sprt's definitions in libc_impl/src/builtin_libcxx.cpp are redeclarations, and a
# dll attribute cannot be added by a redeclaration (clang rejects it outright for the
# sized deletes and silently ignores it elsewhere). libc++ annotates its own declarations
# with _LIBCPP_OVERRIDABLE_FUNC_VIS, but those are not in scope in a freestanding TU.
#
# The mangling is fixed by the MSVC C++ ABI and identical on both 64-bit Windows
# targets. Consumers reference the plain names and bind through the import thunk.
SPRT_ABI_EXPORTS += \
	'??2@YAPEAX_K@Z' \
	'??2@YAPEAX_KAEBUnothrow_t@std@@@Z' \
	'??2@YAPEAX_KW4align_val_t@std@@@Z' \
	'??2@YAPEAX_KW4align_val_t@std@@AEBUnothrow_t@1@@Z' \
	'??_U@YAPEAX_K@Z' \
	'??_U@YAPEAX_KAEBUnothrow_t@std@@@Z' \
	'??_U@YAPEAX_KW4align_val_t@std@@@Z' \
	'??_U@YAPEAX_KW4align_val_t@std@@AEBUnothrow_t@1@@Z' \
	'??3@YAXPEAX@Z' \
	'??3@YAXPEAX_K@Z' \
	'??3@YAXPEAXAEBUnothrow_t@std@@@Z' \
	'??3@YAXPEAXW4align_val_t@std@@@Z' \
	'??3@YAXPEAX_KW4align_val_t@std@@@Z' \
	'??3@YAXPEAXW4align_val_t@std@@AEBUnothrow_t@1@@Z' \
	'??_V@YAXPEAX@Z' \
	'??_V@YAXPEAX_K@Z' \
	'??_V@YAXPEAXAEBUnothrow_t@std@@@Z' \
	'??_V@YAXPEAXW4align_val_t@std@@@Z' \
	'??_V@YAXPEAX_KW4align_val_t@std@@@Z' \
	'??_V@YAXPEAXW4align_val_t@std@@AEBUnothrow_t@1@@Z'

# The linker names the import library after the DLL and would write it to sprt.lib - the
# name the merged library takes. Send the raw import side elsewhere.
#
# Recursive assignment: BUILD_SHARED_LIBRARY only exists after utils/build-targets.mk,
# which runs after this file but before c/apply.mk expands LOCAL_LDFLAGS.
SPRT_IMPORT_LIB = $(dir $(BUILD_SHARED_LIBRARY))sprt_import.lib

LOCAL_LDFLAGS = $(addprefix -Xlinker /EXPORT:,$(SPRT_ABI_EXPORTS)) \
	-Xlinker /IMPLIB:$(SPRT_IMPORT_LIB)

endif # SPRT_SHARED_SECTION == flags


ifeq ($(SPRT_SHARED_SECTION),rules)

# llvm-lib, not llvm-ar: merging archives (flattening their members into the output) is
# lib.exe behaviour; ar would nest the inputs as members instead.
SPRT_LLVM_LIB ?= $(dir $(HOST_AR))llvm-lib$(if $(findstring .exe,$(HOST_AR)),.exe,)

ifeq ($(TARGET_ARCH),aarch64)
SPRT_LIB_MACHINE := arm64
else
SPRT_LIB_MACHINE := x64
endif

# The executable half of a shared-runtime build needs its own translation unit: the .CRT
# sections, TLS directory and /GS cookie are per-image and cannot be imported (see
# libc_impl/app/windows/app_startup.cpp). Baking it into the library consumers already
# link saves every one of them from adding that file by hand - the same arrangement
# msvcrt.lib uses for the MSVC startup stubs.
#
# Overriding the entry point still works: the member is pulled in regardless (it also
# carries _fltused, the cookie and the TLS directory), so mainCRTStartup is a COFF weak
# external and an image defining its own wins instead of colliding.
#
# Two objects, not one: app_startup.cpp holds the per-image machinery plus the DLL entry
# point, exe_startup.cpp only the executable one. An archive member is pulled whole, so a
# consumer DLL sharing an object with mainCRTStartup would inherit its reference to main()
# and fail to link - which is what libclang.dll and LTO.dll did.
SPRT_APP_STARTUP_SRCS := \
	$(LOCAL_ROOT)/libc_impl/app/windows/app_startup.cpp \
	$(LOCAL_ROOT)/libc_impl/app/windows/exe_startup.cpp

SPRT_APP_STARTUP_OBJDIR := $(dir $(BUILD_SHARED_LIBRARY))app_startup_objs
SPRT_APP_STARTUP_OBJS := $(patsubst %.cpp,$(SPRT_APP_STARTUP_OBJDIR)/%.cpp.o,$(notdir $(SPRT_APP_STARTUP_SRCS)))
SPRT_APP_STARTUP_LIB := $(dir $(BUILD_SHARED_LIBRARY))sprt_app_startup.lib
SPRT_MERGED_LIB := $(dir $(BUILD_SHARED_LIBRARY))sprt.lib

# Compiled as consumer code, not as part of the runtime: SPRT_SHARED_RUNTIME puts
# SPRT_API on the import side, so the stub reaches the runtime the way an application
# does. BUILD_LIB_CXXFLAGS supplies the target, sysroot and consumer include set; the
# runtime's own -DSPRT_BUILD_RUNTIME lives in per-module private flags and correctly does
# not reach here.
SPRT_APP_STARTUP_CXXFLAGS := $(BUILD_LIB_CXXFLAGS) -DSPRT_SHARED_RUNTIME -nostdinc++

$(SPRT_APP_STARTUP_OBJDIR)/%.cpp.o: $(LOCAL_ROOT)/libc_impl/app/windows/%.cpp $(LOCAL_MAKEFILE)
	@$(call rule_mkdir,$(dir $@))
	$(GLOBAL_QUIET_CC)
	$(VERBOSE_GUARD) $(GLOBAL_CXX) $(SPRT_APP_STARTUP_CXXFLAGS) -c $< -o $@
$(SPRT_APP_STARTUP_OBJDIR)/%.cpp.o:.TARGET_NAME := [c++] $*.cpp

$(SPRT_APP_STARTUP_LIB): $(SPRT_APP_STARTUP_OBJS)
	@$(call rule_rm,$@)
	$(VERBOSE_GUARD) $(SPRT_LLVM_LIB) /out:$@ $(SPRT_APP_STARTUP_OBJS) /machine:$(SPRT_LIB_MACHINE)
$(SPRT_APP_STARTUP_LIB):.TARGET_NAME := [Link (static)] $(SPRT_APP_STARTUP_LIB)

# Depends on the DLL, not on sprt_import.lib: the import library is a side effect of that
# link and has no rule of its own.
$(SPRT_MERGED_LIB): $(BUILD_SHARED_LIBRARY) $(SPRT_APP_STARTUP_LIB)
	@$(call rule_rm,$@)
	$(VERBOSE_GUARD) $(SPRT_LLVM_LIB) /out:$@ \
		$(SPRT_IMPORT_LIB) $(SPRT_APP_STARTUP_LIB) /machine:$(SPRT_LIB_MACHINE)
$(SPRT_MERGED_LIB):.TARGET_NAME := [Merge] $(SPRT_MERGED_LIB)

all: $(SPRT_MERGED_LIB)

endif # SPRT_SHARED_SECTION == rules
