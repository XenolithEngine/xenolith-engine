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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMECALLBACK_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMECALLBACK_H_

#include "SPRuntimeInt.h"
#include "SPRuntimeArray.h"
#include <string.h>

namespace sprt {

template <typename UnusedType>
class static_function;

// analogue of std::function without memory allocation with a pre-allocated block for data storage

template <typename ReturnType, typename... ArgumentTypes>
class static_function<ReturnType(ArgumentTypes...)> {
public:
	static constexpr size_t FunctionBufferSize = 32;

	using signature_type = ReturnType(ArgumentTypes...);

	~static_function() { clear(); }

	static_function() noexcept : mCallback(nullptr) { }

	static_function(nullptr_t) noexcept : mCallback(nullptr) { }

	static_function &operator=(nullptr_t) noexcept {
		clear();
		mCallback = nullptr;
		return *this;
	}

	static_function(const static_function &other) noexcept {
		mCallback = other.mCallback;
		if (mCallback) {
			mCallback()->copy(other.mBuffer.data(), mBuffer.data());
		}
	}

	static_function &operator=(const static_function &other) noexcept {
		if (&other == this) {
			return *this;
		}

		clear();
		mCallback = other.mCallback;
		if (mCallback) {
			mCallback()->copy(other.mBuffer.data(), mBuffer.data());
		}
		return *this;
	}

	static_function(static_function &&other) noexcept {
		mCallback = other.mCallback;
		if (mCallback) {
			mCallback()->move(other.mBuffer.data(), mBuffer.data());
		}
	}

	static_function &operator=(static_function &&other) noexcept {
		if (&other == this) {
			return *this;
		}

		clear();
		mCallback = other.mCallback;
		if (mCallback) {
			mCallback()->move(other.mBuffer.data(), mBuffer.data());
		}
		return *this;
	}

	template <typename FunctionT,
			class = typename enable_if<
					!is_same< typename remove_cv<typename remove_reference<FunctionT>::type>::type,
							static_function<ReturnType(ArgumentTypes...) >>::value>::type>
	static_function(FunctionT &&f) noexcept {
		mCallback = makeFreeFunction(forward<FunctionT>(f), mBuffer.data());
	}

	template <typename FunctionT>
	static_function &operator=(FunctionT &&f) noexcept {
		clear();
		mCallback = makeFreeFunction(forward<FunctionT>(f), mBuffer.data());
		return *this;
	}

	ReturnType operator()(ArgumentTypes... args) const {
		return mCallback()->invoke(mBuffer.data(), forward<ArgumentTypes>(args)...);
	}

	constexpr bool operator==(nullptr_t) const noexcept { return mCallback == nullptr; }

	constexpr bool operator!=(nullptr_t) const noexcept { return mCallback != nullptr; }

	constexpr explicit operator bool() const noexcept { return mCallback != nullptr; }

	constexpr bool operator==(const static_function &other) const noexcept {
		return mCallback == other.mCallback
				&& ::memcmp(mBuffer.data(), other.mBuffer.data(), mBuffer.size()) == 0;
	}

	constexpr bool operator!=(const static_function &other) const noexcept {
		return mCallback != other.mCallback
				|| ::memcmp(mBuffer.data(), other.mBuffer.data(), mBuffer.size()) == 0;
	}

private:
	static constexpr auto OptBufferSize = 16;

	using invoke_pointer = ReturnType (*)(const void *, ArgumentTypes...);
	using destroy_pointer = void (*)(void *);
	using copy_pointer = void *(*)(const uint8_t *, uint8_t *);
	using move_pointer = void *(*)(uint8_t *, uint8_t *);

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
		static_assert(sizeof(BaseType) <= OptBufferSize,
				"Unable to place functor in static_function");
		static functor_traits traits{
			.invoke = [](const void *arg, ArgumentTypes... args) -> ReturnType {
			return (*static_cast<const BaseType *>(arg))(forward<ArgumentTypes>(args)...);
		},
			.destroy =
					[](void *arg) {
			if (arg) {
				(*static_cast<BaseType *>(arg)).~BaseType();
			}
		},
			.copy = [](const uint8_t *arg, uint8_t *buf) -> void * {
			return new (buf) BaseType(*reinterpret_cast<const BaseType *>(arg));
		},
			.move = [](uint8_t *arg, uint8_t *buf) -> void * {
			return new (buf) BaseType(move_unsafe(*reinterpret_cast<BaseType *>(arg)));
		},
		};

		return &traits;
	}

	template <typename FunctionT>
	static traits_callback makeFreeFunction(FunctionT &&f, uint8_t *buf) noexcept {
		using BaseType = typename remove_cv<typename remove_reference<FunctionT>::type>::type;
		static_assert(sizeof(BaseType) <= OptBufferSize,
				"Unable to place functor in static_function");
		new (buf) BaseType(forward<FunctionT>(f));
		return &makeFunctionTraits<FunctionT>;
	}

	void clear() {
		if (mCallback) {
			auto t = mCallback();
			t->destroy(mBuffer.data());
			mCallback = nullptr;
		}
	}

	traits_callback mCallback = nullptr;
	array<uint8_t, FunctionBufferSize> mBuffer;
};


template <typename UnusedType>
class SPRT_API callback;

// Modern version. inspired by http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html
template <typename ReturnType, typename... ArgumentTypes>
class SPRT_API callback<ReturnType(ArgumentTypes...)> {
public:
	using signature_type = ReturnType(ArgumentTypes...);

	~callback() {
		// functor is not owned, so, no cleanup
	}

	callback(nullptr_t) noexcept : mFunctor(nullptr), mcallback(nullptr) { }

	callback &operator=(nullptr_t) noexcept {
		mFunctor = nullptr;
		mcallback = nullptr;
	}

	template <typename FunctionT>
	callback(const FunctionT &f) noexcept
	: mFunctor(&f), mcallback([](const void *arg, ArgumentTypes... args) {
		return (*static_cast<const FunctionT *>(arg))(forward<ArgumentTypes>(args)...);
	}) { }

	template <typename FunctionT>
	callback &operator=(const FunctionT &f) noexcept {
		mFunctor = &f;
		mcallback = [](const void *arg, ArgumentTypes... args) {
			return (*static_cast<const FunctionT *>(arg))(forward<ArgumentTypes>(args)...);
		};
	}

	template <typename FunctionType, typename ClassType>
	callback(FunctionType ClassType::*f) noexcept : mFunctor(f) {
		mcallback = makeMemberFunction<FunctionType, ArgumentTypes...>(f);
	}

	template <typename FunctionType, typename ClassType>
	callback &operator=(FunctionType ClassType::*f) noexcept {
		mFunctor = f;
		mcallback = makeMemberFunction<FunctionType, ArgumentTypes...>(f);
	}

	callback(const callback &other) noexcept = delete;
	callback &operator=(const callback &other) noexcept = delete;

	callback(callback &&other) noexcept = delete;
	callback &operator=(callback &&other) noexcept = delete;

	ReturnType operator()(ArgumentTypes... args) const {
		return mcallback(mFunctor, forward<ArgumentTypes>(args)...);
	}

	constexpr bool operator==(nullptr_t) const noexcept {
		return mFunctor == nullptr || mcallback == nullptr;
	}

	constexpr bool operator!=(nullptr_t) const noexcept {
		return mFunctor != nullptr && mcallback != nullptr;
	}

	constexpr explicit operator bool() const noexcept {
		return mFunctor != nullptr && mcallback != nullptr;
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
					forward<RestArgumentTypes>(args)...);
		};
	}

	const void *mFunctor;
	FunctionPointer mcallback;
};

template <typename ReturnType, size_t BufferSize, size_t Alignment, typename... ArgumentTypes>
class SPRT_API callback_storage : public callback<ReturnType(ArgumentTypes...)> {
public:
	static constexpr size_t _BufferSize = BufferSize;
	static constexpr size_t _Alignment = Alignment;

	template <typename FunctionT>
	callback_storage(FunctionT &&f)
	: callback<ReturnType(ArgumentTypes...)>(setFunction(forward<FunctionT>(f))) {
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
		using BaseType = typename remove_cv<typename remove_reference<FunctionT>::type>::type;
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
		new ((void *)_storage.buf) FunctionT(forward<FunctionT>(f));
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
struct SPRT_API callback_traits;

template <typename ClassType, typename ReturnType, typename... Args>
struct SPRT_API callback_traits<ReturnType (ClassType::*)(Args...) const> {
	using type = callback_storage<ReturnType, sizeof(ClassType), alignof(ClassType), Args...>;
};

template <typename ClassType, typename ReturnType, typename... Args>
struct SPRT_API callback_traits<ReturnType (ClassType::*)(Args...)> {
	using type = callback_storage<ReturnType, sizeof(ClassType), alignof(ClassType), Args...>;
};

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMECALLBACK_H_
