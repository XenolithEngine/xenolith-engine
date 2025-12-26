/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_MEMORY_SPMEMFUNCTION_H_
#define STAPPLER_CORE_MEMORY_SPMEMFUNCTION_H_

#include "detail/SPMemAlloc.h"

namespace STAPPLER_VERSIONIZED stappler::memory {

// Function - реализация std::function, использующая память из pool_t
// some sources from https://github.com/prograholic/blog/blob/master/cxx_function/main.cpp

template <typename, typename, typename = void>
struct check_signature : std::false_type { };

template <typename Func, typename Ret, typename... Args>
struct check_signature<Func, Ret(Args...),
		typename std::enable_if<
				std::is_convertible< decltype(std::declval<Func>()(std::declval<Args>()...)),
						Ret >::value,
				void>::type> : std::true_type { };

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
			class = typename std::enable_if<!std::is_same<
					typename std::remove_cv<typename std::remove_reference<FunctionT>::type>::type,
					function<ReturnType(ArgumentTypes...) >>::value>::type>
	function(FunctionT &&f, const allocator_type &alloc = allocator_type()) noexcept
	: mAllocator(alloc) {
		mCallback = makeFreeFunction(std::forward<FunctionT>(f), mAllocator, mBuffer.data());
	}

	template <typename FunctionT>
	function &operator=(FunctionT &&f) noexcept {
		clear();
		mCallback = makeFreeFunction(std::forward<FunctionT>(f), mAllocator, mBuffer.data());
		return *this;
	}

	ReturnType operator()(ArgumentTypes... args) const {
		return mCallback()->invoke(mBuffer.data(), std::forward<ArgumentTypes>(args)...);
	}

	constexpr bool operator==(nullptr_t) const noexcept { return mCallback == nullptr; }

	constexpr bool operator!=(nullptr_t) const noexcept { return mCallback != nullptr; }

	constexpr explicit operator bool() const noexcept { return mCallback != nullptr; }

	constexpr bool operator==(const function &other) const noexcept {
		return mAllocator == other.mAllocator && mCallback == other.mCallback
				&& mBuffer == other.mBuffer;
	}

	constexpr bool operator!=(const function &other) const noexcept {
		return mAllocator != other.mAllocator || mCallback != other.mCallback
				|| mBuffer != other.mBuffer;
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
		using BaseType =
				typename std::remove_cv<typename std::remove_reference<FunctionT>::type>::type;
		using BaseTypePtr = BaseType *;
		if constexpr (sizeof(BaseType) <= OptBufferSize) {
			static functor_traits traits{
				[](const void *arg, ArgumentTypes... args) -> ReturnType {
				return (*static_cast<const BaseType *>(arg))(std::forward<ArgumentTypes>(args)...);
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
				return new (buf) BaseType(sp::move_unsafe(*static_cast<BaseType *>(arg)));
			},
			};

			return &traits;
		} else {
			static functor_traits traits{
				[](const void *arg, ArgumentTypes... args) -> ReturnType {
				return (*(*static_cast<const BaseTypePtr *>(arg)))(
						std::forward<ArgumentTypes>(args)...);
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
		using BaseType =
				typename std::remove_cv<typename std::remove_reference<FunctionT>::type>::type;
		if constexpr (sizeof(BaseType) <= OptBufferSize) {
			new (buf) BaseType(std::forward<FunctionT>(f));
		} else {
			detail::Allocator<BaseType> ialloc = alloc;
			auto mem = ialloc.allocate(1);

			memory::perform_conditional([&] { new (mem) BaseType(std::forward<FunctionT>(f)); },
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
	std::array<uint8_t, OptBufferSize> mBuffer;
};


template <typename UnusedType>
class callback;

// Modern version. inspired by http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html
template <typename ReturnType, typename... ArgumentTypes>
class callback<ReturnType(ArgumentTypes...)> : public AllocPool {
public:
	using signature_type = ReturnType(ArgumentTypes...);

	~callback() {
		// functor is not owned, so, no cleanup
	}

	callback(nullptr_t) noexcept : mFunctor(nullptr), mCallback(nullptr) { }

	callback &operator=(nullptr_t) noexcept {
		mFunctor = nullptr;
		mCallback = nullptr;
	}

	template <typename FunctionT>
	callback(const FunctionT &f) noexcept
	: mFunctor(&f), mCallback([](const void *arg, ArgumentTypes... args) {
		return (*static_cast<const FunctionT *>(arg))(std::forward<ArgumentTypes>(args)...);
	}) { }

	template <typename FunctionT>
	callback &operator=(const FunctionT &f) noexcept {
		mFunctor = &f;
		mCallback = [](const void *arg, ArgumentTypes... args) {
			return (*static_cast<const FunctionT *>(arg))(std::forward<ArgumentTypes>(args)...);
		};
	}

	template <typename FunctionType, typename ClassType>
	callback(FunctionType ClassType::*f) noexcept : mFunctor(f) {
		mCallback = makeMemberFunction<FunctionType, ArgumentTypes...>(f);
	}

	template <typename FunctionType, typename ClassType>
	callback &operator=(FunctionType ClassType::*f) noexcept {
		mFunctor = f;
		mCallback = makeMemberFunction<FunctionType, ArgumentTypes...>(f);
	}

	callback(const callback &other) noexcept = delete;
	callback &operator=(const callback &other) noexcept = delete;

	callback(callback &&other) noexcept = delete;
	callback &operator=(callback &&other) noexcept = delete;

	ReturnType operator()(ArgumentTypes... args) const {
		return mCallback(mFunctor, std::forward<ArgumentTypes>(args)...);
	}

	constexpr bool operator==(nullptr_t) const noexcept {
		return mFunctor == nullptr || mCallback == nullptr;
	}

	constexpr bool operator!=(nullptr_t) const noexcept {
		return mFunctor != nullptr && mCallback != nullptr;
	}

	constexpr explicit operator bool() const noexcept {
		return mFunctor != nullptr && mCallback != nullptr;
	}

	template <typename T, typename R, typename... Args>
	friend const callback<R(Args...)> &operator<<(const callback<R(Args...)> &, const T &);

private:
	using FunctionPointer = ReturnType (*)(const void *, ArgumentTypes...);

	template <typename FunctionType, typename ClassType, typename... RestArgumentTypes>
	static FunctionPointer makeMemberFunction(FunctionType ClassType::*f) {
		return [](const void *arg, ClassType &&obj,
					   RestArgumentTypes... args) { // note thunk is captureless
			return (obj.*static_cast<const FunctionType ClassType::*>(arg))(
					std::forward<RestArgumentTypes>(args)...);
		};
	}

	const void *mFunctor;
	FunctionPointer mCallback;
};

template <typename ReturnType, size_t BufferSize, size_t Alignment, typename... ArgumentTypes>
class callback_storage : public callback<ReturnType(ArgumentTypes...)> {
public:
	static constexpr size_t _BufferSize = BufferSize;
	static constexpr size_t _Alignment = Alignment;

	template <typename FunctionT>
	callback_storage(FunctionT &&f)
	: callback<ReturnType(ArgumentTypes...)>(setFunction(std::forward<FunctionT>(f))) {
		static_assert(sizeof(FunctionT) == BufferSize && alignof(FunctionT) == Alignment,
				"BufferSize and Alignment should match with stored type");

		_destroyPointer = makeFunctionTraits<FunctionT>()->destroy;
	}

	~callback_storage() { _destroyPointer(_storage.buf); }

protected:
	using destroy_pointer = void (*)(void *);

	struct functor_traits {
		destroy_pointer destroy;
	};

	template <typename FunctionT>
	static functor_traits *makeFunctionTraits() noexcept {
		using BaseType =
				typename std::remove_cv<typename std::remove_reference<FunctionT>::type>::type;
		static functor_traits traits{[](void *arg) {
			if (arg) {
				(*static_cast<BaseType *>(arg)).~BaseType();
			}
		}};
		return &traits;
	}

	template <typename FunctionT>
	auto setFunction(FunctionT &&f) -> FunctionT & {
		static_assert(sizeof(FunctionT) == BufferSize && alignof(FunctionT) == Alignment,
				"BufferSize and Alignment should match with stored type");
		new ((void *)_storage.buf) FunctionT(std::forward<FunctionT>(f));
		return *reinterpret_cast<FunctionT *>(_storage.buf);
	}

	template <typename FunctionT>
	auto getFunction() -> FunctionT & {
		static_assert(sizeof(FunctionT) == BufferSize && alignof(FunctionT) == Alignment,
				"BufferSize and Alignment should match with stored type");

		return *reinterpret_cast<FunctionT *>(_storage.buf);
	}

	struct alignas(Alignment) storage {
		uint8_t buf[BufferSize];
	};

	storage _storage;
	destroy_pointer _destroyPointer;
};

template <typename Function>
struct callback_traits;

template <typename ClassType, typename ReturnType, typename... Args>
struct callback_traits<ReturnType (ClassType::*)(Args...) const> {
	using type = callback_storage<ReturnType, sizeof(ClassType), alignof(ClassType), Args...>;
};

template <typename ClassType, typename ReturnType, typename... Args>
struct callback_traits<ReturnType (ClassType::*)(Args...)> {
	using type = callback_storage<ReturnType, sizeof(ClassType), alignof(ClassType), Args...>;
};

template <typename T>
inline auto makeCallback(T &&t) ->
		typename std::enable_if<!std::is_function<T>::value && !std::is_bind_expression<T>::value,
				typename callback_traits<decltype(&T::operator())>::type>::type {
	using Type = typename callback_traits<decltype(&T::operator())>::type;

	return Type(std::forward<T>(t));
}

template <typename Sig>
inline auto makeCallback(const std::function<Sig> &fn) {
	return callback<Sig>(fn);
}

template <typename Sig>
inline auto makeCallback(const memory::function<Sig> &fn) {
	return callback<Sig>(fn);
}

} // namespace stappler::memory

#endif /* STAPPLER_CORE_MEMORY_SPMEMFUNCTION_H_ */
