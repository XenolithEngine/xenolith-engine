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
#include <string.h>

#include <sprt/cxx/typeinfo>
#include <sprt/cxx/new>
#include <sprt/cxx/detail/hash.h>
#include <sprt/wrappers/windows/dl_api.h>

#include "Demangle/Demangle.h"

using vtable_fn = void (*)(void);

struct TypeDescriptor {
	const TypeDescriptor *type_info_vtable;
	void *spare;
	char __decorated_name[128];
};

// From rttidata.h

#define CHD_MULTINH					0x0000'0001
#define CHD_VIRTINH					0x0000'0002
#define CHD_AMBIGUOUS				0x0000'0004

#define BCD_NOTVISIBLE				0x0000'0001
#define BCD_AMBIGUOUS				0x0000'0002
#define BCD_PRIVORPROTBASE			0x0000'0004
#define BCD_PRIVORPROTINCOMPOBJ		0x0000'0008
#define BCD_VBOFCONTOBJ				0x0000'0010
#define BCD_NONPOLYMORPHIC			0x0000'0020
#define BCD_HASPCHD					0x0000'0040

struct RTTICompleteObjectLocator {
	unsigned long signature;
	unsigned long classOffset;
	unsigned long cdOffset;
	int rvaTypeDescriptor; // Image relative offset of TypeDescriptor
	int rvaClassDescriptor; // Image relative offset of _RTTIClassHierarchyDescriptor
	int rvaSelf; // Image relative offset of this object
};

struct PMD {
	int mdisp; // Offset of intended data within base
	int pdisp; // Displacement to virtual base pointer
	int vdisp; // Index within vbTable to offset of base
};

struct RTTIBaseClassDescriptor {
	int rvaTypeDescriptor; // Image relative offset of TypeDescriptor
	unsigned long numContainedBases;
	PMD where;
	unsigned long attributes;
	int rvaClassDescriptor; // Image relative offset of RTTIClassHierarchyDescriptor
};

struct RTTIBaseClassArray {
	int rvaBases[];
};

struct RTTIClassHierarchyDescriptor {
	unsigned long signature;
	unsigned long attributes;
	unsigned long numBaseClasses;
	int rvaBaseClassArray;
};

static auto getObjectLocator(void *obj) SPRT_NOEXCEPT {
	return reinterpret_cast<const RTTICompleteObjectLocator *>((*(const vtable_fn **)obj)[-1]);
}

static UINT_PTR getBaseOffset(const RTTICompleteObjectLocator *locator) SPRT_NOEXCEPT {
	UINT_PTR base = 0;

	// It's possible, that locator is empty, in this case we need general lookup from OS
	if (!locator->signature) {
		void *module = nullptr;
		if (RtlPcToFileHeader((void *)locator, &module)) {
			base = reinterpret_cast<UINT_PTR>(module);
		} else {
			fprintf(stderr, "Fatal: %s; aborting;\n", "No module for typeid found");
			return 0;
		}
	} else {
		base = reinterpret_cast<UINT_PTR>(locator) - locator->rvaSelf;
	}
	return base;
}

static intptr_t getPointerToMemberOffset(void *obj, const PMD &pmd) {
	if (pmd.pdisp < 0) {
		return pmd.mdisp;
	}

	auto voff = *reinterpret_cast<intptr_t *>(reinterpret_cast<intptr_t>(obj) + pmd.pdisp);
	auto vdisp = *reinterpret_cast<int32_t *>(voff + pmd.vdisp);

	return pmd.mdisp + pmd.pdisp + vdisp;
}

template <typename T>
static auto getFromRVA(UINT_PTR base, intptr_t offset) {
	return reinterpret_cast<T *>(base + offset);
}

static void *getRealObject(const RTTICompleteObjectLocator *loc, intptr_t obj) {
	// Read class layout to find real object pointer and locator
	return reinterpret_cast<void *>( //
			obj - loc->classOffset // original vtable location
			- (loc->cdOffset // dynamic pispatch offset location
							? // Multiple - read base class offset from cdOffset
							*reinterpret_cast< int32_t *>(obj - loc->cdOffset)
							: 0 // Single inheritance - no base class dispatch offset
					));
}

struct RTTIDynamicCastInfo {
	UINT_PTR base;
	void *object;
	const RTTICompleteObjectLocator *locator;
	const RTTIClassHierarchyDescriptor *hierarchy;
	const TypeDescriptor *input;
	const TypeDescriptor *source;
	const TypeDescriptor *target;

	// For RTTI to work across DLL boundaries, we need a string comparison, which is slow;
	// If we can limit ourselves to local lookup - we can omit it;
	//
	// true if we should not lookup types by names,
	bool localLookup = true;

	bool isEqual(const TypeDescriptor *l, const TypeDescriptor *r) const {
		if (l == r) {
			return true;
		} else if (!localLookup) {
			return strcmp(l->__decorated_name, r->__decorated_name) == 0;
		}
		return false;
	}

	bool isEqual(const RTTIBaseClassDescriptor *l, const TypeDescriptor *r) const {
		auto td = getFromRVA<TypeDescriptor>(base, l->rvaTypeDescriptor);
		if (td == r) {
			return true;
		} else if (!localLookup) {
			return strcmp(td->__decorated_name, r->__decorated_name) == 0;
		}
		return false;
	}

	// Look for an attributes in target's class array
	// To construct subIndex, use the rule:
	// parent's base class array is a subarray in derived at
	// (parent_index, parent_index + parent_numContainedBases];
	unsigned long getAttributesForTarget(RTTIBaseClassDescriptor *cd,
			unsigned long subIndex) const {
		auto h = getFromRVA<RTTIClassHierarchyDescriptor>(base, cd->rvaClassDescriptor);
		auto arr = getFromRVA<RTTIBaseClassArray>(base, h->rvaBaseClassArray);
		return getFromRVA<RTTIBaseClassDescriptor>(base, arr->rvaBases[subIndex])->attributes;
	}

	// Using RTTIBaseClassDescriptor attributes,
	// checks, if (Target *)source cast can be performed for input type;
	// source and target should be visible from input, and target should not be ambiguous
	bool canCast(RTTIBaseClassDescriptor *source, RTTIBaseClassDescriptor *target) const {
		if (!source || !target) {
			return false;
		}

		if ((source->attributes & BCD_NOTVISIBLE) != 0
				|| (target->attributes & (BCD_NOTVISIBLE | BCD_AMBIGUOUS)) != 0) {
			return false;
		} else {
			return true;
		}
	}

	template <bool HasMultipleInheritance, bool HasVirtualInheritance>
	RTTIBaseClassDescriptor *doLocateTarget(intptr_t VfDelta) {
		auto baseClasses = getFromRVA<RTTIBaseClassArray>(base, hierarchy->rvaBaseClassArray);

		// If some source is not available within target's hierarchy - downcast is not possible;
		// We preserve this during target's hierarchy traversal
		bool canDowncast = true;

		RTTIBaseClassDescriptor *targetCD = nullptr;
		RTTIBaseClassDescriptor *sourceCD = nullptr;
		RTTIBaseClassDescriptor *downcastTargetCD = nullptr; // downcast target, for all
		RTTIBaseClassDescriptor *crosscastTargetCD = nullptr; // cross-cast target, VI only

		// current target index to locate it's hierarchy array bounds
		unsigned long targetIndex = sprt::Max<unsigned long>;

		// Offset from object to target during downcast lookup
		intptr_t downcastOffset = -1;

		for (unsigned long idx = 0; idx < hierarchy->numBaseClasses; ++idx) {
			auto cd = getFromRVA<RTTIBaseClassDescriptor>(base, baseClasses->rvaBases[idx]);
			auto td = getFromRVA<TypeDescriptor>(base, cd->rvaTypeDescriptor);

			if constexpr (!HasMultipleInheritance) {
				// For SI, all possible sources will be placed after possible targets;
				// So, at first - find a target, then - find a source
				if (!targetCD) {
					if (isEqual(td, target)) {
						targetCD = cd;
						targetIndex = idx;
						continue;
					}
				} else if (isEqual(td, source)) {
					// source was found, exiting
					downcastTargetCD =
							((cd->attributes & BCD_PRIVORPROTBASE) != 0) ? nullptr : targetCD;
					break;
				}
			} else {
				// Check if cd is target (excluding previously found target's bases)
				if ((!targetCD || (idx > targetIndex + targetCD->numContainedBases))
						&& isEqual(td, target)) {

					// source was found outside of target's hierarchy, check for a cross-cast;
					// For VI - this will be processed after the base classes are completely
					// traversed so that potential base class paths are not lost
					if constexpr (HasVirtualInheritance) {
						if ((cd->attributes & (BCD_NOTVISIBLE | BCD_AMBIGUOUS)) == 0) {
							crosscastTargetCD = cd;
						}
					} else if (sourceCD) {
						// handle cross-cast in place for MI
						return canCast(sourceCD, cd) ? cd : nullptr;
					}

					canDowncast = true; // downcast for this target was not checked yet
					targetCD = cd;
					targetIndex = idx;
				}

				// check if cd is source
				if (isEqual(td, source) && getPointerToMemberOffset(object, cd->where) == VfDelta) {
					if (targetCD) {
						if (idx <= targetIndex + targetCD->numContainedBases) {
							if (canDowncast) {
								auto attr = getAttributesForTarget(targetCD, idx - targetIndex);
								if ((attr & (BCD_NOTVISIBLE | BCD_PRIVORPROTBASE)) == 0) {
									// We found an acessible source;
									// Now, for VI, check, if with offset, if it points to same target;
									// If not, it's ambiguous case, aborting
									if constexpr (HasVirtualInheritance) {
										auto offsetTarget =
												getPointerToMemberOffset(object, targetCD->where);
										if (downcastTargetCD != nullptr
												&& downcastOffset != offsetTarget) {
											return nullptr;
										}
										downcastOffset = offsetTarget;
									}

									downcastTargetCD = targetCD;
								} else {
									// the source is not accessible; that means, any source within target's chain
									// can not be The source;
									canDowncast = false;
								}

								// For non-virtuals, it's an accessible source, or ambiguous source,
								// In both cases, we finish traversal
								if constexpr (!HasVirtualInheritance) {
									break;
								}
							}
						} else if constexpr (!HasVirtualInheritance) {
							// handle cross-cast in place for MI
							return canCast(cd, targetCD) ? targetCD : nullptr;
						}
					}

					if ((cd->attributes & (BCD_NOTVISIBLE | BCD_PRIVORPROTBASE)) == 0) {
						sourceCD = cd;
					}
				}
			}
		}

		if (canDowncast && downcastTargetCD != nullptr) {
			return downcastTargetCD;
		} else if (sourceCD != nullptr && crosscastTargetCD != nullptr) {
			return crosscastTargetCD;
		}

		return nullptr;
	}

	RTTIBaseClassDescriptor *locateTarget(intptr_t VfDelta) {
		if ((hierarchy->attributes & CHD_MULTINH) == 0) {
			// single virtual and non-virtual
			return doLocateTarget<false, false>(VfDelta);
		} else if ((hierarchy->attributes & CHD_VIRTINH) == 0) {
			// multiple non-virtual
			return doLocateTarget<true, false>(VfDelta);
		} else {
			// fill-virtual
			return doLocateTarget<true, true>(VfDelta);
		}
		return nullptr;
	}
};

// See llvm-project/clang/lib/CodeGen/MicrosoftCXXABI.cpp for usage pattern of this
__SPRT_C_FUNC const TypeDescriptor s_type_info_vftable __asm__("??_7type_info@@6B@") = {
	&s_type_info_vftable,
	nullptr,
	".?AVtype_info@@",
};

__SPRT_C_FUNC const type_info *__RTtypeid(void *obj) {
	if (!obj) {
		fprintf(stderr, "Fatal: %s; aborting;\n", "typeid(nullptr) called");
		abort();
	}

	auto locator = getObjectLocator(obj);

	// Now we know the absolute address of locator; with rvaSelf field, we can find
	// an address for base module of the object

	UINT_PTR base = getBaseOffset(locator);
	if (!base) {
		// We have not exception, so, this func can not fail;
		// In case of fail - return hierarchy root, this should be valid and graceful fallback
		return (const type_info *)&s_type_info_vftable;
	}

	return (const type_info *)(base + locator->rvaTypeDescriptor);
}

__SPRT_C_FUNC PVOID __RTCastToVoid(PVOID inptr) SPRT_NOEXCEPT {
	return getRealObject(getObjectLocator(inptr), reinterpret_cast<intptr_t>(inptr));
}

__SPRT_C_FUNC PVOID __RTDynamicCast(PVOID inptr, LONG VfDelta, PVOID SrcType, PVOID TargetType,
		BOOL isReference) SPRT_NOEXCEPT {
	if (!inptr || !SrcType || !TargetType) {
		return nullptr;
	}

	RTTIDynamicCastInfo info{
		.object = inptr,
		.locator = getObjectLocator(inptr),
		.source = static_cast<TypeDescriptor *>(SrcType),
		.target = static_cast<TypeDescriptor *>(TargetType),
	};

	//printf("Source: %s\n", info.source->__decorated_name);
	//printf("Target: %s\n", info.target->__decorated_name);

	info.object = getRealObject(info.locator, reinterpret_cast<intptr_t>(inptr));
	info.base = getBaseOffset(info.locator);
	if (!info.base) {
		return nullptr;
	}

	info.hierarchy =
			getFromRVA<RTTIClassHierarchyDescriptor>(info.base, info.locator->rvaClassDescriptor);
	info.input = getFromRVA<TypeDescriptor>(info.base, info.locator->rvaTypeDescriptor);

	//printf("Input: %s\n", info.input->__decorated_name);

	// merge VfDelta with offset to base class
	const intptr_t ptmOffset =
			reinterpret_cast<intptr_t>(inptr) - reinterpret_cast<intptr_t>(info.object) - VfDelta;

	RTTIBaseClassDescriptor *targetClassDescriptor = info.locateTarget(ptmOffset);

	if (!targetClassDescriptor) {
		return nullptr;
	}

	return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(info.object)
			+ getPointerToMemberOffset(info.object, targetClassDescriptor->where));
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

/*
	We do not implement Microsoft's standard automatic demangling of class names;
	Instead, we provide a function from the Itanium ABI, implemented on top of
	the demangler from LLVM
*/
extern "C" char *__cxa_demangle(const char *mangled_name, char *output_buffer,
		__SPRT_ID(size_t) * length, int *status) {
	return llvm::microsoftDemangle(mangled_name, length, status, llvm::MSDF_None);
}

} // namespace abi
