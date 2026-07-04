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

// [util.smartptr] shared_ptr / weak_ptr / enable_shared_from_this / make_shared + the
// pointer casts. The reference counts live in a control block and are maintained with the
// compiler's __atomic_* builtins (so use_count() is thread-safe). make_shared uses a single
// combined allocation (control block + inline object). NOT provided (documented): the
// allocator-aware allocate_shared, the C++20 make_shared<T[]> array forms, and
// atomic<shared_ptr> — no consumer needs them.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MEMORY_SHARED_PTR_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MEMORY_SHARED_PTR_H_

#include <sprt/cxx/cstddef>
#include <sprt/cxx/compare>
#include <sprt/cxx/new>
#include <sprt/cxx/type_traits>
#include <sprt/cxx/typeinfo>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__memory/unique_ptr.h>
#include <sprt/cxx/detail/hash.h>

// Like unique_ptr, shared_ptr's control blocks and default-delete path are the sanctioned
// users of the language new/delete; silence the freestanding "use sprt::__delete" nudge here.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace sprt {

template <typename _Tp>
class shared_ptr;
template <typename _Tp>
class weak_ptr;
template <typename _Tp>
class enable_shared_from_this;

// ---------------------------------------------------------- control block
class __sp_counted_base {
	long _use_count = 1;
	long _weak_count = 1; // the shared owners collectively hold one weak ref

public:
	__sp_counted_base() noexcept = default;
	__sp_counted_base(const __sp_counted_base &) = delete;
	__sp_counted_base &operator=(const __sp_counted_base &) = delete;
	virtual ~__sp_counted_base() { }

	// destroy the managed object; destroy the control block
	virtual void __dispose() noexcept = 0;
	virtual void __destroy() noexcept { delete this; }
	virtual void *__get_deleter(const type_info &) noexcept { return nullptr; }

	void __add_ref() noexcept { __atomic_fetch_add(&_use_count, 1, __ATOMIC_RELAXED); }
	// increment use_count only if it is not already zero (weak_ptr::lock)
	bool __add_ref_lock() noexcept {
		long __c = __atomic_load_n(&_use_count, __ATOMIC_RELAXED);
		while (__c != 0) {
			if (__atomic_compare_exchange_n(&_use_count, &__c, __c + 1, true, __ATOMIC_ACQ_REL,
						__ATOMIC_RELAXED)) {
				return true;
			}
		}
		return false;
	}
	void __release() noexcept {
		if (__atomic_fetch_sub(&_use_count, 1, __ATOMIC_ACQ_REL) == 1) {
			__dispose();
			__weak_release();
		}
	}
	void __weak_add_ref() noexcept { __atomic_fetch_add(&_weak_count, 1, __ATOMIC_RELAXED); }
	void __weak_release() noexcept {
		if (__atomic_fetch_sub(&_weak_count, 1, __ATOMIC_ACQ_REL) == 1) {
			__destroy();
		}
	}
	long __use_count() const noexcept { return __atomic_load_n(&_use_count, __ATOMIC_RELAXED); }
};

template <typename _Ptr>
class __sp_counted_ptr final : public __sp_counted_base {
	_Ptr _ptr;

public:
	explicit __sp_counted_ptr(_Ptr __p) noexcept : _ptr(__p) { }
	void __dispose() noexcept override { delete _ptr; }
	void __destroy() noexcept override { delete this; }
};

template <typename _Ptr, typename _Deleter>
class __sp_counted_deleter final : public __sp_counted_base {
	_Ptr _ptr;
	_Deleter _del;

public:
	__sp_counted_deleter(_Ptr __p, _Deleter __d) noexcept : _ptr(__p), _del(sprt::move_unsafe(__d)) { }
	void __dispose() noexcept override { _del(_ptr); }
	void __destroy() noexcept override { delete this; }
	void *__get_deleter(const type_info &__ti) noexcept override {
#ifdef __cpp_rtti
		return __ti == typeid(_Deleter) ? sprt::addressof(_del) : nullptr;
#else
		(void) __ti;
		return nullptr;
#endif
	}
};

// combined control-block + inline object storage for make_shared
template <typename _Tp>
class __sp_counted_inplace final : public __sp_counted_base {
	union {
		_Tp _obj;
	};

public:
	template <typename... _Args>
	explicit __sp_counted_inplace(_Args &&...__args) {
		::new (static_cast<void *>(sprt::addressof(_obj))) _Tp(sprt::forward<_Args>(__args)...);
	}
	~__sp_counted_inplace() { }
	_Tp *__get() noexcept { return sprt::addressof(_obj); }
	void __dispose() noexcept override { _obj.~_Tp(); }
	void __destroy() noexcept override { delete this; }
};

// Tag for the private "adopt an already-referenced control block" constructor, so it can't
// be confused with the public shared_ptr(_Yp*, _Deleter) constructor.
struct __sp_adopt_t {
	explicit __sp_adopt_t() = default;
};

// ---------------------------------------------------------- shared_ptr
template <typename _Tp>
class shared_ptr {
public:
	using element_type = remove_extent_t<_Tp>;
	using weak_type = weak_ptr<_Tp>;

private:
	element_type *_ptr = nullptr;
	__sp_counted_base *_cb = nullptr;

	template <typename _Up>
	friend class shared_ptr;
	template <typename _Up>
	friend class weak_ptr;
	template <typename _Up, typename... _Args>
	friend shared_ptr<_Up> make_shared(_Args &&...);
	template <typename _Del, typename _Up>
	friend _Del *get_deleter(const shared_ptr<_Up> &) noexcept;

	// adopting ctor: takes over an already-referenced control block (no add_ref)
	shared_ptr(element_type *__p, __sp_counted_base *__cb, __sp_adopt_t) noexcept
	: _ptr(__p), _cb(__cb) { }

	template <typename _Yp1, typename _Yp2>
	void __enable_weak_this(const enable_shared_from_this<_Yp1> *__e, _Yp2 *__p) noexcept {
		if (__e && __e->_weak_this.expired()) {
			__e->_weak_this = shared_ptr<_Yp1>(*this, const_cast<_Yp1 *>(static_cast<const _Yp1 *>(__p)));
		}
	}
	void __enable_weak_this(const volatile void *, const volatile void *) noexcept { }

public:
	constexpr shared_ptr() noexcept = default;
	constexpr shared_ptr(nullptr_t) noexcept { }

	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	explicit shared_ptr(_Yp *__p) : _ptr(__p), _cb(new __sp_counted_ptr<_Yp *>(__p)) {
		__enable_weak_this(__p, __p);
	}
	template <typename _Yp, typename _Deleter,
			enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	shared_ptr(_Yp *__p, _Deleter __d)
	: _ptr(__p), _cb(new __sp_counted_deleter<_Yp *, _Deleter>(__p, sprt::move_unsafe(__d))) {
		__enable_weak_this(__p, __p);
	}
	template <typename _Deleter>
	shared_ptr(nullptr_t __p, _Deleter __d)
	: _ptr(nullptr), _cb(new __sp_counted_deleter<_Tp *, _Deleter>(__p, sprt::move_unsafe(__d))) { }

	// aliasing constructors
	template <typename _Yp>
	shared_ptr(const shared_ptr<_Yp> &__r, element_type *__p) noexcept : _ptr(__p), _cb(__r._cb) {
		if (_cb) {
			_cb->__add_ref();
		}
	}
	template <typename _Yp>
	shared_ptr(shared_ptr<_Yp> &&__r, element_type *__p) noexcept : _ptr(__p), _cb(__r._cb) {
		__r._ptr = nullptr;
		__r._cb = nullptr;
	}

	shared_ptr(const shared_ptr &__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		if (_cb) {
			_cb->__add_ref();
		}
	}
	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	shared_ptr(const shared_ptr<_Yp> &__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		if (_cb) {
			_cb->__add_ref();
		}
	}

	shared_ptr(shared_ptr &&__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		__r._ptr = nullptr;
		__r._cb = nullptr;
	}
	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	shared_ptr(shared_ptr<_Yp> &&__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		__r._ptr = nullptr;
		__r._cb = nullptr;
	}

	// from weak_ptr (defined out of line; empty weak asserts instead of throwing)
	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	explicit shared_ptr(const weak_ptr<_Yp> &__r);

	// from unique_ptr
	template <typename _Yp, typename _Del,
			enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	shared_ptr(unique_ptr<_Yp, _Del> &&__u)
	: _ptr(__u.get())
	, _cb(__u.get() ? new __sp_counted_deleter<_Yp *, _Del>(__u.get(), __u.get_deleter())
					: nullptr) {
		auto *__raw = __u.get();
		__u.release();
		if (_cb) {
			__enable_weak_this(__raw, __raw);
		}
	}

	~shared_ptr() {
		if (_cb) {
			_cb->__release();
		}
	}

	shared_ptr &operator=(const shared_ptr &__r) noexcept {
		shared_ptr(__r).swap(*this);
		return *this;
	}
	template <typename _Yp>
	shared_ptr &operator=(const shared_ptr<_Yp> &__r) noexcept {
		shared_ptr(__r).swap(*this);
		return *this;
	}
	shared_ptr &operator=(shared_ptr &&__r) noexcept {
		shared_ptr(sprt::move_unsafe(__r)).swap(*this);
		return *this;
	}
	template <typename _Yp>
	shared_ptr &operator=(shared_ptr<_Yp> &&__r) noexcept {
		shared_ptr(sprt::move_unsafe(__r)).swap(*this);
		return *this;
	}
	template <typename _Yp, typename _Del>
	shared_ptr &operator=(unique_ptr<_Yp, _Del> &&__u) {
		shared_ptr(sprt::move_unsafe(__u)).swap(*this);
		return *this;
	}

	void reset() noexcept { shared_ptr().swap(*this); }
	template <typename _Yp>
	void reset(_Yp *__p) {
		shared_ptr(__p).swap(*this);
	}
	template <typename _Yp, typename _Deleter>
	void reset(_Yp *__p, _Deleter __d) {
		shared_ptr(__p, sprt::move_unsafe(__d)).swap(*this);
	}

	void swap(shared_ptr &__r) noexcept {
		element_type *__p = _ptr;
		_ptr = __r._ptr;
		__r._ptr = __p;
		__sp_counted_base *__c = _cb;
		_cb = __r._cb;
		__r._cb = __c;
	}

	element_type *get() const noexcept { return _ptr; }
	element_type &operator*() const noexcept
			requires(!is_array_v<_Tp>)
	{
		return *_ptr;
	}
	element_type *operator->() const noexcept
			requires(!is_array_v<_Tp>)
	{
		return _ptr;
	}
	element_type &operator[](ptrdiff_t __i) const
			requires(is_array_v<_Tp>)
	{
		return _ptr[__i];
	}
	long use_count() const noexcept { return _cb ? _cb->__use_count() : 0; }
	bool unique() const noexcept { return use_count() == 1; }
	explicit operator bool() const noexcept { return _ptr != nullptr; }

	template <typename _Yp>
	bool owner_before(const shared_ptr<_Yp> &__r) const noexcept {
		return _cb < __r._cb;
	}
	template <typename _Yp>
	bool owner_before(const weak_ptr<_Yp> &__r) const noexcept {
		return _cb < __r._cb;
	}
};

template <typename _Tp>
shared_ptr(weak_ptr<_Tp>) -> shared_ptr<_Tp>;
template <typename _Tp, typename _Del>
shared_ptr(unique_ptr<_Tp, _Del>) -> shared_ptr<_Tp>;

// ---------------------------------------------------------- weak_ptr
template <typename _Tp>
class weak_ptr {
public:
	using element_type = remove_extent_t<_Tp>;

private:
	element_type *_ptr = nullptr;
	__sp_counted_base *_cb = nullptr;

	template <typename _Up>
	friend class weak_ptr;
	template <typename _Up>
	friend class shared_ptr;

public:
	constexpr weak_ptr() noexcept = default;

	weak_ptr(const weak_ptr &__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		if (_cb) {
			_cb->__weak_add_ref();
		}
	}
	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	weak_ptr(const weak_ptr<_Yp> &__r) noexcept : _ptr(__r.lock().get()), _cb(__r._cb) {
		if (_cb) {
			_cb->__weak_add_ref();
		}
	}
	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	weak_ptr(const shared_ptr<_Yp> &__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		if (_cb) {
			_cb->__weak_add_ref();
		}
	}
	weak_ptr(weak_ptr &&__r) noexcept : _ptr(__r._ptr), _cb(__r._cb) {
		__r._ptr = nullptr;
		__r._cb = nullptr;
	}
	template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, element_type *>, int> = 0>
	weak_ptr(weak_ptr<_Yp> &&__r) noexcept : _ptr(__r.lock().get()), _cb(__r._cb) {
		__r._ptr = nullptr;
		__r._cb = nullptr;
	}

	~weak_ptr() {
		if (_cb) {
			_cb->__weak_release();
		}
	}

	weak_ptr &operator=(const weak_ptr &__r) noexcept {
		weak_ptr(__r).swap(*this);
		return *this;
	}
	template <typename _Yp>
	weak_ptr &operator=(const weak_ptr<_Yp> &__r) noexcept {
		weak_ptr(__r).swap(*this);
		return *this;
	}
	template <typename _Yp>
	weak_ptr &operator=(const shared_ptr<_Yp> &__r) noexcept {
		weak_ptr(__r).swap(*this);
		return *this;
	}
	weak_ptr &operator=(weak_ptr &&__r) noexcept {
		weak_ptr(sprt::move_unsafe(__r)).swap(*this);
		return *this;
	}

	long use_count() const noexcept { return _cb ? _cb->__use_count() : 0; }
	bool expired() const noexcept { return use_count() == 0; }
	shared_ptr<_Tp> lock() const noexcept {
		if (_cb && _cb->__add_ref_lock()) {
			return shared_ptr<_Tp>(_ptr, _cb, __sp_adopt_t{});
		}
		return shared_ptr<_Tp>();
	}
	void reset() noexcept { weak_ptr().swap(*this); }
	void swap(weak_ptr &__r) noexcept {
		element_type *__p = _ptr;
		_ptr = __r._ptr;
		__r._ptr = __p;
		__sp_counted_base *__c = _cb;
		_cb = __r._cb;
		__r._cb = __c;
	}
	template <typename _Yp>
	bool owner_before(const weak_ptr<_Yp> &__r) const noexcept {
		return _cb < __r._cb;
	}
	template <typename _Yp>
	bool owner_before(const shared_ptr<_Yp> &__r) const noexcept {
		return _cb < __r._cb;
	}
};

template <typename _Tp>
weak_ptr(shared_ptr<_Tp>) -> weak_ptr<_Tp>;

// out-of-line: shared_ptr from weak_ptr (needs weak_ptr complete)
template <typename _Tp>
template <typename _Yp, enable_if_t<is_convertible_v<_Yp *, typename shared_ptr<_Tp>::element_type *>, int>>
shared_ptr<_Tp>::shared_ptr(const weak_ptr<_Yp> &__r) : _ptr(nullptr), _cb(nullptr) {
	if (__r._cb && __r._cb->__add_ref_lock()) {
		_ptr = __r._ptr;
		_cb = __r._cb;
	} else {
		sprt_passert(false, "shared_ptr(weak_ptr): weak_ptr is expired");
	}
}

// ---------------------------------------------------------- enable_shared_from_this
template <typename _Tp>
class enable_shared_from_this {
protected:
	constexpr enable_shared_from_this() noexcept { }
	enable_shared_from_this(const enable_shared_from_this &) noexcept { }
	enable_shared_from_this &operator=(const enable_shared_from_this &) noexcept { return *this; }
	~enable_shared_from_this() { }

public:
	shared_ptr<_Tp> shared_from_this() { return shared_ptr<_Tp>(_weak_this); }
	shared_ptr<const _Tp> shared_from_this() const { return shared_ptr<const _Tp>(_weak_this); }
	weak_ptr<_Tp> weak_from_this() noexcept { return _weak_this; }
	weak_ptr<const _Tp> weak_from_this() const noexcept { return _weak_this; }

private:
	mutable weak_ptr<_Tp> _weak_this;
	template <typename _Up>
	friend class shared_ptr;
};

// ---------------------------------------------------------- make_shared
template <typename _Tp, typename... _Args>
inline shared_ptr<_Tp> make_shared(_Args &&...__args) {
	auto *__cb = new __sp_counted_inplace<_Tp>(sprt::forward<_Args>(__args)...);
	shared_ptr<_Tp> __sp(__cb->__get(), __cb, __sp_adopt_t{});
	__sp.__enable_weak_this(__cb->__get(), __cb->__get());
	return __sp;
}

// ---------------------------------------------------------- casts
template <typename _Tp, typename _Up>
inline shared_ptr<_Tp> static_pointer_cast(const shared_ptr<_Up> &__r) noexcept {
	using _E = typename shared_ptr<_Tp>::element_type;
	return shared_ptr<_Tp>(__r, static_cast<_E *>(__r.get()));
}
template <typename _Tp, typename _Up>
inline shared_ptr<_Tp> const_pointer_cast(const shared_ptr<_Up> &__r) noexcept {
	using _E = typename shared_ptr<_Tp>::element_type;
	return shared_ptr<_Tp>(__r, const_cast<_E *>(__r.get()));
}
template <typename _Tp, typename _Up>
inline shared_ptr<_Tp> reinterpret_pointer_cast(const shared_ptr<_Up> &__r) noexcept {
	using _E = typename shared_ptr<_Tp>::element_type;
	return shared_ptr<_Tp>(__r, reinterpret_cast<_E *>(__r.get()));
}
template <typename _Tp, typename _Up>
inline shared_ptr<_Tp> dynamic_pointer_cast(const shared_ptr<_Up> &__r) noexcept {
	using _E = typename shared_ptr<_Tp>::element_type;
	if (_E *__p = dynamic_cast<_E *>(__r.get())) {
		return shared_ptr<_Tp>(__r, __p);
	}
	return shared_ptr<_Tp>();
}

template <typename _Del, typename _Tp>
inline _Del *get_deleter(const shared_ptr<_Tp> &__p) noexcept {
#ifdef __cpp_rtti
	if (__p._cb) {
		return static_cast<_Del *>(__p._cb->__get_deleter(typeid(_Del)));
	}
#endif
	(void) __p;
	return nullptr;
}

// ---------------------------------------------------------- swap / compare / hash
template <typename _Tp>
inline void swap(shared_ptr<_Tp> &__a, shared_ptr<_Tp> &__b) noexcept {
	__a.swap(__b);
}
template <typename _Tp>
inline void swap(weak_ptr<_Tp> &__a, weak_ptr<_Tp> &__b) noexcept {
	__a.swap(__b);
}

template <typename _T1, typename _T2>
inline bool operator==(const shared_ptr<_T1> &__a, const shared_ptr<_T2> &__b) noexcept {
	return __a.get() == __b.get();
}
template <typename _T1, typename _T2>
inline strong_ordering operator<=>(const shared_ptr<_T1> &__a, const shared_ptr<_T2> &__b) noexcept {
	return __a.get() <=> __b.get();
}
template <typename _Tp>
inline bool operator==(const shared_ptr<_Tp> &__a, nullptr_t) noexcept {
	return !__a;
}
template <typename _Tp>
inline strong_ordering operator<=>(const shared_ptr<_Tp> &__a, nullptr_t) noexcept {
	return __a.get() <=> static_cast<typename shared_ptr<_Tp>::element_type *>(nullptr);
}

template <typename _Tp>
struct hash<shared_ptr<_Tp>> {
	size_t operator()(const shared_ptr<_Tp> &__p) const noexcept {
		return hash<typename shared_ptr<_Tp>::element_type *>()(__p.get());
	}
};

} // namespace sprt

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // RUNTIME_INCLUDE_SPRT_CXX___MEMORY_SHARED_PTR_H_
