/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * **/

#include <sprt/runtime/stream.h>
#include <sprt/cxx/cstdint>

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

/*
	Tests for custom RTTI implementation
*/

namespace sprt {

// ============================================================================
// Single inheritance hierarchy for dynamic_cast testing
// ============================================================================

/** Base class with virtual destructor (required for RTTI and safe downcasting) */
class Base {
public:
	int baseValue;

	Base(int v = 0) : baseValue(v) { }
	virtual ~Base() { }

	virtual void baseMethod() { }
};

/** Derived class - single inheritance from Base */
class Derived : public Base {
public:
	int derivedValue;

	Derived(int b = 0, int d = 0) : Base(b), derivedValue(d) { }
	virtual ~Derived() { }

	void derivedMethod() { }
};

/** MoreDerived class - extends Derived (multi-level inheritance) */
class MoreDerived : public Derived {
public:
	int moreDerivedValue;

	MoreDerived(int b = 0, int d = 0, int m = 0) : Derived(b, d), moreDerivedValue(m) { }
	virtual ~MoreDerived() { }

	void moreDerivedMethod() { }

	Base *getBase() { return static_cast<Base *>(this); }
};

class MoreDerivedBase : private Base, public MoreDerived {
public:
	int moreDerivedValue;

	MoreDerivedBase(int b = 0, int d = 0, int m = 0) : Base(10), MoreDerived(b, d, m) { }
	virtual ~MoreDerivedBase() { }

	void moreDerivedMethod() { }
};

// ============================================================================
// Multiple Inheritance hierarchy for RTTI testing
// ============================================================================

/** Interface A - pure virtual base class for multiple inheritance testing */
class InterfaceA {
public:
	int interfaceAValue;

	InterfaceA(int v = 0) : interfaceAValue(v) { }
	virtual ~InterfaceA() { }
	virtual void interfaceAMethod() = 0;
};

/** Interface B - pure virtual base class for multiple inheritance testing */
class InterfaceB {
public:
	int interfaceBValue;

	InterfaceB(int v = 0) : interfaceBValue(v) { }
	virtual ~InterfaceB() { }
	virtual void interfaceBMethod() = 0;
};

/** Class with multiple inheritance from both InterfaceA and InterfaceB */
class MultipleInheritClass : public InterfaceA, public InterfaceB {
public:
	int multiValue;

	MultipleInheritClass(int a = 0, int b = 0, int m = 0)
	: InterfaceA(a), InterfaceB(b), multiValue(m) { }
	virtual ~MultipleInheritClass() { }

	void interfaceAMethod() override { }
	void interfaceBMethod() override { }
};

// ============================================================================
// Virtual Inheritance (Diamond Pattern) hierarchy for RTTI testing
// ============================================================================

/** Top of diamond - shared base with virtual inheritance */
class VirtualBase {
public:
	int virtualBaseValue;

	VirtualBase(int v = 0) : virtualBaseValue(v) { }
	virtual ~VirtualBase() { }
	virtual void virtualBaseMethod() { }
};

/** Left branch of diamond - virtual inheritance from VirtualBase */
class VirtualLeft : public virtual VirtualBase {
public:
	int leftValue;

	VirtualLeft(int v = 0, int l = 0) : VirtualBase(v), leftValue(l) { }
	virtual ~VirtualLeft() { }
};

/** Right branch of diamond - virtual inheritance from VirtualBase */
class VirtualRight : public virtual VirtualBase {
public:
	int rightValue;

	VirtualRight(int v = 0, int r = 0) : VirtualBase(v), rightValue(r) { }
	virtual ~VirtualRight() { }
};

/** Bottom of diamond - inherits from both VirtualLeft and VirtualRight */
class DiamondChild : public VirtualLeft, public VirtualRight {
public:
	int diamondValue;

	DiamondChild(int v = 0, int l = 0, int r = 0, int d = 0)
	: VirtualBase(v), VirtualLeft(v, l), VirtualRight(v, r), diamondValue(d) { }
	virtual ~DiamondChild() { }
};

// ============================================================================
// Test helper macros
// ============================================================================

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        sprt::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " << msg << "\n"; \
        failures++; \
    } else { \
        passes++; \
    } \
} while(0)

#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)

// ============================================================================
// Test: Upcast with dynamic_cast (pointer form)
// ============================================================================
static void testDynamicCastUpcastPointer() {
	sprt::cout << "\n--- Testing dynamic_cast upcast (pointer form) ---\n";

	// Create objects at different levels of inheritance
	MoreDerived md(1, 2, 3);
	Derived d(4, 5);

	int passes = 0;
	int failures = 0;

	// Test 1: Upcast from MoreDerived* to Base* using dynamic_cast
	void *voidPtr = static_cast<void *>(&md);
	auto mdCast = static_cast<MoreDerived *>(voidPtr);
	auto baseFromMd = dynamic_cast<Base *>(mdCast);
	ASSERT_TRUE(baseFromMd == &md, "dynamic_cast MoreDerived* to Base* should succeed");
	ASSERT_TRUE(baseFromMd->baseValue == 1, "Upcasted pointer should access correct base value");

	// Test 2: Upcast from Derived* to Base* using dynamic_cast
	auto dCast = static_cast<Derived *>(voidPtr);
	(void)dCast;
	auto baseFromD = dynamic_cast<Base *>(&d);
	ASSERT_TRUE(baseFromD == &d, "dynamic_cast Derived* to Base* should succeed");
	ASSERT_TRUE(baseFromD->baseValue == 4, "Upcasted pointer should access correct base value");

	// Test 3: Upcast from MoreDerived* to Derived* using dynamic_cast
	auto derivedFromMd = dynamic_cast<Derived *>(mdCast);
	ASSERT_TRUE(derivedFromMd == &md, "dynamic_cast MoreDerived* to Derived* should succeed");
	ASSERT_TRUE(derivedFromMd->derivedValue == 2,
			"Upcasted pointer should access correct derived value");

	sprt::cout << "Upcast (pointer): " << passes << "/" << (passes + failures) << " passed\n";
}

// ============================================================================
// Test: Downcast with dynamic_cast - successful cases (pointer form)
// ============================================================================
static void testDynamicCastDowncastPointerSuccess() {
	sprt::cout << "\n--- Testing dynamic_cast downcast success (pointer form) ---\n";

	int passes = 0;
	int failures = 0;

	// Test 1: Downcast from Base* to Derived* - direct parent
	Base baseObj(100);
	auto basePtr = &baseObj;
	auto derivedPtr = dynamic_cast<Derived *>(basePtr);
	ASSERT_TRUE(derivedPtr == nullptr, "Downcast to non-derived type should return nullptr");

	// Test 2: Downcast from Base* to MoreDerived* - grandchild
	auto moreDerivedPtr = dynamic_cast<MoreDerived *>(basePtr);
	ASSERT_TRUE(moreDerivedPtr == nullptr, "Downcast to non-derived type should return nullptr");

	// Test 3: Successful downcast from Base* to Derived* with actual Derived object
	Derived derivedObj(100, 200);
	auto baseOfDerived = static_cast<Base *>(&derivedObj);
	auto downcastToDerived = dynamic_cast<Derived *>(baseOfDerived);
	ASSERT_TRUE(downcastToDerived == &derivedObj, "Downcast to actual derived type should succeed");
	ASSERT_TRUE(downcastToDerived->derivedValue == 200,
			"Downcasted pointer should access correct derived value");

	// Test 4: Successful downcast from Base* to MoreDerived* with actual MoreDerived object
	MoreDerived moreDerivedObj(100, 200, 300);
	auto baseOfMoreDerived = static_cast<Base *>(&moreDerivedObj);
	auto downcastToMoreDerived = dynamic_cast<MoreDerived *>(baseOfMoreDerived);
	ASSERT_TRUE(downcastToMoreDerived == &moreDerivedObj,
			"Downcast to actual more-derived type should succeed");
	ASSERT_TRUE(downcastToMoreDerived->moreDerivedValue == 300,
			"Downcasted pointer should access correct more-derived value");

	// Test 5: Downcast from Derived* to MoreDerived* - child class
	auto derivedOfMoreDerived = static_cast<Derived *>(&moreDerivedObj);
	auto downcastToChild = dynamic_cast<MoreDerived *>(derivedOfMoreDerived);
	ASSERT_TRUE(downcastToChild == &moreDerivedObj, "Downcast from parent to child should succeed");

	// Test 6: Downcast from Derived* to MoreDerived* - should fail for pure Derived object
	auto downcastFail = dynamic_cast<MoreDerived *>(&derivedObj);
	ASSERT_TRUE(downcastFail == nullptr,
			"Downcast to deeper child should return nullptr when not applicable");

	sprt::cout << "Downcast success (pointer): " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: Downcast with dynamic_cast - failure cases (pointer form)
// ============================================================================
static void testDynamicCastDowncastPointerFailure() {
	sprt::cout << "\n--- Testing dynamic_cast downcast failure (pointer form) ---\n";

	int passes = 0;
	int failures = 0;

	// Test 1: Downcast from null pointer should return nullptr
	Base *nullBase = nullptr;
	auto nullDowncast = dynamic_cast<Derived *>(nullBase);
	ASSERT_TRUE(nullDowncast == nullptr, "Downcast from null pointer should return nullptr");

	// Test 2: Downcast to unrelated type (no inheritance relationship)
	class Unrelated {
	public:
		int unrelatedValue;
		Unrelated(int v = 0) : unrelatedValue(v) { }
	};

	Derived derivedObj(100, 200);
	// Note: This would be a compile-time error in standard C++, so we test with void* cast
	//auto baseOfDerived = static_cast<Base *>(&derivedObj);

	// Test 3: Downcast hierarchy verification - each level must match
	MoreDerived fullHierarchy(1, 2, 3);
	auto basePtr = static_cast<Base *>(&fullHierarchy);
	auto derivedPtr = static_cast<Derived *>(&fullHierarchy);

	ASSERT_TRUE(dynamic_cast<Derived *>(basePtr) != nullptr, "Base* should downcast to Derived*");
	ASSERT_TRUE(dynamic_cast<MoreDerived *>(basePtr) != nullptr,
			"Base* should downcast to MoreDerived*");
	ASSERT_TRUE(dynamic_cast<MoreDerived *>(derivedPtr) != nullptr,
			"Derived* should downcast to MoreDerived*");

	MoreDerivedBase mdb(1, 2, 3);
	auto b = mdb.getBase();
	ASSERT_TRUE(dynamic_cast<Derived *>(b) != nullptr, "Ambiuous base test fails*");

	sprt::cout << "Downcast failure (pointer): " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: dynamic_cast with polymorphic vs non-polymorphic types
// ============================================================================
static void testDynamicCastPolymorphicRequirement() {
	sprt::cout << "\n--- Testing dynamic_cast polymorphic requirement ---\n";

	int passes = 0;
	int failures = 0;

	// All our test classes have virtual destructor, so they are polymorphic
	// This test verifies that dynamic_cast works correctly with polymorphic types

	Base baseObj(42);
	auto basePtr = &baseObj;

	// Verify dynamic_cast works (requires polymorphic type)
	auto castResult = dynamic_cast<Derived *>(basePtr);
	ASSERT_TRUE(castResult == nullptr,
			"dynamic_cast should work on polymorphic types and return nullptr for wrong type");

	// Verify RTTI is available through dynamic_cast
	const std::type_info &baseType = typeid(baseObj);
	const std::type_info &derivedType = typeid(Derived);
	ASSERT_TRUE(baseType != derivedType, "typeid should distinguish between Base and Derived");

	sprt::cout << "Polymorphic requirement: " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: Multiple downcast attempts from same base pointer
// ============================================================================
static void testMultipleDowncastsFromSameBase() {
	sprt::cout << "\n--- Testing multiple downcasts from same base pointer ---\n";

	int passes = 0;
	int failures = 0;

	// Create a MoreDerived object and get Base* to it
	MoreDerived obj(10, 20, 30);
	auto basePtr = static_cast<Base *>(&obj);

	// Attempt multiple different downcasts from the same base pointer
	auto toDerived = dynamic_cast<Derived *>(basePtr);
	auto toMoreDerived = dynamic_cast<MoreDerived *>(basePtr);

	ASSERT_TRUE(toDerived == &obj, "First downcast should succeed");
	ASSERT_TRUE(toMoreDerived == &obj,
			"Second downcast should also succeed and point to same object");
	ASSERT_TRUE(toDerived == toMoreDerived,
			"Both downcasts should point to the same underlying object");

	sprt::cout << "Multiple downcasts: " << passes << "/" << (passes + failures) << " passed\n";
}

// ============================================================================
// Test: Multiple Inheritance - upcast with dynamic_cast (pointer form)
// ============================================================================
static void testMultipleInheritanceUpcast() {
	sprt::cout << "\n--- Testing multiple inheritance upcast (MI upcast) ---\n";

	int passes = 0;
	int failures = 0;

	// Create a MultipleInheritClass object that has both InterfaceA and InterfaceB subobjects
	MultipleInheritClass miObj(10, 20, 30);

	// Test 1: Upcast from MI* to InterfaceA* using dynamic_cast
	auto miToInterfaceA = dynamic_cast<InterfaceA *>(&miObj);
	ASSERT_TRUE(miToInterfaceA == &miObj, "dynamic_cast MI* to InterfaceA* should succeed");
	ASSERT_TRUE(miToInterfaceA->interfaceAValue == 10,
			"Upcasted pointer should access correct InterfaceA value");

	// Test 2: Upcast from MI* to InterfaceB* using dynamic_cast
	auto miToInterfaceB = dynamic_cast<InterfaceB *>(&miObj);
	ASSERT_TRUE(miToInterfaceB == &miObj, "dynamic_cast MI* to InterfaceB* should succeed");
	ASSERT_TRUE(miToInterfaceB->interfaceBValue == 20,
			"Upcasted pointer should access correct InterfaceB value");

	// Test 3: Verify both base pointers point to different subobjects within same object
	// In MI, each base class has its own vtable and subobject offset
	// Cast through void* because comparing distinct pointer types is a compiler error
	ASSERT_TRUE((void *)miToInterfaceA != (void *)miToInterfaceB,
			"MI creates separate subobjects for each base class");

	sprt::cout << "Multiple inheritance upcast: " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: Multiple Inheritance - downcast with dynamic_cast (pointer form)
// ============================================================================
static void testMultipleInheritanceDowncast() {
	sprt::cout << "\n--- Testing multiple inheritance downcast (MI downcast) ---\n";

	int passes = 0;
	int failures = 0;

	// Create MI object and get InterfaceA* to it
	MultipleInheritClass miObj(10, 20, 30);
	auto interfaceAPtr = static_cast<InterfaceA *>(&miObj);

	// Test 1: Downcast from InterfaceA* back to MI* - should succeed
	auto downcastFromA = dynamic_cast<MultipleInheritClass *>(interfaceAPtr);
	ASSERT_TRUE(downcastFromA == &miObj, "Downcast from InterfaceA* to MI* should succeed");

	// Test 2: Get InterfaceB* and downcast back to MI* - should also succeed
	auto interfaceBPtr = static_cast<InterfaceB *>(&miObj);
	auto downcastFromB = dynamic_cast<MultipleInheritClass *>(interfaceBPtr);
	ASSERT_TRUE(downcastFromB == &miObj, "Downcast from InterfaceB* to MI* should succeed");

	// Test 3: Both downcasts should point to same object
	ASSERT_TRUE(downcastFromA == downcastFromB,
			"Both downcasts from different bases should reach same MI object");

	// Test 4: Downcast failure - try to cast InterfaceA* to InterfaceB* directly
	auto interfaceACast = dynamic_cast<InterfaceB *>(interfaceAPtr);
	ASSERT_TRUE(interfaceACast != nullptr, "Direct cast between MI bases should succeed");

	auto interfaceBCast = dynamic_cast<InterfaceA *>(interfaceBPtr);
	ASSERT_TRUE(interfaceBCast != nullptr, "Direct cast between MI bases should succeed");

	sprt::cout << "Multiple inheritance downcast: " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: Multiple Inheritance - cross-cast between base classes (pointer form)
// ============================================================================
static void testMultipleInheritanceCrossCast() {
	sprt::cout << "\n--- Testing multiple inheritance cross-cast (MI cross-cast) ---\n";

	int passes = 0;
	int failures = 0;

	// Create MI object and get InterfaceA* to it
	MultipleInheritClass miObj(10, 20, 30);
	auto interfaceAPtr = static_cast<InterfaceA *>(&miObj);

	// Test 1: Cross-cast from InterfaceA* to InterfaceB* using dynamic_cast
	// This is a special case - both bases share the same most-derived object
	auto crossCastResult = dynamic_cast<InterfaceB *>(interfaceAPtr);
	ASSERT_TRUE(crossCastResult == &miObj, "Cross-cast between MI bases should succeed");
	ASSERT_TRUE(crossCastResult->interfaceBValue == 20,
			"Cross-casted pointer should access correct InterfaceB value");

	// Test 2: Reverse cross-cast from InterfaceB* to InterfaceA*
	auto interfaceBPtr = static_cast<InterfaceB *>(&miObj);
	auto reverseCrossCast = dynamic_cast<InterfaceA *>(interfaceBPtr);
	ASSERT_TRUE(reverseCrossCast == &miObj, "Reverse cross-cast should succeed");
	ASSERT_TRUE(reverseCrossCast->interfaceAValue == 10,
			"Reverse cross-casted pointer should access correct InterfaceA value");

	// Test 3: Cross-cast from MI* to InterfaceB when we have InterfaceA*
	auto miToInterfaceA = dynamic_cast<InterfaceA *>(&miObj);
	auto miCrossCast = dynamic_cast<InterfaceB *>(miToInterfaceA);
	ASSERT_TRUE(miCrossCast == &miObj, "Cross-cast via InterfaceA should reach InterfaceB");

	// Test 4: Cross-cast failure - try to cross-cast from non-MI object
	Base baseObj(999);
	auto failCrossCast = dynamic_cast<InterfaceB *>(static_cast<InterfaceA *>(nullptr));
	ASSERT_TRUE(failCrossCast == nullptr, "Null pointer cross-cast should return nullptr");

	sprt::cout << "Multiple inheritance cross-cast: " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: Virtual Inheritance - diamond upcast with dynamic_cast (pointer form)
// ============================================================================
static void testVirtualInheritanceDiamondUpcast() {
	sprt::cout << "\n--- Testing virtual inheritance diamond upcast (diamond upcast) ---\n";

	int passes = 0;
	int failures = 0;

	// Create DiamondChild object - bottom of the diamond
	DiamondChild child(100, 200, 300, 400);

	// Test 1: Upcast from DiamondChild* to VirtualBase* using dynamic_cast
	auto childToVirtual = dynamic_cast<VirtualBase *>(&child);
	ASSERT_TRUE(childToVirtual == &child,
			"dynamic_cast DiamondChild* to VirtualBase* should succeed");
	ASSERT_TRUE(childToVirtual->virtualBaseValue == 100,
			"Upcasted pointer should access correct VirtualBase value");

	// Test 2: Upcast from VirtualLeft* to VirtualBase* using dynamic_cast
	auto leftToVirtual = static_cast<VirtualBase *>(&child);
	ASSERT_TRUE(leftToVirtual->virtualBaseValue == 100,
			"Upcast via VirtualLeft should access shared VirtualBase");

	// Test 3: Upcast from VirtualRight* to VirtualBase* using dynamic_cast
	auto rightToVirtual = static_cast<VirtualBase *>(&child);
	ASSERT_TRUE(rightToVirtual->virtualBaseValue == 100,
			"Upcast via VirtualRight should access shared VirtualBase");

	// Test 4: Verify both upcasts point to same VirtualBase subobject (key property of virtual inheritance)
	auto leftAsVirtual = static_cast<VirtualBase *>(&child);
	auto rightAsVirtual = static_cast<VirtualBase *>(&child);
	ASSERT_TRUE(leftAsVirtual == rightAsVirtual,
			"Virtual inheritance ensures single shared base subobject");

	sprt::cout << "Diamond upcast: " << passes << "/" << (passes + failures) << " passed\n";
}

// ============================================================================
// Test: Virtual Inheritance - diamond downcast with dynamic_cast (pointer form)
// ============================================================================
static void testVirtualInheritanceDiamondDowncast() {
	sprt::cout << "\n--- Testing virtual inheritance diamond downcast (diamond downcast) ---\n";

	int passes = 0;
	int failures = 0;

	// Create DiamondChild object and get VirtualBase* to it
	DiamondChild child(100, 200, 300, 400);
	auto virtualPtr = static_cast<VirtualBase *>(&child);

	// Test 1: Downcast from VirtualBase* to DiamondChild* - should succeed
	auto downcastToDiamond = dynamic_cast<DiamondChild *>(virtualPtr);
	ASSERT_TRUE(downcastToDiamond == &child,
			"Downcast from VirtualBase* to DiamondChild* should succeed");
	ASSERT_TRUE(downcastToDiamond->diamondValue == 400,
			"Downcasted pointer should access correct diamond value");

	// Test 2: Downcast from VirtualBase* to VirtualLeft* - should succeed
	auto downcastToLeft = dynamic_cast<VirtualLeft *>(virtualPtr);
	ASSERT_TRUE(downcastToLeft != nullptr,
			"Downcast from VirtualBase* to VirtualLeft* should succeed");

	// Test 3: Downcast from VirtualBase* to VirtualRight* - should succeed
	auto downcastToRight = dynamic_cast<VirtualRight *>(virtualPtr);
	ASSERT_TRUE(downcastToRight != nullptr,
			"Downcast from VirtualBase* to VirtualRight* should succeed");

	// Test 4: Downcast from VirtualLeft* to DiamondChild* - should succeed
	auto leftAsVirtual = static_cast<VirtualLeft *>(&child);
	auto downcastFromLeft = dynamic_cast<DiamondChild *>(leftAsVirtual);
	ASSERT_TRUE(downcastFromLeft == &child,
			"Downcast from VirtualLeft* to DiamondChild* should succeed");

	// Test 5: Downcast from VirtualRight* to DiamondChild* - should succeed
	auto rightAsVirtual = static_cast<VirtualRight *>(&child);
	auto downcastFromRight = dynamic_cast<DiamondChild *>(rightAsVirtual);
	ASSERT_TRUE(downcastFromRight == &child,
			"Downcast from VirtualRight* to DiamondChild* should succeed");

	// Test 6: Downcast failure - try to cast VirtualBase* (from non-diamond object) to DiamondChild*
	VirtualBase standalone(999);
	auto standalonePtr = &standalone;
	auto failDowncast = dynamic_cast<DiamondChild *>(standalonePtr);
	ASSERT_TRUE(failDowncast == nullptr,
			"Downcast from standalone VirtualBase* to DiamondChild* should return nullptr");

	sprt::cout << "Diamond downcast: " << passes << "/" << (passes + failures) << " passed\n";
}

// ============================================================================
// Test: Virtual Inheritance - verify single base subobject exists (pointer form)
// ============================================================================
static void testVirtualInheritanceSharedBase() {
	sprt::cout << "\n--- Testing virtual inheritance shared base verification ---\n";

	int passes = 0;
	int failures = 0;

	// Create DiamondChild object - bottom of the diamond
	DiamondChild child(100, 200, 300, 400);

	// Get VirtualBase* through both paths in the diamond
	auto viaLeft = static_cast<VirtualBase *>(&child);
	auto viaRight = static_cast<VirtualBase *>(&child);

	// Test 1: Both pointers must point to same memory address (single base subobject)
	ASSERT_TRUE(viaLeft == viaRight,
			"Virtual inheritance ensures only one VirtualBase subobject exists");

	// Test 2: Modifying through one path should be visible through the other
	viaLeft->virtualBaseValue = 999;
	ASSERT_TRUE(viaRight->virtualBaseValue == 999,
			"Modification via VirtualLeft path should be visible via VirtualRight");

	// Restore original value
	viaRight->virtualBaseValue = 100;
	ASSERT_TRUE(viaLeft->virtualBaseValue == 100,
			"Restored value should also be visible through both paths");

	// Test 3: typeid should return same type for both pointers
	const std::type_info &leftType = typeid(*viaLeft);
	const std::type_info &rightType = typeid(*viaRight);
	ASSERT_TRUE(leftType == rightType,
			"typeid should report identical types through both diamond paths");

	sprt::cout << "Shared base verification: " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Test: Mixed Multiple Inheritance and Virtual Inheritance
// ============================================================================
static void testMixedMIAndVirtualInheritance() {
	sprt::cout << "\n--- Testing mixed MI and virtual inheritance ---\n";

	int passes = 0;
	int failures = 0;

	// Create a DiamondChild that also implements InterfaceA (combining MI + virtual inheritance)
	// For this test, we'll use the existing hierarchy to demonstrate the concept

	// Test 1: MI object can be cast through both its interfaces independently
	MultipleInheritClass miObj(10, 20, 30);
	auto miToInterfaceA = dynamic_cast<InterfaceA *>(&miObj);
	auto miToInterfaceB = dynamic_cast<InterfaceB *>(&miObj);

	// Both casts should succeed and point to same MI object
	ASSERT_TRUE(miToInterfaceA == &miObj, "MI upcast to InterfaceA* should succeed");
	ASSERT_TRUE(miToInterfaceB == &miObj, "MI upcast to InterfaceB* should succeed");

	// Cross-cast between interfaces should also work
	auto crossCast = dynamic_cast<InterfaceB *>(miToInterfaceA);
	ASSERT_TRUE(crossCast == &miObj, "Cross-cast from InterfaceA* to InterfaceB* should succeed");

	// Test 2: Diamond object can be cast through both diamond paths and VirtualBase
	DiamondChild child(100, 200, 300, 400);
	auto childToVirtual = dynamic_cast<VirtualBase *>(&child);
	ASSERT_TRUE(childToVirtual == &child, "Diamond upcast to VirtualBase* should succeed");

	// Test 3: Verify MI and diamond can coexist in same test context without interference
	auto miDownCastA = dynamic_cast<MultipleInheritClass *>(miToInterfaceA);
	auto childDownCast = dynamic_cast<DiamondChild *>(childToVirtual);
	ASSERT_TRUE(miDownCastA == &miObj, "MI downcast from InterfaceA* should succeed");
	ASSERT_TRUE(childDownCast == &child, "Diamond downcast from VirtualBase* should succeed");

	// Test 4: Verify they are distinct objects (no cross-contamination)
	// Cast through void* because comparing distinct pointer types is a compiler error
	ASSERT_TRUE((void *)miDownCastA != (void *)childDownCast,
			"MI object and DiamondChild must be distinct");

	sprt::cout << "Mixed MI + virtual inheritance: " << passes << "/" << (passes + failures)
			   << " passed\n";
}

// ============================================================================
// Main test entry point - called from main.cpp
// ============================================================================
void performRttiTests() {
	sprt::cout << "\n== RTTI tests ==\n";

	int passes = 0;
	int failures = 0;

	MoreDerivedBase mdb(1, 2, 3);
	auto b = mdb.getBase();
	ASSERT_TRUE(dynamic_cast<Derived *>(b) != nullptr, "Ambiuous base test fails*");

	sprt::cout << "Special RTTI tests: " << passes << "/" << (passes + failures) << " passed\n";

	// Original single inheritance tests
	testDynamicCastUpcastPointer();
	testDynamicCastDowncastPointerSuccess();
	testDynamicCastDowncastPointerFailure();
	testDynamicCastPolymorphicRequirement();
	testMultipleDowncastsFromSameBase();

	// New multiple inheritance tests
	testMultipleInheritanceUpcast();
	testMultipleInheritanceDowncast();
	testMultipleInheritanceCrossCast();

	// New virtual inheritance (diamond pattern) tests
	testVirtualInheritanceDiamondUpcast();
	testVirtualInheritanceDiamondDowncast();
	testVirtualInheritanceSharedBase();

	// Mixed inheritance test
	testMixedMIAndVirtualInheritance();

	sprt::cout << "\n== RTTI tests complete ==\n";
}

} // namespace sprt

__SPRT_POP_ALLOW_CXXABI_ALLOC
