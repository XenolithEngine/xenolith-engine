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

#ifndef XENOLITH_APPLICATION_NODES_XLCOMPONENT_H_
#define XENOLITH_APPLICATION_NODES_XLCOMPONENT_H_

#include "XLNodeInfo.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/** Component is the way to provide additional data fields with Node

It's a lightweight type-punning struct to store some data, associated with per-type unique id

Component in Node defined by it's id, only one Component per id allowed.

To acquire new unique id for your type (and save it in static System's field possibly), use `Component::GetNextId()`

Type for a component should define field `static const CompnentId Id;`

Code style recomendations:
- Component typm should be standart-layout struct (https://en.cppreference.com/w/cpp/named_req/StandardLayoutType.html)
- Component Id should be acquired statically
- Component type should be default construnctable (https://en.cppreference.com/w/cpp/types/is_default_constructible.html)
*/

struct ComponentId {
	uint32_t value;

	ComponentId();
};

struct Component {
	// Size in bytes for static in-place storage
	static constexpr uint32_t STATIC_SIZE = 28;

	static uint32_t GetNextId();

	uint32_t id			 : 31; // unique component class id
	mutable uint32_t soo : 1; // 1 if dynamic storage is used

	union {
		mutable struct {
			size_t size;
			void *data;
		} dynamicStorage = {0, nullptr};

		mutable struct {
			uint8_t bytes[STATIC_SIZE];
		} staticStorage;
	};

	mutable void (*destructor)(void *) = nullptr;

	bool operator==(const ComponentId &value) const { return id == value.value; }
	bool operator!=(const ComponentId &value) const { return id != value.value; }

	bool operator==(uint32_t value) const { return id == value; }
	bool operator!=(uint32_t value) const { return id != value; }

	bool operator==(const Component &value) const { return id == value.id; }
	bool operator!=(const Component &value) const { return id != value.id; }

	void clear() const;

	template <typename T>
	T *get() const {
		static_assert(requires(T) { T::Id.value; },
				"Component type T should define `static ComponentId Id` field");
		if (id != T::Id.value) {
			return nullptr;
		}
		if (soo) {
			return reinterpret_cast<T *>(staticStorage.bytes);
		} else {
			return reinterpret_cast<T *>(dynamicStorage.data);
		}
	}

	template <typename T, typename... Args>
	T *create(Args &&...args) const {
		static_assert(requires(T) { T::Id.value; },
				"Component type T should define `static ComponentId Id` field");
		if (T::Id.value != id) {
			log::source().error("Component", "ComponentId mismatch: ", T::Id.value, " vs. ", id);
			return nullptr;
		}

		clear();

		if constexpr (sizeof(T) > STATIC_SIZE) {
			soo = 0;
			destructor = [](void *ptr) { delete reinterpret_cast<T *>(ptr); };
			dynamicStorage.size = sizeof(T);
			auto d = new (std::nothrow) T(std::forward<Args>(args)...);
			dynamicStorage.data = d;
			return d;
		} else {
			soo = 1;
			destructor = [](void *ptr) { reinterpret_cast<T *>(ptr)->~T(); };
			return new (staticStorage.bytes) T(std::forward<Args>(args)...);
		}
	}

	Component(const ComponentId &v) : id(v.value), soo(0) { }
	~Component() { clear(); }
};

struct ComponentEqual {
	using is_transparent = void;

	bool operator()(const Component &l, const Component &r) const { return l.id == r.id; }
	bool operator()(const Component &l, const ComponentId &r) const { return l.id == r.value; }
	bool operator()(const Component &l, const uint32_t &r) const { return l.id == r; }

	bool operator()(const ComponentId &l, const Component &r) const { return l.value == r.id; }
	bool operator()(const ComponentId &l, const ComponentId &r) const { return l.value == r.value; }
	bool operator()(const ComponentId &l, const uint32_t &r) const { return l.value == r; }

	bool operator()(const uint32_t &l, const Component &r) const { return l == r.id; }
	bool operator()(const uint32_t &l, const ComponentId &r) const { return l == r.value; }
	bool operator()(const uint32_t &l, const uint32_t &r) const { return l == r; }
};

struct ComponentHash {
	using hash_type = std::hash<uint32_t>;
	using is_transparent = void;

	std::size_t operator()(Component c) const { return hash_type{}(c.id); }
	std::size_t operator()(ComponentId id) const { return hash_type{}(id.value); }
	std::size_t operator()(uint32_t id) const { return hash_type{}(id); }
};

class ComponentContainer {
public:
	// Set value for component of type T
	// T should implement static Id field
	// Arguments passed directly into T's constructor
	// If component T was already defined - it will be updated with new value
	template <typename T, typename... Args>
	T *setComponent(Args &&...);

	// Acquire current value of component T to perform some updates on it
	// If there is no component with type T - do nothing and returns nullptr.
	// Callback should return true if value was modified
	template <typename T>
	const T *updateComponent(const Callback<bool(NotNull<T>)> &);

	// Acquire current value of component T to perform some updates on it
	// or create this component with default constructor (should be available)
	// then return components's value into callback
	// Callback should return true if value was modified.
	template <typename T>
	const T *setOrUpdateComponent(const Callback<bool(NotNull<T>)> &);

	// Acquire current value of component T or nullptr
	template <typename T>
	const T *getComponent() const;

	// Remove component T, if it was defined
	// returns true if component was defined
	template <typename T>
	bool removeComponent();

	void removeAllComponents();

protected:
	bool _componentsDirty = false;
	HashSet<Component, ComponentHash, ComponentEqual> _components;
};

template <typename T, typename... Args>
T *ComponentContainer::setComponent(Args &&...args) {
	_componentsDirty = true;
	auto it = _components.find(T::Id);
	if (it == _components.end()) {
		it = _components.emplace(T::Id).first;
	}
	return (*it).template create<T>(sp::forward<Args>(args)...);
}

template <typename T>
const T *ComponentContainer::updateComponent(const Callback<bool(NotNull<T>)> &cb) {
	auto it = _components.find(T::Id);
	if (it != _components.end()) {
		if (cb((*it).template get<T>())) {
			_componentsDirty = true;
		}
	}
	return (*it).template get<T>();
}

template <typename T>
const T *ComponentContainer::setOrUpdateComponent(const Callback<bool(NotNull<T>)> &cb) {
	static_assert(std::is_default_constructible_v<T>,
			"Component with type T should be default constructable for Node::setOrUpdateComponent");
	auto it = _components.find(T::Id);
	if (it == _components.end()) {
		it = _components.emplace(T::Id).first;
		(*it).template create<T>();
		_componentsDirty = true;
	}

	if (cb((*it).template get<T>())) {
		_componentsDirty = true;
	}
	return (*it).template get<T>();
}

template <typename T>
const T *ComponentContainer::getComponent() const {
	auto it = _components.find(T::Id);
	if (it != _components.end()) {
		return (*it).template get<T>();
	}
	return nullptr;
}

template <typename T>
bool ComponentContainer::removeComponent() {
	auto it = _components.find(T::Id);
	if (it != _components.end()) {
		_componentsDirty = true;
		_components.erase(it);
		return true;
	}
	return false;
}

} // namespace stappler::xenolith

namespace std {

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Component> {
	hash() { }

	size_t operator()(
			const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Component &comp) const noexcept {
		return hash<uint32_t>()(comp.id);
	}
};

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::ComponentId> {
	hash() { }

	size_t operator()(
			const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::ComponentId &comp) const noexcept {
		return hash<uint32_t>()(comp.value);
	}
};

} // namespace std

#endif // XENOLITH_APPLICATION_NODES_XLCOMPONENT_H_
