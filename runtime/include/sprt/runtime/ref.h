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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_REF_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_REF_H_

#include <sprt/runtime/utils/notnull.h>
#include <sprt/runtime/stringview.h>

#include <sprt/cxx/list>
#include <sprt/cxx/new>
#include <sprt/cxx/memory>
#include <sprt/cxx/atomic>

// enable Ref debug mode to track retain/release sources
#ifndef SPRT_REF_DEBUG
#define SPRT_REF_DEBUG 1
#endif

// WinCRT instantiates Rc's in place of definition, not actual usage
// So, we need to safely call Ref's methods on classes with only forward declarations visible
#if SPRT_WINDOWS
#define SPRT_REF_SAFE_INSTANIATION 1
#else
#define SPRT_REF_SAFE_INSTANIATION 0
#endif

namespace sprt {

class SPRT_API Ref;

template <typename T, typename... Args>
inline T *__new(Args &&...args) noexcept;

template <typename T>
inline void __delete(T *t) noexcept;

template <typename T>
inline uint64_t retain(T *t, uint64_t value = Max<uint64_t>) noexcept;

template <typename T>
inline void release(T *t, uint64_t value) noexcept;

namespace memleak {

SPRT_API uint64_t getNextRefId();

SPRT_API uint64_t retainBacktrace(const Ref *, uint64_t = Max<uint64_t>);
SPRT_API void releaseBacktrace(const Ref *, uint64_t);
SPRT_API void releaseRef(const Ref *);
SPRT_API void foreachBacktrace(const Ref *,
		const callback<void(uint64_t, time_t, const __pool_list<StringView> &)> &);

} // namespace memleak

// Reference allocation base class
// References use thread-safe reference counting mechanism
// Reference object will be removed, when _referenceCount decremented to zero
//
// Reference object can be allocated from memory pool, in this case memory will be freed only when memory pool is destroyed

class SPRT_API RefAlloc : AllocBase {
public:
	template <typename T, typename... Args>
	static T *__new(Args &&...args) {
		T *uninitialized = nullptr;
		if constexpr (toInt(alignof(T)) <= alignof(__sprt_max_align_t)) {
			uninitialized = reinterpret_cast<T *>(__sprt_malloc(sizeof(T)));
		} else {
			uninitialized = reinterpret_cast<T *>(__sprt_aligned_alloc(alignof(T), sizeof(T)));
		}

		return sprt::__construct_at(uninitialized, sprt::forward<Args>(args)...);
	}

	template <typename T, typename... Args>
	static T *__pnew(memory::pool_t *pool, Args &&...args) {
		// Use __construct_at with disabled checker to use with protected constructors
		return sprt::__construct_at(
				reinterpret_cast<T *>(sprt::memory::pool::palloc(pool, sizeof(T), alignof(T))),
				sprt::forward<Args>(args)...);
	}

	// Disable default ABI operators
	static void *operator new(size_t size) = delete;
	static void *operator new[](size_t size) = delete;

	// Disable placements
	static void *operator new(size_t size, void *ptr) noexcept = delete;

	static void *operator new(size_t size, sprt::align_val_t al,
			const sprt::nothrow_t &tag) noexcept = delete;

	static void *operator new(size_t size, sprt::align_val_t al,
			memory::pool_t *ptr) noexcept = delete;

	static inline void operator delete(void *ptr) noexcept = delete;
	static inline void operator delete(void *ptr, align_val_t al) noexcept = delete;

	static void operator delete(RefAlloc *ptr, sprt::destroying_delete_t) noexcept;

	virtual ~RefAlloc();

	uint32_t getReferenceCount() const noexcept;

protected:
	RefAlloc() noexcept;

	// DO NOT USE THIS DIRECTLY, use sprt::retain
	void incrementReferenceCount();

	// DO NOT USE THIS DIRECTLY, use sprt::release
	bool decrementReferenceCount();

	atomic<uint32_t> _referenceCount = 1;
};

class SPRT_API Ref : public RefAlloc {
public:
	// You can assign an unique number for retain/release sequence to track leaked references
	// In this case, release should be called with the same id, as returned from retain.
	// Retain argument can be used as designated id, or maxOf<uint64_t>() to allocate unique id
	// If SPRT_REF_DEBUG is not enabled - it's noop

#if SPRT_REF_DEBUG
	virtual ~Ref() {
		if (isRetainTrackerEnabled()) {
			memleak::releaseRef(this);
		}
	}

	virtual void foreachBacktrace(
			const callback<void(uint64_t, time_t, const __pool_list<StringView> &)> &cb) const {
		memleak::foreachBacktrace(this, cb);
	}

#else
	virtual ~Ref() = default;

#endif

protected:
	template <typename U>
	friend uint64_t retain(U *, uint64_t) noexcept;

	template <typename U>
	friend void release(U *, uint64_t) noexcept;

#if SPRT_REF_DEBUG
	virtual uint64_t retain(uint64_t value = Max<uint64_t>) {
		incrementReferenceCount();
		if (isRetainTrackerEnabled()) {
			return memleak::retainBacktrace(this, value);
		}
		return 0;
	}

	[[nodiscard("Caller should delete object with __delete if release returns true")]]
	virtual bool release(uint64_t v) {
		if (isRetainTrackerEnabled()) {
			memleak::releaseBacktrace(this, v);
		}
		if (decrementReferenceCount()) {
			return true;
		}
		return false;
	}
#else
	uint64_t retain(uint64_t value = Max<uint64_t>) {
		(void)value;
		incrementReferenceCount();
		return 0;
	}

	[[nodiscard("Caller should delete object with __delete if release returns true")]]
	bool release(uint64_t id) {
		if (decrementReferenceCount()) {
			return true;
		}
		return false
	}
#endif

protected:
	Ref() noexcept : RefAlloc() { }

	// override this method to enable automatic retain/release tracking for subclass
#if SPRT_REF_DEBUG
	virtual bool isRetainTrackerEnabled() const { return false; }
#endif
};

enum class SharedRefMode {
	Pool,
	Allocator,
};

/** Интерфейс разделяемых объектов на основе пулов памяти и подсчёта ссылок
 *
 * Используется в виде Rc<SharedRef<Type>>. Интерфейс аналогичен Rc<Type> для
 * простого подсчёта ссылок.
 *
 * Пул памяти связывается с новым объектом и удаляется при исчерпании ссылок.
 *
 */

template <typename T>
class SPRT_API SharedRef final : public Ref {
public:
	template <typename... Args>
	static SharedRef *create(Args &&...);

	template <typename... Args>
	static SharedRef *create(memory::pool_t *, Args &&...);

	template <typename... Args>
	static SharedRef *create(SharedRefMode, Args &&...);

	static Status invaldate(void *);

	virtual ~SharedRef();

	template <typename Callback>
	void perform(Callback &&cb);

	inline T *get() const noexcept { return _shared; }

	inline operator T *() const noexcept { return get(); }
	inline T *operator->() const noexcept { return get(); }

	inline explicit operator bool() const noexcept { return _shared != nullptr; }

	memory::pool_t *getPool() const { return _pool; }
	memory::allocator_t *getAllocator() const { return _allocator; }

protected:
	// Uncomment to track retain/release cycles
	//#if SPRT_REF_DEBUG
	//	virtual bool isRetainTrackerEnabled() const { return true; }
	//#endif

	template <typename U>
	friend uint64_t retain(U *, uint64_t) noexcept;

	template <typename U>
	friend void release(U *, uint64_t) noexcept;

	static void __delete(SharedRef *t) noexcept {
		auto allocator = t->_allocator;
		auto pool = t->_pool;

		sprt::destroy_at(t);

		if (pool) {
			sprt::memory::pool::destroy(pool);
		}
		if (allocator) {
			sprt::memory::allocator::destroy(allocator);
		}
	}

	template <typename _Tp, typename... _Args>
	friend constexpr _Tp *__construct_at(_Tp *__location, _Args &&...__args) noexcept;

	SharedRef(SharedRefMode m, memory::allocator_t *, memory::pool_t *) noexcept;

	memory::allocator_t *_allocator = nullptr;
	memory::pool_t *_pool = nullptr;
	memory::pool_t *_parent = nullptr;
	T *_shared = nullptr;
	SharedRefMode _mode = SharedRefMode::Pool;
};

template <typename T>
struct __is_shared_ref : false_type { };

template <typename T>
struct __is_shared_ref<SharedRef<T>> : true_type { };

template <typename _Base, typename _Pointer>
class RcBase {
public:
	using Base = _Base;
	using Type = typename remove_cv<_Base>::type;
	using Pointer = _Pointer;

	RcBase() noexcept;
	RcBase(const nullptr_t &) noexcept;
	RcBase(const Pointer &value) noexcept;
	RcBase(const RcBase<Base, Pointer> &v) noexcept;
	RcBase(RcBase<Base, Pointer> &&v) noexcept;

	RcBase &operator=(const nullptr_t &) noexcept;

	RcBase &operator=(const Pointer &value) noexcept;
	RcBase &operator=(const NotNull<remove_pointer_t<Pointer>> &value) noexcept;
	RcBase &operator=(const RcBase<Base, Pointer> &v) noexcept;
	RcBase &operator=(RcBase<Base, Pointer> &&v) noexcept;

	~RcBase();

	void set(const Pointer &value);
	void swap(RcBase<Base, Pointer> &v);
	void clear();

	bool operator==(const RcBase<Base, Pointer> &other) const;
	bool operator==(const Base *&other) const;
	bool operator==(typename remove_const<Base>::type *other) const;
	bool operator==(const nullptr_t other) const;

	bool operator!=(const RcBase<Base, Pointer> &other) const;
	bool operator!=(const Base *&other) const;
	bool operator!=(typename remove_const<Base>::type *other) const;
	bool operator!=(const nullptr_t other) const;

	bool operator>(const RcBase<Base, Pointer> &other) const;
	bool operator>(const Base *other) const;
	bool operator>(typename remove_const<Base>::type *other) const;
	bool operator>(const nullptr_t other) const;

	bool operator<(const RcBase<Base, Pointer> &other) const;
	bool operator<(const Base *other) const;
	bool operator<(typename remove_const<Base>::type *other) const;
	bool operator<(const nullptr_t other) const;

	bool operator>=(const RcBase<Base, Pointer> &other) const;
	bool operator>=(const Base *other) const;
	bool operator>=(typename remove_const<Base>::type *other) const;
	bool operator>=(const nullptr_t other) const;

	bool operator<=(const RcBase<Base, Pointer> &other) const;
	bool operator<=(const Base *other) const;
	bool operator<=(typename remove_const<Base>::type *other) const;
	bool operator<=(const nullptr_t other) const;

#if SPRT_REF_DEBUG
	uint64_t getId() const { return _id; }
#endif
protected:
	inline void doRetain();
	inline void doRelease();

	inline Pointer doSwap(Pointer value);

// unsafe assignment
#if SPRT_REF_SAFE_INSTANIATION
public:
#else
protected:
#endif
	inline RcBase(Pointer value, bool v) noexcept;

protected:
	Pointer _ptr = nullptr;
#if SPRT_REF_DEBUG
	uint64_t _id = 0;
#endif
};

template <typename _Base>
class Rc final : public RcBase<_Base, typename remove_cv<_Base>::type *> {
public:
	using Parent = RcBase<_Base, typename remove_cv<_Base>::type *>;
	using Self = Rc<_Base>;
	using Type = typename remove_cv<_Base>::type;

	template <typename Target>
	static auto doReferenceCast(Rc<Type> &&source) -> Rc<Target>;

	template <typename... Args>
	static inline Self create(Args &&...args);

	static inline Self alloc();

	template <typename... Args>
	static inline Self alloc(Args &&...args);

	using Parent::Parent;
	using Parent::operator=;

	template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type * = nullptr>
	Rc(const Rc<B> &value) noexcept {
		this->set(value);
	}

	template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type * = nullptr>
	Rc(NotNull<B> value) noexcept {
		this->set(value);
	}

	template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type * = nullptr>
	inline Rc &operator=(const Rc<B> &value) noexcept;

	template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type * = nullptr>
	inline Rc &operator=(Rc<B> &&value) noexcept;

	template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type * = nullptr>
	Rc &operator=(NotNull<B>) noexcept;

	// upcast
	template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type * = nullptr>
	B *get_cast() const noexcept;

	// Direct call of `get` should not be on empty storage
	_Base *get() const noexcept;

	operator _Base *() const noexcept;

	operator NotNull<_Base>() const noexcept;

	template <typename B, typename enable_if<is_convertible_v<_Base *, B *>>::type * = nullptr>
	operator NotNull<B>() const noexcept;

	_Base *operator->() const noexcept;

	explicit operator bool() const noexcept;

	template <typename B, typename enable_if<is_convertible_v<_Base *, B *>>::type * = nullptr>
	operator Rc<B>() noexcept;

	template <typename Target>
	Rc<Target> cast() const;

	template <typename T>
	friend class Rc;
};

template <typename _Base>
class Rc<SharedRef<_Base>> final : public RcBase<_Base, SharedRef<_Base> *> {
public:
	using Parent = RcBase<_Base, SharedRef<_Base> *>;
	using Self = Rc<SharedRef<_Base>>;
	using Type = SharedRef<_Base>;

	template <typename... Args>
	static Self create(Args &&...args);

	template <typename... Args>
	static Self create(memory::pool_t *pool, Args &&...args);

	template <typename... Args>
	static Self create(SharedRefMode mode, Args &&...args);

	static Self alloc();

	template <typename... Args>
	static Self alloc(Args &&...args);

	template <typename... Args>
	static Self alloc(memory::pool_t *pool, Args &&...args);

	template <typename... Args>
	static Self alloc(SharedRefMode mode, Args &&...args);

	using Parent::Parent;
	using Parent::operator=;

	// Direct call of `get` should not be on empty storage
	_Base *get() const noexcept {
#if SPRT_REF_DEBUG
		assert(this->_ptr);
#endif
		return this->_ptr->get();
	}

	inline operator _Base *() const noexcept { return get(); }

	inline _Base *operator->() const noexcept { return this->_ptr->get(); }

	inline explicit operator bool() const noexcept {
		return this->_ptr != nullptr && this->_ptr->get() != nullptr;
	}
};

template <typename T>
using SharedRc = Rc<SharedRef<T>>;

// Cast between reference types, possibly, without retain/release cycle
// Can be performed for upcast/downcast
template <typename Target, typename Source,
		typename enable_if<is_convertible_v<Target *, Source *>
				|| is_convertible_v<Source *, Target *>>::type * = nullptr>
auto ref_cast(Rc<Source> &&source) -> Rc<Target> {
	return Rc<Source>::template doReferenceCast<Target>(move(source));
}

template <typename Target, typename Source,
		typename enable_if<is_convertible_v<Target *, Source *>
				|| is_convertible_v<Source *, Target *>>::type * = nullptr>
auto ref_cast(const Rc<Source> &source) -> Rc<Target> {
	return Rc<Source>::template doReferenceCast<Target>(source.get());
}

//
// Implementation
//

inline uint32_t RefAlloc::getReferenceCount() const noexcept { return _referenceCount.load(); }

// DO NOT USE THIS DIRECTLY, use Ref::retain
inline void RefAlloc::incrementReferenceCount() { ++_referenceCount; }

// DO NOT USE THIS DIRECTLY, use Ref::release
inline bool RefAlloc::decrementReferenceCount() {
	if (_referenceCount.fetch_sub(1) == 1) {
		return true;
	}
	return false;
}

template <typename T>
template <typename... Args>
auto SharedRef<T>::create(Args &&...args) -> SharedRef * {
	auto pool = sprt::memory::pool::create((memory::pool_t *)nullptr);

	SharedRef *shared = nullptr;
	memory::perform([&] {
		shared = __pnew<SharedRef>(pool, SharedRefMode::Pool, nullptr, pool);
		if (shared) {
			shared->_shared = __pnew<T>(pool, shared, pool, sprt::forward<Args>(args)...);
		}
	}, pool);
	return shared;
}

template <typename T>
template <typename... Args>
auto SharedRef<T>::create(memory::pool_t *p, Args &&...args) -> SharedRef * {
	auto pool = sprt::memory::pool::create(p);

	SharedRef *shared = nullptr;
	memory::perform([&] {
		shared = __pnew<SharedRef>(pool, SharedRefMode::Pool, nullptr, pool);
		if (shared) {
			shared->_shared = __pnew<T>(pool, shared, pool, sprt::forward<Args>(args)...);
		}
		if (p) {
			shared->_parent = p;
			sprt::memory::pool::pre_cleanup_register(shared->_parent, shared, &invaldate);
		}
	}, pool);
	return shared;
}

template <typename T>
template <typename... Args>
auto SharedRef<T>::create(SharedRefMode mode, Args &&...args) -> SharedRef * {
	memory::allocator_t *alloc = nullptr;
	memory::pool_t *pool = nullptr;

	switch (mode) {
	case SharedRefMode::Pool: pool = sprt::memory::pool::create((memory::pool_t *)nullptr); break;
	case SharedRefMode::Allocator:
		alloc = sprt::memory::allocator::create();
		pool = sprt::memory::pool::create(alloc);
		break;
	}

	SharedRef *shared = nullptr;
	memory::perform([&] {
		shared = __pnew<SharedRef>(pool, mode, alloc, pool);
		if (shared) {
			shared->_shared = __pnew<T>(pool, shared, pool, sprt::forward<Args>(args)...);
		}
	}, pool);
	return shared;
}

template <typename T>
Status SharedRef<T>::invaldate(void *ptr) {
	auto shared = (SharedRef *)ptr;

	shared->_shared = nullptr;
	shared->_pool = nullptr;
	shared->_parent = nullptr;

	return Status::Ok;
}

template <typename T>
SharedRef<T>::~SharedRef() {
	if (_shared) {
		memory::perform([&, this] { sprt::destroy_at(_shared); }, _pool);
		_shared = nullptr;
	}

	auto parent = _parent;

	if (parent) {
		sprt::memory::pool::cleanup_kill(parent, this, &invaldate);
		parent = nullptr;
	}

	_parent = nullptr;
	_pool = nullptr;
	_allocator = nullptr;
}

template <typename T>
template <typename Callback>
void SharedRef<T>::perform(Callback &&cb) {
	if constexpr (is_invocable_v<Callback, memory::pool_t *, T *>) {
		memory::perform([&, this] { cb(_pool, _shared); }, _pool);
	} else {
		static_assert(is_invocable_v<Callback, T *>, "Invalid callback type");
		memory::perform([&, this] { cb(_shared); }, _pool);
	}
}

template <typename T>
SharedRef<T>::SharedRef(SharedRefMode m, memory::allocator_t *alloc, memory::pool_t *pool) noexcept
: _allocator(alloc), _pool(pool), _shared(nullptr), _mode(m) { }


template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::RcBase() noexcept : _ptr(nullptr) { }

template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::RcBase(const nullptr_t &) noexcept : _ptr(nullptr) { }

template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::RcBase(const Pointer &value) noexcept : _ptr(value) {
	doRetain();
}

template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::RcBase(const RcBase<Base, Pointer> &v) noexcept : _ptr(v._ptr) {
	doRetain();
}

template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::RcBase(RcBase<Base, Pointer> &&v) noexcept : _ptr(v._ptr) {
	v._ptr = nullptr;
#if SPRT_REF_DEBUG
	_id = v._id;
	v._id = 0;
#endif
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::operator=(const nullptr_t &) noexcept -> RcBase & {
	clear();
	return *this;
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::operator=(const Pointer &value) noexcept -> RcBase & {
	set(value);
	return *this;
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::operator=(
		const NotNull<remove_pointer_t<Pointer>> &value) noexcept -> RcBase & {
	set(value);
	return *this;
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::operator=(const RcBase<Base, Pointer> &v) noexcept
		-> RcBase & {
	if (this == &v) {
		return *this;
	}
	set(v._ptr);
	return *this;
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::operator=(RcBase<Base, Pointer> &&v) noexcept -> RcBase & {
	if (this == &v) {
		return *this;
	}

	doRelease();
	_ptr = v._ptr;
	v._ptr = nullptr;
#if SPRT_REF_DEBUG
	_id = v._id;
	v._id = 0;
#endif
	return *this;
}

template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::~RcBase() {
	doRelease();
	_ptr = nullptr;
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::set(const Pointer &value) {
	_ptr = doSwap(value);
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::swap(RcBase<Base, Pointer> &v) {
	swap(_ptr, v._ptr);
#if SPRT_REF_DEBUG
	swap(_id, v._id);
#endif
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::clear() {
	doRelease();
	_ptr = nullptr;
#if SPRT_REF_DEBUG
	_id = 0;
#endif
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator==(const RcBase<Base, Pointer> &other) const {
	return _ptr == other._ptr;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator==(const Base *&other) const {
	return _ptr == other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator==(typename remove_const<Base>::type *other) const {
	return _ptr == other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator==(const nullptr_t other) const {
	return _ptr == other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator!=(const RcBase<Base, Pointer> &other) const {
	return _ptr != other._ptr;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator!=(const Base *&other) const {
	return _ptr != other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator!=(typename remove_const<Base>::type *other) const {
	return _ptr != other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator!=(const nullptr_t other) const {
	return _ptr != other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>(const RcBase<Base, Pointer> &other) const {
	return _ptr > other._ptr;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>(const Base *other) const {
	return _ptr > other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>(typename remove_const<Base>::type *other) const {
	return _ptr > other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>(const nullptr_t other) const {
	return _ptr > other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<(const RcBase<Base, Pointer> &other) const {
	return _ptr < other._ptr;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<(const Base *other) const {
	return _ptr < other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<(typename remove_const<Base>::type *other) const {
	return _ptr < other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<(const nullptr_t other) const {
	return _ptr < other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>=(const RcBase<Base, Pointer> &other) const {
	return _ptr >= other._ptr;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>=(const Base *other) const {
	return _ptr >= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>=(typename remove_const<Base>::type *other) const {
	return _ptr >= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>=(const nullptr_t other) const {
	return _ptr >= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<=(const RcBase<Base, Pointer> &other) const {
	return _ptr <= other._ptr;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<=(const Base *other) const {
	return _ptr <= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<=(typename remove_const<Base>::type *other) const {
	return _ptr <= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<=(const nullptr_t other) const {
	return _ptr <= other;
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::doRetain() {
#if SPRT_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
#else
	auto ptr = _ptr;
#endif

#if SPRT_REF_DEBUG
	if (ptr) {
		_id = sprt::retain(ptr);
	}
#else
	if (ptr) {
		sprt::retain(ptr);
	}
#endif
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::doRelease() {
#if SPRT_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
#else
	auto ptr = _ptr;
#endif

#if SPRT_REF_DEBUG
	if (ptr) {
		sprt::release(ptr, _id);
	}
#else
	if (ptr) {
		sprt::release(ptr, 0);
	}
#endif
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::doSwap(Pointer _value) -> Pointer {
#if SPRT_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
	auto value = (Ref *)_value;
#else
	auto ptr = _ptr;
	auto value = _value;
#endif

#if SPRT_REF_DEBUG
	uint64_t id = 0;
	if (value) {
		id = sprt::retain(value);
	}
	if (ptr) {
		sprt::release(ptr, _id);
	}
	_id = id;
	return (Pointer)value;
#else
	if (value) {
		sprt::retain(value);
	}
	if (ptr) {
		sprt::release(ptr, 0);
	}
	return (Pointer)value;
#endif
}

template <typename _Base, typename _Pointer>
inline RcBase<_Base, _Pointer>::RcBase(Pointer value, bool v) noexcept : _ptr(value) { }


template <typename _Base>
template <typename Target>
inline auto Rc<_Base>::doReferenceCast(Rc<Type> &&source) -> Rc<Target> {
	Rc<Target> ret;
	ret._ptr = static_cast<Target *>(source._ptr);
	source._ptr = nullptr;
#if SPRT_REF_DEBUG
	ret._id = source._id;
	source._id = 0;
#endif
	return ret;
}

template <typename _Base>
template <typename... Args>
inline auto Rc<_Base>::create(Args &&...args) -> Self {
	static_assert(is_base_of<Ref, _Base>::value, "Rc base class should be derived from Ref");

	constexpr auto hasInit =
			requires(Type *pRet, Args &&...args) { pRet->init(sprt::forward<Args>(args)...); };

	constexpr auto hasNew =
			requires(Args &&...args) { RefAlloc::__new<Type>(sprt::forward<Args>(args)...); };

	static_assert(hasInit || hasNew,
			"Fail to detect Type::init(...) or Type(...) with arguments provided");

	if constexpr (hasInit) {
		auto pRet = RefAlloc::__new<Type>();
		if (pRet->init(sprt::forward<Args>(args)...)) {
			return Self(pRet, true); // unsafe assignment
		} else {
			__delete(pRet);
			return Self(nullptr);
		}
	} else if constexpr (hasNew) {
		auto pRet = RefAlloc::__new<Type>(sprt::forward<Args>(args)...);
		return Self(pRet, true); // unsafe assignment
	} else {
		static_assert(false);
		return nullptr;
	}
}

template <typename _Base>
inline auto Rc<_Base>::alloc() -> Self {
	static_assert(is_base_of<Ref, _Base>::value, "Rc base class should be derived from Ref");
	return Self(RefAlloc::__new<Type>(), true);
}

template <typename _Base>
template <typename... Args>
inline auto Rc<_Base>::alloc(Args &&...args) -> Self {
	static_assert(is_base_of<Ref, _Base>::value, "Rc base class should be derived from Ref");
	return Self(RefAlloc::__new<Type>(sprt::forward<Args>(args)...), true);
}

template <typename _Base>
template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type *>
inline auto Rc<_Base>::operator=(const Rc<B> &value) noexcept -> Rc & {
	this->set(value);
	return *this;
}

template <typename _Base>
template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type *>
inline auto Rc<_Base>::operator=(Rc<B> &&value) noexcept -> Rc & {
	this->_ptr = static_cast<Type *>(value._ptr);
	value._ptr = nullptr;
#if SPRT_REF_DEBUG
	this->_id = value._id;
	value._id = 0;
#endif
	return *this;
}

template <typename _Base>
template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type *>
inline auto Rc<_Base>::operator=(NotNull<B> value) noexcept -> Rc & {
	this->set(value.get());
	return *this;
}

// upcast
template <typename _Base>
template <typename B, typename enable_if<is_convertible_v<B *, _Base *>>::type *>
inline auto Rc<_Base>::get_cast() const noexcept -> B * {
	return static_cast<B *>(this->_ptr);
}

template <typename _Base>
inline auto Rc<_Base>::get() const noexcept -> _Base * {
#if SPRT_REF_DEBUG
	sprt_passert(this->_ptr, "Rc should be initialized when dereferenced");
#endif
	return this->_ptr;
}

template <typename _Base>
inline Rc<_Base>::operator _Base *() const noexcept {
	return *this ? get() : nullptr;
}

template <typename _Base>
inline Rc<_Base>::operator NotNull<_Base>() const noexcept {
	auto ptr = get();
	sprt_passert(ptr, "");
	return ptr;
}

template <typename _Base>
template <typename B, typename enable_if<is_convertible_v<_Base *, B *>>::type *>
inline Rc<_Base>::operator NotNull<B>() const noexcept {
	auto ptr = get();
	sprt_passert(ptr, "");
	return ptr;
}

template <typename _Base>
inline auto Rc<_Base>::operator->() const noexcept -> _Base * {
	return this->_ptr;
}

template <typename _Base>
inline Rc<_Base>::operator bool() const noexcept {
	return this->_ptr != nullptr;
}

template <typename _Base>
template <typename B, typename enable_if<is_convertible_v<_Base *, B *>>::type *>
inline Rc<_Base>::operator Rc<B>() noexcept {
	return Rc<B>(static_cast<B *>(get()));
}

template <typename _Base>
template <typename Target>
inline Rc<Target> Rc<_Base>::cast() const {
	if (auto v = dynamic_cast<Target *>(this->_ptr)) {
		return Rc<Target>(v);
	}
	return Rc<Target>(nullptr);
}

template <typename _Base>
template <typename... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::create(Args &&...args) {
	auto pRet = Type::create();
	Self ret(nullptr);
	pRet->perform([&](_Base *base) {
		if (base->init(sprt::forward<Args>(args)...)) {
			ret = Self(pRet, true); // unsafe assignment
		}
	});
	if (!ret) {
		__delete(pRet);
	}
	return ret;
}

template <typename _Base>
template <typename... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::create(memory::pool_t *pool,
		Args &&...args) {
	auto pRet = Type::create(pool);
	Self ret(nullptr);
	pRet->perform([&](_Base *base) {
		if (base->init(sprt::forward<Args>(args)...)) {
			ret = Self(pRet, true); // unsafe assignment
		}
	});
	if (!ret) {
		__delete(pRet);
	}
	return ret;
}

template <typename _Base>
template <typename... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::create(SharedRefMode mode,
		Args &&...args) {
	auto pRet = Type::create(mode);
	Self ret(nullptr);
	pRet->perform([&](_Base *base) {
		if (base->init(sprt::forward<Args>(args)...)) {
			ret = Self(pRet, true); // unsafe assignment
		}
	});
	if (!ret) {
		__delete(pRet);
	}
	return ret;
}

template <typename _Base>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc() {
	return Self(Type::create(), true);
}

template <typename _Base>
template <typename... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc(Args &&...args) {
	return Self(Type::create(sprt::forward<Args>(args)...), true);
}

template <typename _Base>
template <typename... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc(memory::pool_t *pool,
		Args &&...args) {
	return Self(Type::create(pool, sprt::forward<Args>(args)...), true);
}

template <typename _Base>
template <typename... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc(SharedRefMode mode,
		Args &&...args) {
	return Self(Type::create(mode, sprt::forward<Args>(args)...), true);
}


template <typename T, typename... Args>
inline T *__new(Args &&...args) noexcept {
	return RefAlloc::__new<T>(sprt::forward<Args>(args)...);
}

template <typename T>
inline uint64_t retain(T *t, uint64_t value) noexcept {
	return t->retain(value);
}

template <typename T>
inline void release(T *t, uint64_t value) noexcept {
	if (t->release(value)) {
		if constexpr (__is_shared_ref<T>::value) {
			T::__delete(t);
		} else {
			__delete(t);
		}
	}
}

template <typename T>
inline uint64_t retain(const Rc<T> &t, uint64_t value = Max<uint64_t>) noexcept {
	return sprt::retain(t.get(), value);
}

template <typename T>
inline void release(const Rc<T> &t, uint64_t value) noexcept {
	sprt::release(t.get(), value);
}

template <typename T>
inline uint64_t retain(const NotNull<T> &t, uint64_t value = Max<uint64_t>) noexcept {
	return sprt::retain(t.get(), value);
}

template <typename T>
inline void release(const NotNull<T> &t, uint64_t value) noexcept {
	sprt::release(t.get(), value);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_REF_H_
