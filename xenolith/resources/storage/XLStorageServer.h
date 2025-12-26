/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RESOURCES_STORAGE_XLSTORAGESERVER_H_
#define XENOLITH_RESOURCES_STORAGE_XLSTORAGESERVER_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLAppThread.h" // IWYU pragma: keep
#include "XLEvent.h"
#include "SPDbScheme.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::storage {

class ComponentContainer;

class SP_PUBLIC Server : public ApplicationExtension {
public:
	using DataCallback = Function<void(const Value &)>;
	using QueryCallback = Function<void(db::Query &)>;

	template <typename T>
	using InitList = std::initializer_list<T>;

	struct ServerData;

	using Scheme = db::Scheme;

	static EventHeader onBroadcast;

	static Rc<ApplicationExtension> createServer(AppThread *, const Value &params);

	virtual ~Server();

	virtual bool init(AppThread *, const Value &params);

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	virtual void update(AppThread *, const UpdateTime &t, bool wakeup) override;

	Rc<ComponentContainer> getComponentContainer(StringView) const;
	bool addComponentContainer(const Rc<ComponentContainer> &);
	bool removeComponentContainer(const Rc<ComponentContainer> &);

	// get value for key
	bool get(CoderSource, DataCallback &&) const;

	// set value for key, optionally returns previous
	bool set(CoderSource, Value &&, DataCallback && = nullptr) const;

	// remove value for key, optionally returns previous
	bool clear(CoderSource, DataCallback && = nullptr) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const Value &id,
			db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid, StringView field,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias, StringView field,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const Value &id, StringView field,
			db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid, InitList<StringView> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias, InitList<StringView> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const Value &id, InitList<StringView> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid, InitList<const char *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias, InitList<const char *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const Value &id, InitList<const char *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid, InitList<const db::Field *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			InitList<const db::Field *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const Value &id, InitList<const db::Field *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;

	// returns Array with zero or more Dictionaries with object data or Null value
	bool select(const Scheme &, DataCallback &&, QueryCallback && = nullptr,
			db::UpdateFlags = db::UpdateFlags::None) const;

	// returns Dictionary with single object data or Null value
	bool create(const Scheme &, Value &&, DataCallback && = nullptr,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool create(const Scheme &, Value &&, DataCallback &&, db::Conflict::Flags) const;
	bool create(const Scheme &, Value &&, DataCallback &&, db::UpdateFlags,
			db::Conflict::Flags) const;

	bool update(const Scheme &, uint64_t oid, Value &&data, DataCallback && = nullptr,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool update(const Scheme &, const Value &obj, Value &&data, DataCallback && = nullptr,
			db::UpdateFlags = db::UpdateFlags::None) const;

	bool remove(const Scheme &, uint64_t oid, Function<void(bool)> && = nullptr) const;
	bool remove(const Scheme &, const Value &, Function<void(bool)> && = nullptr) const;

	bool count(const Scheme &, Function<void(size_t)> &&) const;
	bool count(const Scheme &, Function<void(size_t)> &&, QueryCallback &&) const;

	bool touch(const Scheme &, uint64_t id) const;
	bool touch(const Scheme &, const Value &obj) const;

	// perform on Server's thread
	bool perform(Function<bool(const Server &, const db::Transaction &)> &&, Ref * = nullptr) const;

	AppThread *getApplication() const;

protected:
	bool get(const Scheme &, DataCallback &&, uint64_t oid, Vector<const db::Field *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias, Vector<const db::Field *> &&fields,
			db::UpdateFlags = db::UpdateFlags::None) const;

	ServerData *_data = nullptr;
};

} // namespace stappler::xenolith::storage

#endif /* XENOLITH_RESOURCES_STORAGE_XLSTORAGESERVER_H_ */
