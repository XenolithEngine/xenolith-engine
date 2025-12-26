/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_DB_SPDBADAPTER_H_
#define STAPPLER_DB_SPDBADAPTER_H_

#include "SPDbBackendInterface.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class SP_PUBLIC ApplicationInterface {
public:
	static void defineUserScheme(Scheme &);
	static void defineFileScheme(Scheme &);
	static void defineErrorScheme(Scheme &);

	virtual ~ApplicationInterface();

	template <typename ... Args>
	void error(Args && ...args) const {
		_pushError(std::forward<Args>(args)...);
	}

	template <typename ... Args>
	void debug(Args && ...args) const {
		_pushDebug(std::forward<Args>(args)...);
	}

	template <typename Source, typename Text>
	void _pushError(Source &&source, Text &&text) const {
		pushErrorMessage(Value{
			std::make_pair("source", Value(std::forward<Source>(source))),
			std::make_pair("text", Value(std::forward<Text>(text)))
		});
	}

	template <typename Source, typename Text>
	void _pushError(Source &&source, Text &&text, Value &&d) const {
		pushErrorMessage(Value{
			std::make_pair("source", Value(std::forward<Source>(source))),
			std::make_pair("text", Value(std::forward<Text>(text))),
			std::make_pair("data", sp::move(d))
		});
	}

	template <typename Source, typename Text>
	void _pushDebug(Source &&source, Text &&text) const {
		pushDebugMessage(Value{
			std::make_pair("source", Value(std::forward<Source>(source))),
			std::make_pair("text", Value(std::forward<Text>(text)))
		});
	}

	template <typename Source, typename Text>
	void _pushDebug(Source &&source, Text &&text, Value &&d) const {
		pushDebugMessage(Value{
			std::make_pair("source", Value(std::forward<Source>(source))),
			std::make_pair("text", Value(std::forward<Text>(text))),
			std::make_pair("data", sp::move(d))
		});
	}

	virtual db::Adapter getAdapterFromContext() const;
	virtual void scheduleAyncDbTask(const Callback<Function<void(const Transaction &)>(pool_t *)> &setupCb) const;

	virtual StringView getDocumentRoot() const;

	virtual const Scheme *getFileScheme() const { return nullptr; }
	virtual const Scheme *getUserScheme() const { return nullptr; }

	virtual void pushErrorMessage(Value &&) const;
	virtual void pushDebugMessage(Value &&) const;

	virtual db::InputFile *getFileFromContext(int64_t id) const { return nullptr; }
	virtual int64_t getUserIdFromContext() const { return 0; }

	virtual RequestData getRequestData() const { return RequestData(); }

	virtual void initTransaction(db::Transaction &) const { }

	virtual void reportDbUpdate(StringView, bool successful);
};

class SP_PUBLIC Adapter final : public AllocBase {
public:
	static Adapter FromContext(const ApplicationInterface *);

	Adapter(BackendInterface *, const ApplicationInterface *);

	Adapter(const Adapter &);
	Adapter& operator=(const Adapter &);

	explicit operator bool () const { return _interface != nullptr && _application != nullptr; }

	bool operator==(const Adapter &other) const { return _interface == other._interface && _application == other._application; }

	const ApplicationInterface *getApplicationInterface() const { return _application; }
	BackendInterface *getBackendInterface() const { return _interface; }

	String getTransactionKey() const;

	bool set(const CoderSource &, const Value &, TimeInterval = config::STORAGE_DEFAULT_KEY_VALUE_INTERVAL) const;
	Value get(const CoderSource &) const;
	bool clear(const CoderSource &) const;

	bool init(const BackendInterface::Config &cfg, const Map<StringView, const Scheme *> &) const;

	void makeSessionsCleanup() const;

	User * authorizeUser(const Auth &, const StringView &name, const StringView &password) const;

	void broadcast(const Bytes &) const;
	void broadcast(const Value &val) const;
	void broadcast(StringView url, Value &&val, bool exclusive) const;

	bool performWithTransaction(const Callback<bool(const db::Transaction &)> &cb) const;

	Vector<int64_t> getReferenceParents(const Scheme &, uint64_t oid, const Scheme *, const Field *) const;

	StringView getDatabaseName() const { return _interface->getDatabaseName(); }

protected:
	friend class Transaction;

	int64_t getDeltaValue(const Scheme &); // scheme-based delta
	int64_t getDeltaValue(const Scheme &, const FieldView &, uint64_t); // view-based delta

	Vector<int64_t> performQueryListForIds(const QueryList &, size_t count = stappler::maxOf<size_t>()) const;
	Value performQueryList(const QueryList &, size_t count = stappler::maxOf<size_t>(), bool forUpdate = false) const;

	bool foreach(Worker &, const Query &, const Callback<bool(Value &)> &) const;
	Value select(Worker &, const Query &) const;

	Value create(Worker &, Value &) const;
	Value save(Worker &, uint64_t oid, Value &obj, Value &patch, const Set<const Field *> &fields) const;

	bool remove(Worker &, uint64_t oid) const;

	size_t count(Worker &, const Query &) const;

	Value field(Action, Worker &, uint64_t oid, const Field &, Value && = Value()) const;
	Value field(Action, Worker &, const Value &, const Field &, Value && = Value()) const;

	bool addToView(const FieldView &, const Scheme *, uint64_t oid, const Value &) const;
	bool removeFromView(const FieldView &, const Scheme *, uint64_t oid) const;

	bool beginTransaction() const;
	bool endTransaction() const;

	void cancelTransaction() const;
	bool isInTransaction() const;
	TransactionStatus getTransactionStatus() const;

	void processFullTextFields(const Scheme &, Value &patch, Vector<InputField> &ifields, Vector<InputRow> &ivalues) const;

	const ApplicationInterface *_application = nullptr;
	BackendInterface *_interface = nullptr;
};

}

#endif /* STAPPLER_DB_SPDBADAPTER_H_ */
