//===------------------------- MicrosoftDemangle.h --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEMANGLE_MICROSOFTDEMANGLE_H
#define LLVM_DEMANGLE_MICROSOFTDEMANGLE_H

#include "Demangle.h"
#include "MicrosoftDemangleNodes.h"

namespace llvm {
namespace ms_demangle {
// This memory allocator is extremely fast, but it doesn't call dtors
// for allocated objects. That means you can't use STL containers
// (such as std::vector) with this allocator. But it pays off --
// the demangler is 3x faster with this allocator compared to one with
// STL containers.
constexpr size_t AllocUnit = 4'096;

class ArenaAllocator {
	struct AllocatorNode {
		sprt::uint8_t *Buf = nullptr;
		size_t Used = 0;
		size_t Capacity = 0;
		AllocatorNode *Next = nullptr;
	};

	void addNode(size_t Capacity) {
		AllocatorNode *NewHead = new (sprt::nothrow) AllocatorNode;
		NewHead->Buf = new (sprt::nothrow) sprt::uint8_t[Capacity];
		NewHead->Next = Head;
		NewHead->Capacity = Capacity;
		Head = NewHead;
		NewHead->Used = 0;
	}

public:
	ArenaAllocator() { addNode(AllocUnit); }

	~ArenaAllocator() {
		while (Head) {
			assert(Head->Buf);
			sprt::__delete_n(Head->Buf, Head->Capacity);
			AllocatorNode *Next = Head->Next;
			sprt::__delete(Head);
			Head = Next;
		}
	}

	// Delete the copy constructor and the copy assignment operator.
	ArenaAllocator(const ArenaAllocator &) = delete;
	ArenaAllocator &operator=(const ArenaAllocator &) = delete;

	char *allocUnalignedBuffer(size_t Size) {
		assert(Head && Head->Buf);

		sprt::uint8_t *P = Head->Buf + Head->Used;

		Head->Used += Size;
		if (Head->Used <= Head->Capacity) {
			return reinterpret_cast<char *>(P);
		}

		addNode(sprt::max(AllocUnit, Size));
		Head->Used = Size;
		return reinterpret_cast<char *>(Head->Buf);
	}

	template <typename T, typename... Args>
	T *allocArray(size_t Count) {
		size_t Size = Count * sizeof(T);
		assert(Head && Head->Buf);

		size_t P = (size_t)Head->Buf + Head->Used;
		sprt::uintptr_t AlignedP = (((size_t)P + alignof(T) - 1) & ~(size_t)(alignof(T) - 1));
		sprt::uint8_t *PP = (sprt::uint8_t *)AlignedP;
		size_t Adjustment = AlignedP - P;

		Head->Used += Size + Adjustment;
		if (Head->Used <= Head->Capacity) {
			return new (PP) T[Count]();
		}

		addNode(sprt::max(AllocUnit, Size));
		Head->Used = Size;
		return new (Head->Buf) T[Count]();
	}

	template <typename T, typename... Args>
	T *alloc(Args &&...ConstructorArgs) {
		constexpr size_t Size = sizeof(T);
		assert(Head && Head->Buf);

		size_t P = (size_t)Head->Buf + Head->Used;
		sprt::uintptr_t AlignedP = (((size_t)P + alignof(T) - 1) & ~(size_t)(alignof(T) - 1));
		sprt::uint8_t *PP = (sprt::uint8_t *)AlignedP;
		size_t Adjustment = AlignedP - P;

		Head->Used += Size + Adjustment;
		if (Head->Used <= Head->Capacity) {
			return new (PP) T(sprt::forward<Args>(ConstructorArgs)...);
		}

		static_assert(Size < AllocUnit);
		addNode(AllocUnit);
		Head->Used = Size;
		return new (Head->Buf) T(sprt::forward<Args>(ConstructorArgs)...);
	}

private:
	AllocatorNode *Head = nullptr;
};

struct BackrefContext {
	static constexpr size_t Max = 10;

	TypeNode *FunctionParams[Max];
	size_t FunctionParamCount = 0;

	// The first 10 BackReferences in a mangled name can be back-referenced by
	// special name @[0-9]. This is a storage for the first 10 BackReferences.
	NamedIdentifierNode *Names[Max];
	size_t NamesCount = 0;
};

enum class QualifierMangleMode {
	Drop,
	Mangle,
	Result
};

enum NameBackrefBehavior : sprt::uint8_t {
	NBB_None = 0, // don't save any names as backrefs.
	NBB_Template = 1 << 0, // save template instanations.
	NBB_Simple = 1 << 1, // save simple names.
};

enum class FunctionIdentifierCodeGroup {
	Basic,
	Under,
	DoubleUnder
};

// Demangler class takes the main role in demangling symbols.
// It has a set of functions to parse mangled symbols into Type instances.
// It also has a set of functions to convert Type instances to strings.
class Demangler {
	friend sprt::optional<size_t> llvm::getArm64ECInsertionPointInMangledName(
			sprt::StringView MangledName);

public:
	Demangler() = default;
	__SPRT_PUSH_ALLOW_CXXABI_ALLOC
	virtual ~Demangler() = default;
	__SPRT_POP_ALLOW_CXXABI_ALLOC

	// You are supposed to call parse() first and then check if error is true.  If
	// it is false, call output() to write the formatted name to the given stream.
	SymbolNode *parse(sprt::StringView &MangledName);

	TagTypeNode *parseTagUniqueName(sprt::StringView &MangledName);

	// True if an error occurred.
	bool Error = false;

	void dumpBackReferences();

private:
	SymbolNode *demangleEncodedSymbol(sprt::StringView &MangledName, QualifiedNameNode *QN);
	SymbolNode *demangleDeclarator(sprt::StringView &MangledName);
	SymbolNode *demangleMD5Name(sprt::StringView &MangledName);
	SymbolNode *demangleTypeinfoName(sprt::StringView &MangledName);

	VariableSymbolNode *demangleVariableEncoding(sprt::StringView &MangledName, StorageClass SC);
	FunctionSymbolNode *demangleFunctionEncoding(sprt::StringView &MangledName);

	Qualifiers demanglePointerExtQualifiers(sprt::StringView &MangledName);

	bool isMemberPointer(sprt::StringView MangledName, bool &Error);

	sprt::optional<PointerAuthQualifierNode::ArgArray> demanglePointerAuthQualifier(
			sprt::StringView &MangledName);

	PointerAuthQualifierNode *createPointerAuthQualifier(sprt::StringView &MangledName);

	// Parser functions. This is a recursive-descent parser.
	TypeNode *demangleType(sprt::StringView &MangledName, QualifierMangleMode QMM);
	PrimitiveTypeNode *demanglePrimitiveType(sprt::StringView &MangledName);
	CustomTypeNode *demangleCustomType(sprt::StringView &MangledName);
	TagTypeNode *demangleClassType(sprt::StringView &MangledName);
	PointerTypeNode *demanglePointerType(sprt::StringView &MangledName);
	PointerTypeNode *demangleMemberPointerType(sprt::StringView &MangledName);
	FunctionSignatureNode *demangleFunctionType(sprt::StringView &MangledName, bool HasThisQuals);

	ArrayTypeNode *demangleArrayType(sprt::StringView &MangledName);

	NodeArrayNode *demangleFunctionParameterList(sprt::StringView &MangledName, bool &IsVariadic);
	NodeArrayNode *demangleTemplateParameterList(sprt::StringView &MangledName);

	sprt::pair<sprt::uint64_t, bool> demangleNumber(sprt::StringView &MangledName);
	sprt::uint64_t demangleUnsigned(sprt::StringView &MangledName);
	sprt::int64_t demangleSigned(sprt::StringView &MangledName);

	void memorizeString(sprt::StringView s);
	void memorizeIdentifier(IdentifierNode *Identifier);

	/// Allocate a copy of \p Borrowed into memory that we own.
	sprt::StringView copyString(sprt::StringView Borrowed);

	QualifiedNameNode *demangleFullyQualifiedTypeName(sprt::StringView &MangledName);
	QualifiedNameNode *demangleFullyQualifiedSymbolName(sprt::StringView &MangledName);

	IdentifierNode *demangleUnqualifiedTypeName(sprt::StringView &MangledName, bool Memorize);
	IdentifierNode *demangleUnqualifiedSymbolName(sprt::StringView &MangledName,
			NameBackrefBehavior NBB);

	QualifiedNameNode *demangleNameScopeChain(sprt::StringView &MangledName,
			IdentifierNode *UnqualifiedName);
	IdentifierNode *demangleNameScopePiece(sprt::StringView &MangledName);

	NamedIdentifierNode *demangleBackRefName(sprt::StringView &MangledName);
	IdentifierNode *demangleTemplateInstantiationName(sprt::StringView &MangledName,
			NameBackrefBehavior NBB);
	IntrinsicFunctionKind translateIntrinsicFunctionCode(char CH,
			FunctionIdentifierCodeGroup Group);
	IdentifierNode *demangleFunctionIdentifierCode(sprt::StringView &MangledName);
	IdentifierNode *demangleFunctionIdentifierCode(sprt::StringView &MangledName,
			FunctionIdentifierCodeGroup Group);
	StructorIdentifierNode *demangleStructorIdentifier(sprt::StringView &MangledName,
			bool IsDestructor);
	ConversionOperatorIdentifierNode *demangleConversionOperatorIdentifier(
			sprt::StringView &MangledName);
	LiteralOperatorIdentifierNode *demangleLiteralOperatorIdentifier(sprt::StringView &MangledName);

	SymbolNode *demangleSpecialIntrinsic(sprt::StringView &MangledName);
	SpecialTableSymbolNode *demangleSpecialTableSymbolNode(sprt::StringView &MangledName,
			SpecialIntrinsicKind SIK);
	LocalStaticGuardVariableNode *demangleLocalStaticGuard(sprt::StringView &MangledName,
			bool IsThread);
	VariableSymbolNode *demangleUntypedVariable(ArenaAllocator &Arena,
			sprt::StringView &MangledName, sprt::StringView VariableName);
	VariableSymbolNode *demangleRttiBaseClassDescriptorNode(ArenaAllocator &Arena,
			sprt::StringView &MangledName);
	FunctionSymbolNode *demangleInitFiniStub(sprt::StringView &MangledName, bool IsDestructor);

	NamedIdentifierNode *demangleSimpleName(sprt::StringView &MangledName, bool Memorize);
	NamedIdentifierNode *demangleAnonymousNamespaceName(sprt::StringView &MangledName);
	NamedIdentifierNode *demangleLocallyScopedNamePiece(sprt::StringView &MangledName);
	EncodedStringLiteralNode *demangleStringLiteral(sprt::StringView &MangledName);
	FunctionSymbolNode *demangleVcallThunkNode(sprt::StringView &MangledName);

	sprt::StringView demangleSimpleString(sprt::StringView &MangledName, bool Memorize);

	FuncClass demangleFunctionClass(sprt::StringView &MangledName);
	CallingConv demangleCallingConvention(sprt::StringView &MangledName);
	StorageClass demangleVariableStorageClass(sprt::StringView &MangledName);
	bool demangleThrowSpecification(sprt::StringView &MangledName);
	wchar_t demangleWcharLiteral(sprt::StringView &MangledName);
	sprt::uint8_t demangleCharLiteral(sprt::StringView &MangledName);

	sprt::pair<Qualifiers, bool> demangleQualifiers(sprt::StringView &MangledName);

	// Memory allocator.
	ArenaAllocator Arena;

	// A single type uses one global back-ref table for all function params.
	// This means back-refs can even go "into" other types.  Examples:
	//
	//  // Second int* is a back-ref to first.
	//  void foo(int *, int*);
	//
	//  // Second int* is not a back-ref to first (first is not a function param).
	//  int* foo(int*);
	//
	//  // Second int* is a back-ref to first (ALL function types share the same
	//  // back-ref map.
	//  using F = void(*)(int*);
	//  F G(int *);
	BackrefContext Backrefs;
};

} // namespace ms_demangle
} // namespace llvm

#endif // LLVM_DEMANGLE_MICROSOFTDEMANGLE_H
