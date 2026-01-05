/**
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_UTILS_SPSUBSCRIPTION_H_
#define STAPPLER_CORE_UTILS_SPSUBSCRIPTION_H_

#include "SPMemInterface.h"

namespace STAPPLER_VERSIONIZED stappler {

using SubscriptionId = ValueWrapper<uint64_t, class SubscriptionIdClassFlag>;

struct SP_PUBLIC SubscriptionFlags
: public ValueWrapper<uint64_t, class SubscriptionFlagsClassFlag> {
	using Super = ValueWrapper<uint64_t, class SubscriptionFlagsClassFlag>;

	inline constexpr explicit SubscriptionFlags(const Type &val) : Super(val) { }
	inline constexpr explicit SubscriptionFlags(Type &&val) : Super(sp::move(val)) { }

	inline constexpr SubscriptionFlags(const SubscriptionFlags &other) { value = other.value; }
	inline constexpr SubscriptionFlags &operator=(const SubscriptionFlags &other) {
		value = other.value;
		return *this;
	}
	inline constexpr SubscriptionFlags(SubscriptionFlags &&other) { value = sp::move(other.value); }
	inline constexpr SubscriptionFlags &operator=(SubscriptionFlags &&other) {
		value = sp::move(other.value);
		return *this;
	}

	inline constexpr SubscriptionFlags(const Super &other) { value = other.value; }
	inline constexpr SubscriptionFlags &operator=(const Super &other) {
		value = other.value;
		return *this;
	}
	inline constexpr SubscriptionFlags(Super &&other) { value = sp::move(other.value); }
	inline constexpr SubscriptionFlags &operator=(Super &&other) {
		value = sp::move(other.value);
		return *this;
	}

	template <typename T>
	inline constexpr bool hasFlag(T f) const {
		return (toInt(f) & value) != 0;
	}

	inline constexpr bool initial() const { return (value & 1) != 0; }
};

/* Subscription is a refcounted class, that allow
 * provider object (derived from Subscription) to post some update flags,
 * and subscribers (any other objects) to receive it via `check` call
 *
 * Every registered subscriber has it's own id, that used to check for updates
 * since last call of `check`
 */
template <typename _Interface>
class SP_PUBLIC SubscriptionTemplate : public Ref {
public:
	using Interface = _Interface;
	using Id = SubscriptionId;
	using Flags = SubscriptionFlags;
	using FlagsMap = typename Interface::template MapType<Id, Flags>;

	// get unique subscription id
	static Id getNextId();

	// initial flags value, every new subscriber receive this on first call of 'check'
	static constexpr Flags Initial = Flags(1);

	virtual ~SubscriptionTemplate() { setForwardedSubscription(nullptr); }

	// safe way to get Flag with specific bit set, prevents UB
	template <class T>
	static Flags _Flag(T idx) {
		return ((uint8_t)idx == 0 || (uint8_t)idx > (sizeof(Flags) * 8)) ? Flags(0)
																		 : Flags(1 << (uint8_t)idx);
	}

	inline static Flags Flag() { return Flags(0); }

	// Variadic way to get specific bits set via multiple arguments of different integral types
	template <class T, class... Args>
	static Flags Flag(T val, Args &&...args) {
		return _Flag(val) | Flag(args...);
	}

	// Set subscription dirty flags
	void setDirty(Flags flags = Initial, bool forwardedOnly = false);

	// subscribe with specific object id
	bool subscribe(Id);

	// unsubscribe object by id
	bool unsubscribe(Id);

	// check wich flags has been set dirty since last check
	Flags check(Id);

	// subscription can actually be a reference to other subscription, in this case
	// current subscription works as bidirectional forwarder for reference subscription
	void setForwardedSubscription(SubscriptionTemplate *);

protected:
	Rc<SubscriptionTemplate> _forwarded;
	FlagsMap *_forwardedFlags = nullptr;
	FlagsMap _flags;
};

/** Binding is an interface or slot for subscription, that handles
 * - unique id acquisition
 * - proper refcounting for binded subscription
 * - easy access and not-null check for binded subscription
 *
 */
template <class T>
class SP_PUBLIC Binding {
public:
	using Id = SubscriptionId;
	using Flags = SubscriptionFlags;

	Binding();
	Binding(T *);

	~Binding();

	Binding(const Binding<T> &);
	Binding &operator=(const Binding<T> &);

	Binding(Binding<T> &&);
	Binding &operator=(Binding<T> &&);

	inline operator T *() { return get(); }
	inline operator T *() const { return get(); }
	inline explicit operator bool() const { return _subscription; }

	inline T *operator->() { return get(); }
	inline const T *operator->() const { return get(); }

	Flags check();

	void set(T *);
	T *get() const;

protected:
	Id _id;
	Rc<T> _subscription;
};

template <typename Interface>
void SubscriptionTemplate<Interface>::setDirty(Flags flags, bool forwardedOnly) {
	if (_forwardedFlags) {
		for (auto &it : (*_forwardedFlags)) {
			if (forwardedOnly) {
				if (_flags.find(it.first) != _flags.end()) {
					it.second |= flags;
				}
			} else {
				it.second |= flags;
			}
		}
	} else {
		for (auto &it : _flags) { it.second |= flags; }
	}
}

template <typename Interface>
bool SubscriptionTemplate<Interface>::subscribe(Id id) {
	if (_forwardedFlags) {
		auto it = _forwardedFlags->find(id);
		if (it == _forwardedFlags->end()) {
			_forwardedFlags->insert(std::make_pair(id, Initial));
			_flags.insert(std::make_pair(id, Initial));
			return true;
		}
	} else {
		auto it = _flags.find(id);
		if (it == _flags.end()) {
			_flags.insert(std::make_pair(id, Initial));
			return true;
		}
	}
	return false;
}

template <typename Interface>
bool SubscriptionTemplate<Interface>::unsubscribe(Id id) {
	if (_forwardedFlags) {
		auto it = _forwardedFlags->find(id);
		if (it == _forwardedFlags->end()) {
			_forwardedFlags->erase(id);
			_flags.erase(id);
			return true;
		}
	} else {
		auto it = _flags.find(id);
		if (it != _flags.end()) {
			_flags.erase(id);
			return true;
		}
	}
	return false;
}

template <typename Interface>
SubscriptionFlags SubscriptionTemplate<Interface>::check(Id id) {
	if (_forwardedFlags) {
		auto it = _forwardedFlags->find(id);
		if (it != _forwardedFlags->end()) {
			auto val = it->second;
			it->second = Flags(0);
			return val;
		}
	} else {
		auto it = _flags.find(id);
		if (it != _flags.end()) {
			auto val = it->second;
			it->second = Flags(0);
			return val;
		}
	}
	return Flags(0);
}

template <typename Interface>
void SubscriptionTemplate<Interface>::setForwardedSubscription(SubscriptionTemplate *sub) {
	if (_forwardedFlags) {
		// erase forwarded items from sub
		for (auto &it : _flags) { _forwardedFlags->erase(it.first); }
	}
	_forwardedFlags = nullptr;
	_forwarded = sub;
	_flags.clear();
	if (sub) {
		_forwardedFlags = sub->_forwardedFlags ? sub->_forwardedFlags : &sub->_flags;
	}
}

template <class T>
Binding<T>::Binding() : _id(T::getNextId()) { }

template <class T>
Binding<T>::Binding(T *sub) : _id(T::getNextId()), _subscription(sub) {
	if (_subscription) {
		_subscription->subscribe(_id);
	}
}

template <class T>
Binding<T>::~Binding() {
	if (_subscription) {
		_subscription->unsubscribe(_id);
	}
}

template <class T>
Binding<T>::Binding(const Binding<T> &other)
: _id(T::getNextId()), _subscription(other._subscription) {
	if (_subscription) {
		_subscription->subscribe(_id);
	}
}

template <class T>
Binding<T> &Binding<T>::operator=(const Binding<T> &other) {
	if (_subscription) {
		_subscription->unsubscribe(_id);
	}
	_subscription = other._subscription;
	if (_subscription) {
		_subscription->subscribe(_id);
	}
	return *this;
}

template <class T>
Binding<T>::Binding(Binding &&other) {
	_id = other._id;
	_subscription = sp::move(other._subscription);
}

template <class T>
Binding<T> &Binding<T>::operator=(Binding<T> &&other) {
	if (_subscription) {
		_subscription->unsubscribe(_id);
	}
	_subscription = sp::move(other._subscription);
	_id = other._id;
	return *this;
}

template <class T>
SubscriptionFlags Binding<T>::check() {
	if (_subscription) {
		return _subscription->check(_id);
	}
	return Flags(0);
}

template <class T>
void Binding<T>::set(T *sub) {
	if (_subscription) {
		_subscription->unsubscribe(_id);
	}
	_subscription = sub;
	if (_subscription) {
		_subscription->subscribe(_id);
	}
}

template <class T>
T *Binding<T>::get() const {
	return _subscription ? _subscription.get() : nullptr;
}

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::mem_std {

using Subscription = stappler::SubscriptionTemplate<memory::StandartInterface>;

}

namespace STAPPLER_VERSIONIZED stappler::mem_pool {

using Subscription = stappler::SubscriptionTemplate<memory::PoolInterface>;

}

#endif /* STAPPLER_CORE_UTILS_SPSUBSCRIPTION_H_ */
