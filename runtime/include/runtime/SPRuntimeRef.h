/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEREF_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEREF_H_

#include "SPRuntimeInt.h"
#include "SPRuntimeNotNull.h"

namespace sprt {

// Very simple Ref implementation, do not use it outside of runtime context,
// Use stappler::Ref full impplementation
class RtRef {
public:
	virtual ~RtRef() = default;

	uint64_t retain(uint64_t value = uint64_t(0) - 1) {
		(void)value;
		__atomic_fetch_add(&_referenceCount, 1, __ATOMIC_SEQ_CST);
		return 0;
	}
	void release(uint64_t id) {
		if (__atomic_fetch_sub(&_referenceCount, 1, __ATOMIC_SEQ_CST) == 1) {
			delete this;
		}
	}

protected:
	RtRef() noexcept { }

	uint32_t _referenceCount = 1;
};


template <typename _Pointer>
class RtRc {
public:
	using Type = _Pointer;
	using Base = _Pointer;
	using Pointer = _Pointer *;

	static RtRc<Base> alloc();

	template <typename... Args>
	static RtRc<Base> alloc(Args &&...);

	RtRc() noexcept;
	RtRc(const nullptr_t &) noexcept;
	RtRc(const Pointer &value) noexcept;
	RtRc(const RtRc &v) noexcept;
	RtRc(RtRc &&v) noexcept;

	RtRc &operator=(const nullptr_t &) noexcept;
	RtRc &operator=(const Pointer &value) noexcept;
	RtRc &operator=(const NotNull<remove_pointer_t<Pointer>> &value) noexcept;
	RtRc &operator=(const RtRc &v) noexcept;
	RtRc &operator=(RtRc &&v) noexcept;

	~RtRc();

	void set(const Pointer &value);
	void swap(RtRc &v);
	void clear();

	template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type * = nullptr>
	inline RtRc &operator=(const RtRc<B> &value) noexcept;

	template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type * = nullptr>
	inline RtRc &operator=(RtRc<B> &&value) noexcept;

	template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type * = nullptr>
	RtRc &operator=(NotNull<B>) noexcept;

	// upcast
	template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type * = nullptr>
	B *get_cast() const noexcept;

	// Direct call of `get` should not be on empty storage
	Base *get() const noexcept;

	operator Base *() const noexcept;

	operator NotNull<Base>() const noexcept;

	template <typename B, typename enable_if<is_convertible<Base *, B *>{}>::type * = nullptr>
	operator NotNull<B>() const noexcept;

	Base *operator->() const noexcept;

	explicit operator bool() const noexcept;

	template <typename B, typename enable_if<is_convertible<Base *, B *>{}>::type * = nullptr>
	operator RtRc<B>() noexcept;

	template <typename Target>
	RtRc<Target> cast() const;


protected:
	inline void doRetain();
	inline void doRelease();

	inline Pointer doSwap(Pointer value);

	// unsafe assignment
	inline RtRc(Pointer value, bool v) noexcept;

	Pointer _ptr = nullptr;
};

template <typename _Base>
inline auto RtRc<_Base>::alloc() -> RtRc<Base> {
	return RtRc(Type::alloc(), true);
}

template <typename _Base>
template < typename... Args>
inline auto RtRc<_Base>::alloc(Args &&...args) -> RtRc<Base> {
	return RtRc(Type::alloc(forward<Args>(args)...), true);
}

template <typename _Pointer>
inline RtRc<_Pointer>::RtRc() noexcept : _ptr(nullptr) { }

template <typename _Pointer>
inline RtRc<_Pointer>::RtRc(const nullptr_t &) noexcept : _ptr(nullptr) { }

template <typename _Pointer>
inline RtRc<_Pointer>::RtRc(const Pointer &value) noexcept : _ptr(value) {
	doRetain();
}

template <typename _Pointer>
inline RtRc<_Pointer>::RtRc(const RtRc &v) noexcept : _ptr(v._ptr) {
	doRetain();
}

template <typename _Pointer>
inline RtRc<_Pointer>::RtRc(RtRc &&v) noexcept : _ptr(v._ptr) {
	v._ptr = nullptr;
}

template <typename _Pointer>
inline auto RtRc<_Pointer>::operator=(const nullptr_t &) noexcept -> RtRc & {
	clear();
	return *this;
}

template <typename _Pointer>
inline auto RtRc<_Pointer>::operator=(const Pointer &value) noexcept -> RtRc & {
	set(value);
	return *this;
}

template <typename _Pointer>
inline auto RtRc<_Pointer>::operator=(const NotNull<remove_pointer_t<Pointer>> &value) noexcept
		-> RtRc & {
	set(value);
	return *this;
}

template <typename _Pointer>
inline auto RtRc<_Pointer>::operator=(const RtRc &v) noexcept -> RtRc & {
	if (this == &v) {
		return *this;
	}
	set(v._ptr);
	return *this;
}

template <typename _Pointer>
inline auto RtRc<_Pointer>::operator=(RtRc &&v) noexcept -> RtRc & {
	if (this == &v) {
		return *this;
	}

	doRelease();
	_ptr = v._ptr;
	v._ptr = nullptr;
#if SP_REF_DEBUG
	_id = v._id;
	v._id = 0;
#endif
	return *this;
}

template <typename _Pointer>
inline RtRc< _Pointer>::~RtRc() {
	doRelease();
	_ptr = nullptr;
}

template <typename _Pointer>
inline void RtRc<_Pointer>::set(const Pointer &value) {
	_ptr = doSwap(value);
}

template < typename _Pointer>
inline void RtRc<_Pointer>::swap(RtRc &v) {
	sprt::swap(_ptr, v._ptr);
}

template <typename _Pointer>
inline void RtRc<_Pointer>::clear() {
	doRelease();
	_ptr = nullptr;
}

template <typename _Pointer>
inline void RtRc< _Pointer>::doRetain() {
#if SPRT_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
#else
	auto ptr = _ptr;
#endif

	if (ptr) {
		ptr->retain();
	}
}

template < typename _Pointer>
inline void RtRc<_Pointer>::doRelease() {
#if SPRT_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
#else
	auto ptr = _ptr;
#endif

	if (ptr) {
		ptr->release(0);
	}
}

template < typename _Pointer>
inline auto RtRc<_Pointer>::doSwap(Pointer _value) -> Pointer {
#if SPRT_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
	auto value = (Ref *)_value;
#else
	auto ptr = _ptr;
	auto value = _value;
#endif

	if (value) {
		value->retain();
	}
	if (ptr) {
		ptr->release(0);
	}
	return (Pointer)value;
}

template < typename _Pointer>
inline RtRc<_Pointer>::RtRc(Pointer value, bool v) noexcept : _ptr(value) { }

template <typename Base>
template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type *>
inline auto RtRc<Base>::operator=(const RtRc<B> &value) noexcept -> RtRc & {
	this->set(value);
	return *this;
}

template <typename Base>
template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type *>
inline auto RtRc<Base>::operator=(RtRc<B> &&value) noexcept -> RtRc & {
	this->_ptr = static_cast<Type *>(value._ptr);
	value._ptr = nullptr;
	return *this;
}

template <typename Base>
template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type *>
inline auto RtRc<Base>::operator=(NotNull<B> value) noexcept -> RtRc & {
	this->set(value.get());
	return *this;
}

// upcast
template <typename Base>
template <typename B, typename enable_if<is_convertible<B *, Base *>{}>::type *>
inline auto RtRc<Base>::get_cast() const noexcept -> B * {
	return static_cast<B *>(this->_ptr);
}

template <typename Base>
inline auto RtRc<Base>::get() const noexcept -> Base * {
	return this->_ptr;
}

template <typename Base>
inline RtRc<Base>::operator Base *() const noexcept {
	return *this ? get() : nullptr;
}

template <typename Base>
inline RtRc<Base>::operator NotNull<Base>() const noexcept {
	auto ptr = get();
	return ptr;
}

template <typename Base>
template <typename B, typename enable_if<is_convertible<Base *, B *>{}>::type *>
inline RtRc<Base>::operator NotNull<B>() const noexcept {
	auto ptr = get();
	return ptr;
}

template <typename Base>
inline auto RtRc<Base>::operator->() const noexcept -> Base * {
	return this->_ptr;
}

template <typename Base>
inline RtRc<Base>::operator bool() const noexcept {
	return this->_ptr != nullptr;
}

template <typename Base>
template <typename B, typename enable_if<is_convertible<Base *, B *>{}>::type *>
inline RtRc<Base>::operator RtRc<B>() noexcept {
	return Rc<B>(static_cast<B *>(get()));
}

template <typename Base>
template <typename Target>
inline RtRc<Target> RtRc<Base>::cast() const {
	if (auto v = dynamic_cast<Target *>(this->_ptr)) {
		return RtRc<Target>(v);
	}
	return RtRc<Target>(nullptr);
}

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEREF_H_
