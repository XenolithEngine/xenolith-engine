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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ACCESS_TOKEN_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ACCESS_TOKEN_H_

#include <sprt/cxx/__memory/common.h>
#include <sprt/cxx/detail/aligned_storage.h>
#include <sprt/cxx/iterator>

namespace sprt::detail {

// To use contaiter's lookup functions like at() or [] without exceptions enabled,
// we need a proper wrapper, that can provide well-defined behavior, when SLT's
// containers throws exceptions

// access_token can be used to implement (without exceptions):
// - container[key] = value; // element will be constructed on assignment, not on access
// - value = container[key];
// - fn(container[key].get()); // default-constructable
// - fn(container[key].get(aternative_value)); // by value or reference
// - container[key](arg1, arg2, ...); // result should be default-constructable
// - container[key].unwrap([](mapped_type &) { ... }); // called when value is defined

template <typename Container, typename Key>
class access_token {
public:
	using key_type = Key;
	using container_key_type = Container::key_type;
	using value_type = Container::value_type;
	using mapped_type = Container::mapped_type;
	using iterator = Container::iterator;

	friend Container;

	constexpr access_token &operator=(const mapped_type &value) noexcept {
		if (_container) {
			if (_iterator == sprt::end(*_container)) {
				_iterator = _container->emplace_with_token(sprt::move_unsafe(_key.ref(), value));
				if (_iterator != sprt::end(*_container)) {
					sprt::destroy_at(_key.ptr());
				}
			} else {
				aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
						*_iterator) = value;
			}
		}
		return *this;
	}

	constexpr access_token &operator=(mapped_type &&value) noexcept {
		if (_container) {
			if (_iterator == sprt::end(*_container)) {
				_iterator = _container->emplace_with_token(sprt::move_unsafe(_key.ref()),
						sprt::move_unsafe(value));
				if (_iterator != sprt::end(*_container)) {
					sprt::destroy_at(_key.ptr());
				}
			} else {
				aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
						*_iterator) = sprt::move_unsafe(value);
			}
		}
		return *this;
	}

	template <typename... Args>
	constexpr access_token &emplace(Args &&...args) noexcept {
		if (_container) {
			if (_iterator == sprt::end(*_container)) {
				_iterator = _container->emplace_with_token(
						sprt::move_unsafe(_key.ref(), sprt::forward<Args>(args)...));
				if (_iterator != sprt::end(*_container)) {
					sprt::destroy_at(_key.ptr());
				}
			} else {
				aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
						*_iterator) = mapped_type(sprt::forward<Args>(args)...);
			}
		}
		return *this;
	}

	template <typename Callback>
	constexpr sprt::enable_if_t<sprt::is_invocable_v<Callback, mapped_type &>, bool> unwrap(
			const Callback &cb) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			cb(aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator));
			return true;
		}
		return false;
	}

	constexpr mapped_type *ptr() const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return &aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return nullptr;
	}

	constexpr mapped_type get() const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return mapped_type();
	}
	constexpr mapped_type get(const mapped_type &alt) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return alt;
	}
	constexpr mapped_type get(mapped_type &&alt) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return alt;
	}

	constexpr mapped_type &get(mapped_type &alt) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return alt;
	}

	constexpr explicit operator bool() const noexcept {
		return _iterator != sprt::end(*_container);
	}

	template <typename... _ArgTypes>
	constexpr invoke_result_t<mapped_type &, _ArgTypes...> operator()(_ArgTypes &&...__args) const
			// Since is_nothrow_invocable requires C++17 LWG3764 is not backported
			// to earlier versions.
			noexcept(is_nothrow_invocable_v<mapped_type &, _ArgTypes...>) {
		if (_iterator != sprt::end(*_container)) {
			return sprt::__invoke(
					aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
							*_iterator),
					sprt::forward<_ArgTypes>(__args)...);
		}
		return invoke_result_t<mapped_type &, _ArgTypes...>();
	}

protected:
	access_token(Container &c, const Key &k) {
		_iterator = c.find_for_token(k);
		_container = &c;
		if (_iterator == sprt::end(*_container)) {
			sprt::construct_at(_key.ptr(), k);
		}
	}

	access_token(Container &c, Key &&k) {
		_iterator = c.find_for_token(k);
		_container = &c;
		if (_iterator == sprt::end(*_container)) {
			sprt::construct_at(_key.ptr(), sprt::move_unsafe(k));
		}
	}

	Container *_container = nullptr;
	iterator _iterator;
	aligned_storage<key_type> _key;
};

template <typename Container, typename Key>
class const_access_token {
public:
	using key_type = Key;
	using container_key_type = Container::key_type;
	using value_type = Container::value_type;
	using mapped_type = Container::mapped_type;
	using iterator = Container::const_iterator;

	friend Container;

	template <typename Callback>
	constexpr sprt::enable_if_t<sprt::is_invocable_v<Callback, const mapped_type &>, bool> unwrap(
			const Callback &) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			cb(aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator));
			return true;
		}
		return false;
	}

	constexpr const mapped_type *ptr() const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return &aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return nullptr;
	}

	constexpr mapped_type get() const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return mapped_type();
	}
	constexpr mapped_type get(const mapped_type &alt) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return alt;
	}
	constexpr mapped_type get(mapped_type &&alt) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return alt;
	}

	constexpr const mapped_type &get(mapped_type &alt) const noexcept {
		if (_iterator != sprt::end(*_container)) {
			return aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
					*_iterator);
		}
		return alt;
	}

	constexpr explicit operator bool() const noexcept {
		return _iterator != sprt::end(*_container);
	}

	template <typename... _ArgTypes>
	constexpr invoke_result_t<mapped_type &, _ArgTypes...> operator()(_ArgTypes &&...__args) const
			// Since is_nothrow_invocable requires C++17 LWG3764 is not backported
			// to earlier versions.
			noexcept(is_nothrow_invocable_v<mapped_type &, _ArgTypes...>) {
		if (_iterator != sprt::end(*_container)) {
			return sprt::__invoke(
					aligned_storage_kv_traits<container_key_type, value_type>::extract_value(
							*_iterator),
					sprt::forward<_ArgTypes>(__args)...);
		}
		return invoke_result_t<mapped_type &, _ArgTypes...>();
	}

protected:
	const_access_token(const Container &c, const Key &k) {
		_iterator = c.find_for_token(k);
		_container = &c;
	}

	const_access_token(const Container &c, Key &&k) {
		_iterator = c.find_for_token(k);
		_container = &c;
	}

	const Container *_container = nullptr;
	iterator _iterator;
	aligned_storage<key_type> _key;
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ACCESS_TOKEN_H_
