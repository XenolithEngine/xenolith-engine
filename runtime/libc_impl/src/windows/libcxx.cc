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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sprt/cxx/typeinfo>
#include <sprt/cxx/new>
#include <sprt/cxx/detail/hash.h>
#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/debug_api.h>
#include <sprt/cxx/mutex>

#include "Demangle/Demangle.h"

using vtable_fn = void (*)(void);

// From rttidata.h
struct RTTICompleteObjectLocator {
	unsigned long signature;
	unsigned long classOffset;
	unsigned long cdOffset;
	int rvaTypeDescriptor; // Image relative offset of TypeDescriptor
	int rvaClassDescriptor; // Image relative offset of _RTTIClassHierarchyDescriptor
	int rvaSelf; // Image relative offset of this object
};

struct TypeDescriptor {
	const TypeDescriptor *type_info_vtable;
	void *spare;
	char __decorated_name[128];
};

typedef struct PMD {
	int mdisp; // Offset of intended data within base
	int pdisp; // Displacement to virtual base pointer
	int vdisp; // Index within vbTable to offset of base
} PMD;

struct RTTIBaseClassDescriptor {
	int rvaTypeDescriptor; // Image relative offset of TypeDescriptor
	unsigned long numContainedBases;
	PMD where;
	unsigned long attributes;
	int rvaClassDescriptor; // Image relative offset of RTTIClassHierarchyDescriptor
};

struct RTTIBaseClassArray {
	int rvaBases[4];
};

struct RTTIClassHierarchyDescriptor {
	unsigned long signature;
	unsigned long attributes;
	unsigned long numBaseClasses;
	int pBaseClassArray; // Image relative offset of _RTTIBaseClassArray
};

__SPRT_C_FUNC const TypeDescriptor s_type_info_vftable __asm__("??_7type_info@@6B@") = {
	&s_type_info_vftable,
	nullptr,
	".?AVtype_info@@",
};

__SPRT_C_FUNC FILE *stderr;

__SPRT_C_FUNC const type_info *__RTtypeid(void *obj) {
	if (!obj) {
		fprintf(stderr, "Fatal: %s; aborting;\n", "typeid(nullptr) called");
		abort();
	}

	auto locator = (const RTTICompleteObjectLocator *)(*(const vtable_fn **)obj)[-1];

	// Now we know the absolute address of locator; with rvaSelf field, we can find
	// an address for base module of the object

	// It's possible, that locator is empty, in this case we need general lookup from OS
	UINT_PTR base = 0;
	if (!locator->signature) {
		void *module = nullptr;
		if (RtlPcToFileHeader((void *)locator, &module)) {
			base = reinterpret_cast<UINT_PTR>(module);
		} else {
			fprintf(stderr, "Fatal: %s; aborting;\n", "No module for typeid found");
			abort();
		}
	} else {
		base = reinterpret_cast<UINT_PTR>(locator) - locator->rvaSelf;
	}

	return (const type_info *)(base + locator->rvaTypeDescriptor);
}

namespace sprt {

int type_info::__compare(const type_info &__rhs) const noexcept {
	return strcmp(__data.__decorated_name, __rhs.__data.__decorated_name);
}

__SPRT_PUSH_ALLOW_CXXABI_ALLOC
type_info::~type_info() { }
__SPRT_POP_ALLOW_CXXABI_ALLOC

const char *type_info::name() const noexcept {
	return __data.__decorated_name; //
}

size_t type_info::hash_code() const noexcept {
	return sprt::hash<void>()((const char *)__data.__decorated_name);
}

} // namespace sprt

namespace abi {

static sprt::mutex s_cxa_demangle_lock;

extern "C" char *__cxa_demangle(const char *mangled_name, char *output_buffer,
		__SPRT_ID(size_t) * length, int *status) {
	return llvm::microsoftDemangle(mangled_name, length, status, llvm::MSDF_None);
}
} // namespace abi
