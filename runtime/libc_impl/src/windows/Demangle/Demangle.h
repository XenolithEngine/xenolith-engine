//===--- Demangle.h ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEMANGLE_DEMANGLE_H
#define LLVM_DEMANGLE_DEMANGLE_H

#include <sprt/runtime/stringview.h>
#include <sprt/cxx/optional>

namespace llvm {
/// This is a llvm local version of __cxa_demangle. Other than the name and
/// being in the llvm namespace it is identical.
///
/// The mangled_name is demangled into buf and returned. If the buffer is not
/// large enough, realloc is used to expand it.
///
/// The *status will be set to a value from the following enumeration
enum : int {
	demangle_unknown_error = -4,
	demangle_invalid_args = -3,
	demangle_invalid_mangled_name = -2,
	demangle_memory_alloc_failure = -1,
	demangle_success = 0,
};

enum MSDemangleFlags {
	MSDF_None = 0,
	MSDF_DumpBackrefs = 1 << 0,
	MSDF_NoAccessSpecifier = 1 << 1,
	MSDF_NoCallingConvention = 1 << 2,
	MSDF_NoReturnType = 1 << 3,
	MSDF_NoMemberType = 1 << 4,
	MSDF_NoVariableType = 1 << 5,
};

/// Demangles the Microsoft symbol pointed at by mangled_name and returns it.
/// Returns a pointer to the start of a null-terminated demangled string on
/// success, or nullptr on error.
/// If n_read is non-null and demangling was successful, it receives how many
/// bytes of the input string were consumed.
/// status receives one of the demangle_ enum entries above if it's not nullptr.
/// Flags controls various details of the demangled representation.
char *microsoftDemangle(sprt::StringView mangled_name, size_t *n_read, int *status,
		MSDemangleFlags Flags = MSDF_None);

sprt::optional<size_t> getArm64ECInsertionPointInMangledName(sprt::StringView MangledName);

} // namespace llvm

#endif
