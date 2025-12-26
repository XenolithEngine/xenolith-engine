/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_UTILS_SPREF_H_
#define STAPPLER_CORE_UTILS_SPREF_H_

#include "SPCore.h"
#include "SPTime.h"

// enable Ref debug mode to track retain/release sources
#ifndef SP_REF_DEBUG
#define SP_REF_DEBUG 0
#endif

// WinCRT instantiates Rc's in place of definition, not actual usage
// So, we need to safely call Ref's methods on classes with only forward declarations visible
#if WIN32
#define SP_REF_SAFE_INSTANIATION 1
#else
#define SP_REF_SAFE_INSTANIATION 0
#endif

namespace STAPPLER_VERSIONIZED stappler {

// Reference allocation base class
// References use thread-safe reference counting mechanism
// Reference object will be removed, when _referenceCount decremented to zero
//
// Reference object can be allocated from memory pool, in this case memory will be freed only when memory pool is destroyed

class SP_PUBLIC RefAlloc : public memory::StandartInterface::AllocBaseType {
public:
	static constexpr uint32_t PoolAllocBit = 0x8000'0000;

	static void *operator new(size_t size, const std::nothrow_t &tag) noexcept {
		return ::operator new(size, tag);
	}
	static void *operator new(size_t size, std::align_val_t al,
			const std::nothrow_t &tag) noexcept {
		return ::operator new(size, al, tag);
	}
	static void *operator new(size_t size, void *ptr) noexcept { return ::operator new(size, ptr); }
	static void *operator new(size_t size, memory::pool_t *ptr) noexcept;

	static void operator delete(void *ptr) noexcept;
	static void operator delete(void *ptr, std::align_val_t al) noexcept;

	virtual ~RefAlloc();

	uint32_t getReferenceCount() const noexcept;
	bool isPoolAllocated() const noexcept;

protected:
	RefAlloc() noexcept;

	// DO NOT USE THIS DIRECTLY, use Ref::retain
	void incrementReferenceCount();

	// DO NOT USE THIS DIRECTLY, use Ref::release
	bool decrementReferenceCount();

	// use this function to destroy memory pool, that contains ref itself
	void destroySelfContained(memory::pool_t *);

	// use this function to destroy memory allocator, that contains ref itself
	void destroySelfContained(memory::allocator_t *);

	std::atomic<uint32_t> _referenceCount = 1;
};

class SP_PUBLIC Ref : public RefAlloc {
public:
	virtual ~Ref() = default;

	// You can assign an unique number for retain/release sequence to track leaked references
	// In this case, release should be called with the same id, as returned from retain.
	// Retain argument can be used as designated id, or maxOf<uint64_t>() to allocate unique id
	// If SP_REF_DEBUG is not enabled - it's noop

#if SP_REF_DEBUG
	// In SP_REF_DEBUG mode, you can override retain or release
	virtual uint64_t retain(uint64_t value = maxOf<uint64_t>());
	virtual void release(uint64_t id);

	void foreachBacktrace(
			const Callback<void(uint64_t, Time, const std::vector<std::string> &)> &) const;
#else
	uint64_t retain(uint64_t value = maxOf<uint64_t>()) {
		(void)value;
		incrementReferenceCount();
		return 0;
	}
	void release(uint64_t id) {
		if (decrementReferenceCount()) {
			delete this;
		}
	}
#endif

protected:
	Ref() noexcept : RefAlloc() { }

	// override this method to enable automatic retain/release tracking for subclass
#if SP_REF_DEBUG
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
class SP_PUBLIC SharedRef : public Ref {
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
	//#if SP_REF_DEBUG
	//	virtual bool isRetainTrackerEnabled() const { return true; }
	//#endif

	SharedRef(SharedRefMode m, memory::allocator_t *, memory::pool_t *) noexcept;

	memory::allocator_t *_allocator = nullptr;
	memory::pool_t *_pool = nullptr;
	memory::pool_t *_parent = nullptr;
	T *_shared = nullptr;
	SharedRefMode _mode = SharedRefMode::Pool;
};

template <typename _Base, typename _Pointer>
class RcBase {
public:
	using Base = _Base;
	using Type = typename std::remove_cv<_Base>::type;
	using Pointer = _Pointer;

	RcBase() noexcept;
	RcBase(const nullptr_t &) noexcept;
	RcBase(const Pointer &value) noexcept;
	RcBase(const RcBase<Base, Pointer> &v) noexcept;
	RcBase(RcBase<Base, Pointer> &&v) noexcept;

	RcBase &operator=(const nullptr_t &) noexcept;

	RcBase &operator=(const Pointer &value) noexcept;
	RcBase &operator=(const NotNull<std::remove_pointer_t<Pointer>> &value) noexcept;
	RcBase &operator=(const RcBase<Base, Pointer> &v) noexcept;
	RcBase &operator=(RcBase<Base, Pointer> &&v) noexcept;

	~RcBase();

	void set(const Pointer &value);
	void swap(RcBase<Base, Pointer> &v);
	void clear();

	bool operator==(const RcBase<Base, Pointer> &other) const;
	bool operator==(const Base *&other) const;
	bool operator==(typename std::remove_const<Base>::type *other) const;
	bool operator==(const std::nullptr_t other) const;

	bool operator!=(const RcBase<Base, Pointer> &other) const;
	bool operator!=(const Base *&other) const;
	bool operator!=(typename std::remove_const<Base>::type *other) const;
	bool operator!=(const std::nullptr_t other) const;

	bool operator>(const RcBase<Base, Pointer> &other) const;
	bool operator>(const Base *other) const;
	bool operator>(typename std::remove_const<Base>::type *other) const;
	bool operator>(const std::nullptr_t other) const;

	bool operator<(const RcBase<Base, Pointer> &other) const;
	bool operator<(const Base *other) const;
	bool operator<(typename std::remove_const<Base>::type *other) const;
	bool operator<(const std::nullptr_t other) const;

	bool operator>=(const RcBase<Base, Pointer> &other) const;
	bool operator>=(const Base *other) const;
	bool operator>=(typename std::remove_const<Base>::type *other) const;
	bool operator>=(const std::nullptr_t other) const;

	bool operator<=(const RcBase<Base, Pointer> &other) const;
	bool operator<=(const Base *other) const;
	bool operator<=(typename std::remove_const<Base>::type *other) const;
	bool operator<=(const std::nullptr_t other) const;

#if SP_REF_DEBUG
	uint64_t getId() const { return _id; }
#endif
protected:
	inline void doRetain();
	inline void doRelease();

	inline Pointer doSwap(Pointer value);

	// unsafe assignment
	inline RcBase(Pointer value, bool v) noexcept;

	Pointer _ptr = nullptr;
#if SP_REF_DEBUG
	uint64_t _id = 0;
#endif
};

template <typename _Base>
class Rc final : public RcBase<_Base, typename std::remove_cv<_Base>::type *> {
public:
	using Parent = RcBase<_Base, typename std::remove_cv<_Base>::type *>;
	using Self = Rc<_Base>;
	using Type = typename std::remove_cv<_Base>::type;

	template <typename Target>
	static auto doReferenceCast(Rc<Type> &&source) -> Rc<Target>;

	template <class... Args>
	static inline Self create(Args &&...args);

	static inline Self alloc();

	template <class... Args>
	static inline Self alloc(Args &&...args);

	using Parent::Parent;
	using Parent::operator=;

	template <typename B,
			typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type * = nullptr>
	inline Rc &operator=(const Rc<B> &value) noexcept;

	template <typename B,
			typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type * = nullptr>
	inline Rc &operator=(Rc<B> &&value) noexcept;

	template <typename B,
			typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type * = nullptr>
	Rc &operator=(NotNull<B>) noexcept;

	// upcast
	template <typename B,
			typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type * = nullptr>
	B *get_cast() const noexcept;

	// Direct call of `get` should not be on empty storage
	_Base *get() const noexcept;

	operator _Base *() const noexcept;

	operator NotNull<_Base>() const noexcept;

	template <typename B,
			typename std::enable_if<std::is_convertible<_Base *, B *>{}>::type * = nullptr>
	operator NotNull<B>() const noexcept;

	_Base *operator->() const noexcept;

	explicit operator bool() const noexcept;

	template <typename B,
			typename std::enable_if<std::is_convertible<_Base *, B *>{}>::type * = nullptr>
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

	template <class... Args>
	static Self create(Args &&...args);

	template <class... Args>
	static Self create(memory::pool_t *pool, Args &&...args);

	template <class... Args>
	static Self create(SharedRefMode mode, Args &&...args);

	static Self alloc();

	template <class... Args>
	static Self alloc(Args &&...args);

	template <class... Args>
	static Self alloc(memory::pool_t *pool, Args &&...args);

	template <class... Args>
	static Self alloc(SharedRefMode mode, Args &&...args);

	using Parent::Parent;
	using Parent::operator=;

	// Direct call of `get` should not be on empty storage
	_Base *get() const noexcept {
#if SP_REF_DEBUG
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
		typename std::enable_if<std::is_convertible_v<Target *, Source *>
				|| std::is_convertible_v<Source *, Target *>>::type * = nullptr>
auto ref_cast(Rc<Source> &&source) -> Rc<Target> {
	return Rc<Source>::template doReferenceCast<Target>(move(source));
}

template <typename Target, typename Source,
		typename std::enable_if<std::is_convertible_v<Target *, Source *>
				|| std::is_convertible_v<Source *, Target *>>::type * = nullptr>
auto ref_cast(const Rc<Source> &source) -> Rc<Target> {
	return Rc<Source>::template doReferenceCast<Target>(source.get());
}

namespace memleak {

SP_PUBLIC uint64_t getNextRefId();

SP_PUBLIC uint64_t retainBacktrace(const Ref *, uint64_t = maxOf<uint64_t>());
SP_PUBLIC void releaseBacktrace(const Ref *, uint64_t);
SP_PUBLIC void foreachBacktrace(const Ref *,
		const Callback<void(uint64_t, Time, const std::vector<std::string> &)> &);

} // namespace memleak


//
// Implementation
//

inline uint32_t RefAlloc::getReferenceCount() const noexcept {
	return _referenceCount.load() & (~PoolAllocBit);
}

inline bool RefAlloc::isPoolAllocated() const noexcept {
	return (_referenceCount.load() & PoolAllocBit) == PoolAllocBit;
}

// DO NOT USE THIS DIRECTLY, use Ref::retain
inline void RefAlloc::incrementReferenceCount() { ++_referenceCount; }

// DO NOT USE THIS DIRECTLY, use Ref::release
inline bool RefAlloc::decrementReferenceCount() {
	if ((_referenceCount.fetch_sub(1) & (~PoolAllocBit)) == 1) {
		return true;
	}
	return false;
}

template <typename T>
template <typename... Args>
auto SharedRef<T>::create(Args &&...args) -> SharedRef * {
	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	SharedRef *shared = nullptr;
	memory::perform([&] {
		shared = new (pool) SharedRef(SharedRefMode::Pool, nullptr, pool);
		if (shared) {
			shared->_shared = new (pool) T(shared, pool, std::forward<Args>(args)...);
		}
	}, pool);
	return shared;
}

template <typename T>
template <typename... Args>
auto SharedRef<T>::create(memory::pool_t *p, Args &&...args) -> SharedRef * {
	auto pool = memory::pool::create(p);

	SharedRef *shared = nullptr;
	memory::perform([&] {
		shared = new (pool) SharedRef(SharedRefMode::Pool, nullptr, pool);
		if (shared) {
			shared->_shared = new (pool) T(shared, pool, std::forward<Args>(args)...);
		}
		if (p) {
			shared->_parent = p;
			memory::pool::pre_cleanup_register(shared->_parent, shared, &invaldate);
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
	case SharedRefMode::Pool: pool = memory::pool::create((memory::pool_t *)nullptr); break;
	case SharedRefMode::Allocator:
		alloc = memory::allocator::create();
		pool = memory::pool::create(alloc);
		break;
	}

	SharedRef *shared = nullptr;
	memory::perform([&] {
		shared = new (pool) SharedRef(mode, alloc, pool);
		if (shared) {
			shared->_shared = new (pool) T(shared, pool, std::forward<Args>(args)...);
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
		memory::perform([&, this] { delete _shared; }, _pool);
		_shared = nullptr;
	}

	auto pool = _pool;
	auto allocator = _allocator;
	auto parent = _parent;

	if (parent) {
		memory::pool::cleanup_kill(parent, this, &invaldate);
		parent = nullptr;
	}

	_parent = nullptr;
	_pool = nullptr;
	_allocator = nullptr;

	if (pool) {
		destroySelfContained(pool);
	}
	if (allocator) {
		destroySelfContained(allocator);
	}
}

template <typename T>
template <typename Callback>
void SharedRef<T>::perform(Callback &&cb) {
	if constexpr (std::is_invocable_v<Callback, memory::pool_t *, T *>) {
		memory::perform([&, this] { cb(_pool, _shared); }, _pool);
	} else {
		static_assert(std::is_invocable_v<Callback, T *>, "Invalid callback type");
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
#if SP_REF_DEBUG
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
		const NotNull<std::remove_pointer_t<Pointer>> &value) noexcept -> RcBase & {
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
#if SP_REF_DEBUG
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
	std::swap(_ptr, v._ptr);
#if SP_REF_DEBUG
	std::swap(_id, v._id);
#endif
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::clear() {
	doRelease();
	_ptr = nullptr;
#if SP_REF_DEBUG
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
inline bool RcBase<_Base, _Pointer>::operator==(
		typename std::remove_const<Base>::type *other) const {
	return _ptr == other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator==(const std::nullptr_t other) const {
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
inline bool RcBase<_Base, _Pointer>::operator!=(
		typename std::remove_const<Base>::type *other) const {
	return _ptr != other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator!=(const std::nullptr_t other) const {
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
inline bool RcBase<_Base, _Pointer>::operator>(
		typename std::remove_const<Base>::type *other) const {
	return _ptr > other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>(const std::nullptr_t other) const {
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
inline bool RcBase<_Base, _Pointer>::operator<(
		typename std::remove_const<Base>::type *other) const {
	return _ptr < other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<(const std::nullptr_t other) const {
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
inline bool RcBase<_Base, _Pointer>::operator>=(
		typename std::remove_const<Base>::type *other) const {
	return _ptr >= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator>=(const std::nullptr_t other) const {
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
inline bool RcBase<_Base, _Pointer>::operator<=(
		typename std::remove_const<Base>::type *other) const {
	return _ptr <= other;
}

template <typename _Base, typename _Pointer>
inline bool RcBase<_Base, _Pointer>::operator<=(const std::nullptr_t other) const {
	return _ptr <= other;
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::doRetain() {
#if SP_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
#else
	auto ptr = _ptr;
#endif

#if SP_REF_DEBUG
	if (ptr) {
		_id = ptr->retain();
	}
#else
	if (ptr) {
		ptr->retain();
	}
#endif
}

template <typename _Base, typename _Pointer>
inline void RcBase<_Base, _Pointer>::doRelease() {
#if SP_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
#else
	auto ptr = _ptr;
#endif

#if SP_REF_DEBUG
	if (ptr) {
		ptr->release(_id);
	}
#else
	if (ptr) {
		ptr->release(0);
	}
#endif
}

template <typename _Base, typename _Pointer>
inline auto RcBase<_Base, _Pointer>::doSwap(Pointer _value) -> Pointer {
#if SP_REF_SAFE_INSTANIATION
	auto ptr = (Ref *)_ptr;
	auto value = (Ref *)_value;
#else
	auto ptr = _ptr;
	auto value = _value;
#endif

#if SP_REF_DEBUG
	uint64_t id = 0;
	if (value) {
		id = value->retain();
	}
	if (ptr) {
		ptr->release(_id);
	}
	_id = id;
	return (Pointer)value;
#else
	if (value) {
		value->retain();
	}
	if (ptr) {
		ptr->release(0);
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
#if SP_REF_DEBUG
	ret._id = source._id;
	source._id = 0;
#endif
	return ret;
}

template <typename _Base>
template <class... Args>
inline auto Rc<_Base>::create(Args &&...args) -> Self {
	static_assert(std::is_base_of<Ref, _Base>::value, "Rc base class should be derived from Ref");

	if constexpr (requires(Type *pRet, Args &&...args) {
					  pRet->init(std::forward<Args>(args)...);
				  }) {
		auto pRet = new (std::nothrow) Type();
		if (pRet->init(std::forward<Args>(args)...)) {
			return Self(pRet, true); // unsafe assignment
		} else {
			delete pRet;
			return Self(nullptr);
		}
	} else if constexpr (requires(Args &&...args) {
							 new (std::nothrow) Type(std::forward<Args>(args)...);
						 }) {
		auto pRet = new (std::nothrow) Type(std::forward<Args>(args)...);
		return Self(pRet, true); // unsafe assignment
	} else {
		static_assert(false, "Fail to detect Type::init(...) or Type(...) with arguments provided");
		return nullptr;
	}
}

template <typename _Base>
inline auto Rc<_Base>::alloc() -> Self {
	static_assert(std::is_base_of<Ref, _Base>::value, "Rc base class should be derived from Ref");
	return Self(new (std::nothrow) Type(), true);
}

template <typename _Base>
template <class... Args>
inline auto Rc<_Base>::alloc(Args &&...args) -> Self {
	static_assert(std::is_base_of<Ref, _Base>::value, "Rc base class should be derived from Ref");
	return Self(new (std::nothrow) Type(std::forward<Args>(args)...), true);
}

template <typename _Base>
template <typename B, typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type *>
inline auto Rc<_Base>::operator=(const Rc<B> &value) noexcept -> Rc & {
	this->set(value);
	return *this;
}

template <typename _Base>
template <typename B, typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type *>
inline auto Rc<_Base>::operator=(Rc<B> &&value) noexcept -> Rc & {
	this->_ptr = static_cast<Type *>(value._ptr);
	value._ptr = nullptr;
#if SP_REF_DEBUG
	this->_id = value._id;
	value._id = 0;
#endif
	return *this;
}

template <typename _Base>
template <typename B, typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type *>
inline auto Rc<_Base>::operator=(NotNull<B> value) noexcept -> Rc & {
	this->set(value.get());
	return *this;
}

// upcast
template <typename _Base>
template <typename B, typename std::enable_if<std::is_convertible<B *, _Base *>{}>::type *>
inline auto Rc<_Base>::get_cast() const noexcept -> B * {
	return static_cast<B *>(this->_ptr);
}

template <typename _Base>
inline auto Rc<_Base>::get() const noexcept -> _Base * {
#if SP_REF_DEBUG
	assert(this->_ptr);
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
template <typename B, typename std::enable_if<std::is_convertible<_Base *, B *>{}>::type *>
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
template <typename B, typename std::enable_if<std::is_convertible<_Base *, B *>{}>::type *>
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
template <class... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::create(Args &&...args) {
	auto pRet = Type::create();
	Self ret(nullptr);
	pRet->perform([&](_Base *base) {
		if (base->init(std::forward<Args>(args)...)) {
			ret = Self(pRet, true); // unsafe assignment
		}
	});
	if (!ret) {
		delete pRet;
	}
	return ret;
}

template <typename _Base>
template <class... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::create(memory::pool_t *pool,
		Args &&...args) {
	auto pRet = Type::create(pool);
	Self ret(nullptr);
	pRet->perform([&](_Base *base) {
		if (base->init(std::forward<Args>(args)...)) {
			ret = Self(pRet, true); // unsafe assignment
		}
	});
	if (!ret) {
		delete pRet;
	}
	return ret;
}

template <typename _Base>
template <class... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::create(SharedRefMode mode,
		Args &&...args) {
	auto pRet = Type::create(mode);
	Self ret(nullptr);
	pRet->perform([&](_Base *base) {
		if (base->init(std::forward<Args>(args)...)) {
			ret = Self(pRet, true); // unsafe assignment
		}
	});
	if (!ret) {
		delete pRet;
	}
	return ret;
}

template <typename _Base>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc() {
	return Self(Type::create(), true);
}

template <typename _Base>
template <class... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc(Args &&...args) {
	return Self(Type::create(std::forward<Args>(args)...), true);
}

template <typename _Base>
template <class... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc(memory::pool_t *pool,
		Args &&...args) {
	return Self(Type::create(pool, std::forward<Args>(args)...), true);
}

template <typename _Base>
template <class... Args>
inline typename Rc<SharedRef<_Base>>::Self Rc<SharedRef<_Base>>::alloc(SharedRefMode mode,
		Args &&...args) {
	return Self(Type::create(mode, std::forward<Args>(args)...), true);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_UTILS_REF_SPREF_H_ */
