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

#include <sprt/cxx/typeinfo>
#include <sprt/wrappers/windows/dl_api.h>

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

	// Now we know the absolute address of locator; using rvaSelf field, we can find
	// an address for base module of the object

	// It's possible, that locator is empty, in this case we need general loocup from OS
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

/*
__SPRT_C_FUNC void *__thiscall type_info_vector_dtor(type_info *_this, unsigned int flags) {
	if (flags & 2) {
		 we have an array, with the number of elements stored before the first object 
		INT_PTR i, *ptr = (INT_PTR *)_this - 1;

		//for (i = *ptr - 1; i >= 0; i--) { free(_this[i].name); }
		free(ptr);
	} else {
		//free(_this->name);
		if (flags & 1) {
			free(_this);
		}
	}
	return _this;
}

struct __rtti_data {
	__type_info type_info = {
		.vtable = &type_info_vtable,
		.__undecorated_name = NULL,
	};

	RTTIBaseClassDescriptor base[1] = {
		{
			-1,
			0,
			{0, -1, 0},
			64,
			0,
		},
	};

	RTTIBaseClassArray baseArray;

	RTTIClassHierarchyDescriptor hierarchy = {
		0,
		0,
		0,
	};

	void init(UINT_PTR moduleBase, RTTICompleteObjectLocator *locator, const char *mangled_name) {
		base[0].rvaTypeDescriptor = reinterpret_cast<UINT_PTR>(&type_info) - moduleBase;

		baseArray.rvaBases[0] = reinterpret_cast<UINT_PTR>(&base) - moduleBase;

		hierarchy.pBaseClassArray = reinterpret_cast<UINT_PTR>(&baseArray) - moduleBase;

		locator->rvaTypeDescriptor = reinterpret_cast<UINT_PTR>(&type_info) - moduleBase;
		locator->rvaClassDescriptor = reinterpret_cast<UINT_PTR>(&hierarchy) - moduleBase;
		locator->rvaSelf = reinterpret_cast<UINT_PTR>(locator) - moduleBase;
	}
};


__SPRT_C_FUNC RTTICompleteObjectLocator type_info_rtti = {
	.signature = 1,
	.classOffset = 0,
	.cdOffset = 0,
	.rvaTypeDescriptor = -1,
	.rvaClassDescriptor = -1,
	.rvaSelf = -1,
};

#define __ASM_VTABLE(name, funcs) \
    __asm__(".data\n" \
            "\t.balign 8\n" \
            "\t.quad " #name "_rtti" "\n" \
            __ASM_GLOBL(#name "_vtable") "\n" \
            funcs "\n\t.text")

#define __ASM_GLOBL(name) ".globl " name "\n" name ":"
*/
//#define VTABLE_ADD_FUNC(name) "\t.quad " #name "\n"

//__ASM_VTABLE(type_info, VTABLE_ADD_FUNC(type_info_vector_dtor));
