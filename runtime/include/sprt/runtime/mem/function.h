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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_FUNCTION_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_FUNCTION_H_

#include <sprt/runtime/mem/detail/alloc.h>
#include <sprt/runtime/array.h>
#include <sprt/runtime/callback.h>
#include <sprt/c/__sprt_string.h>

namespace sprt::memory {

// Function - реализация std::function, использующая память из pool_t
// some sources from https://github.com/prograholic/blog/blob/master/cxx_function/main.cpp

template <typename, typename, typename = void>
struct check_signature : false_type { };

template <typename Func, typename Ret, typename... Args>
struct check_signature<Func, Ret(Args...),
		typename enable_if<
				is_convertible< decltype(declval<Func>()(declval<Args>()...)), Ret >::value,
				void>::type> : true_type { };

template <typename UnusedType>
class function;

template <typename ReturnType, typename... ArgumentTypes>
class function<ReturnType(ArgumentTypes...)> : public AllocPool {
public:
	using signature_type = ReturnType(ArgumentTypes...);
	using allocator_type = detail::Allocator<void *>;

	~function() { clear(); }

	function(const allocator_type &alloc = allocator_type()) noexcept
	: mAllocator(alloc), mCallback(nullptr) { }

	function(nullptr_t, const allocator_type &alloc = allocator_type()) noexcept
	: mAllocator(alloc), mCallback(nullptr) { }

	function &operator=(nullptr_t) noexcept {
		clear();
		mCallback = nullptr;
		return *this;
	}

	function(const function &other, const allocator_type &alloc = allocator_type()) noexcept
	: mAllocator(alloc) {
		mCallback = other.mCallback;
		if (mCallback) {
			mCallback()->copy(other.mBuffer.data(), mAllocator, mBuffer.data());
		}
	}

	function &operator=(const function &other) noexcept {
		if (&other == this) {
			return *this;
		}

		clear();
		mCallback = other.mCallback;
		if (mCallback) {
			mCallback()->copy(other.mBuffer.data(), mAllocator, mBuffer.data());
		}
		return *this;
	}

	function(function &&other, const allocator_type &alloc = allocator_type()) noexcept
	: mAllocator(alloc) {
		mCallback = other.mCallback;
		if (mCallback) {
			if (other.mAllocator == mAllocator) {
				mCallback()->move(other.mBuffer.data(), mAllocator, mBuffer.data());
			} else {
				mCallback()->copy(other.mBuffer.data(), mAllocator, mBuffer.data());
			}
		}
	}

	function &operator=(function &&other) noexcept {
		if (&other == this) {
			return *this;
		}

		clear();
		mCallback = other.mCallback;
		if (mCallback) {
			if (other.mAllocator == mAllocator) {
				mCallback()->move(other.mBuffer.data(), mAllocator, mBuffer.data());
			} else {
				mCallback()->copy(other.mBuffer.data(), mAllocator, mBuffer.data());
			}
		}
		return *this;
	}

	template <typename FunctionT,
			class = typename enable_if<
					!is_same< typename remove_cv<typename remove_reference<FunctionT>::type>::type,
							function<ReturnType(ArgumentTypes...) >>::value>::type>
	function(FunctionT &&f, const allocator_type &alloc = allocator_type()) noexcept
	: mAllocator(alloc) {
		mCallback = makeFreeFunction(sprt::forward<FunctionT>(f), mAllocator, mBuffer.data());
	}

	template <typename FunctionT>
	function &operator=(FunctionT &&f) noexcept {
		clear();
		mCallback = makeFreeFunction(sprt::forward<FunctionT>(f), mAllocator, mBuffer.data());
		return *this;
	}

	ReturnType operator()(ArgumentTypes... args) const {
		return mCallback()->invoke(mBuffer.data(), sprt::forward<ArgumentTypes>(args)...);
	}

	constexpr bool operator==(nullptr_t) const noexcept { return mCallback == nullptr; }

	constexpr bool operator!=(nullptr_t) const noexcept { return mCallback != nullptr; }

	constexpr explicit operator bool() const noexcept { return mCallback != nullptr; }

	constexpr bool operator==(const function &other) const noexcept {
		return mAllocator == other.mAllocator && mCallback == other.mCallback
				&& (__sprt_memcmp(mBuffer.data(), other.mBuffer.data(), mBuffer.size()) == 0);
	}

	constexpr bool operator!=(const function &other) const noexcept {
		return mAllocator != other.mAllocator || mCallback != other.mCallback
				|| (__sprt_memcmp(mBuffer.data(), other.mBuffer.data(), mBuffer.size()) != 0);
	}

	const allocator_type &get_allocator() const { return mAllocator; }

private:
	static constexpr auto OptBufferSize = 16;

	using invoke_pointer = ReturnType (*)(const void *, ArgumentTypes...);
	using destroy_pointer = void (*)(void *);
	using copy_pointer = void *(*)(const void *, allocator_type &, uint8_t *);
	using move_pointer = void *(*)(void *, allocator_type &, uint8_t *);

	struct functor_traits {
		invoke_pointer invoke;
		destroy_pointer destroy;
		copy_pointer copy;
		move_pointer move;
	};

	using traits_callback = functor_traits *(*)();

	template <typename FunctionT>
	static functor_traits *makeFunctionTraits() noexcept {
		using BaseType = typename remove_cv<typename remove_reference<FunctionT>::type>::type;
		using BaseTypePtr = BaseType *;
		if constexpr (sizeof(BaseType) <= OptBufferSize) {
			static functor_traits traits{
				[](const void *arg, ArgumentTypes... args) -> ReturnType {
				return (*static_cast<const BaseType *>(arg))(sprt::forward<ArgumentTypes>(args)...);
			},
				[](void *arg) {
				if (arg) {
					(*static_cast<BaseType *>(arg)).~BaseType();
				}
			},
				[](const void *arg, allocator_type &alloc, uint8_t *buf) -> void * {
				return new (buf) BaseType(*static_cast<const BaseType *>(arg));
			},
				[](void *arg, allocator_type &alloc, uint8_t *buf) -> void * {
				return new (buf) BaseType(sprt::move_unsafe(*static_cast<BaseType *>(arg)));
			},
			};

			return &traits;
		} else {
			static functor_traits traits{
				[](const void *arg, ArgumentTypes... args) -> ReturnType {
				return (*(*static_cast<const BaseTypePtr *>(arg)))(
						sprt::forward<ArgumentTypes>(args)...);
			},
				[](void *arg) {
				auto ptr = *static_cast<BaseTypePtr *>(arg);
				if (ptr) {
					ptr->~BaseType();
					new (arg)(const BaseType *)(nullptr);
				}
			},
				[](const void *arg, allocator_type &alloc, uint8_t *buf) -> void * {
				detail::Allocator<BaseType> ialloc = alloc;
				auto mem = ialloc.allocate(1);
				ialloc.construct(mem, *(*static_cast<const BaseTypePtr *>(arg)));
				return new (buf)(const BaseType *)(mem);
			},
				[](void *arg, allocator_type &alloc, uint8_t *buf) -> void * {
				auto ret = new (buf)(const BaseType *)((*static_cast<BaseTypePtr *>(arg)));
				new (arg)(const BaseType *)(nullptr);
				return ret;
			},
			};

			return &traits;
		}
	}

	template <typename FunctionT>
	static traits_callback makeFreeFunction(FunctionT &&f, allocator_type &alloc,
			uint8_t *buf) noexcept {
		using BaseType = typename remove_cv<typename remove_reference<FunctionT>::type>::type;
		if constexpr (sizeof(BaseType) <= OptBufferSize) {
			new (buf) BaseType(sprt::forward<FunctionT>(f));
		} else {
			detail::Allocator<BaseType> ialloc = alloc;
			auto mem = ialloc.allocate(1);

			memory::perform_conditional([&] { new (mem) BaseType(sprt::forward<FunctionT>(f)); },
					alloc);

			new (buf)(const BaseType *)(mem);
		}
		return &makeFunctionTraits<FunctionT>;
	}

	void clear() {
		if (mCallback) {
			auto t = mCallback();
			t->destroy(mBuffer.data());
			mCallback = nullptr;
		}
	}

	allocator_type mAllocator;
	traits_callback mCallback = nullptr;
	array<uint8_t, OptBufferSize> mBuffer;
};

} // namespace sprt::memory

namespace sprt::memory::pool {

SPRT_API void cleanup_register(pool_t *p, memory::function<void()> &&cb);

SPRT_API void pre_cleanup_register(pool_t *p, memory::function<void()> &&cb);

} // namespace sprt::memory::pool

#endif
