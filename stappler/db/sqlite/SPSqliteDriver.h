/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_DB_SQLITE_SPSQLITEDRIVER_H_
#define STAPPLER_DB_SQLITE_SPSQLITEDRIVER_H_

#include "SPSqlDriver.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

struct DriverHandle;
struct DriverSym;

class SP_PUBLIC Driver : public sql::Driver {
public:
	static Driver *open(pool_t *, ApplicationInterface *, StringView path = StringView());

	virtual ~Driver();

	virtual bool init(Handle handle, const Vector<StringView> &) override;

	virtual void performWithStorage(Handle handle, const Callback<void(const db::Adapter &)> &cb) const override;
	virtual BackendInterface *acquireInterface(Handle handle, pool_t *) const override;

	virtual Handle connect(const Map<StringView, StringView> &) const override;
	virtual void finish(Handle) const override;

	virtual Connection getConnection(Handle h) const override;

	virtual bool isValid(Handle) const override;
	virtual bool isValid(Connection) const override;
	virtual bool isIdle(Connection) const override;

	virtual Time getConnectionTime(Handle) const override;

	virtual bool isNotificationsSupported() const override { return false; }

	StringView getDbName(Handle) const;
	Value getInfo(Connection, int) const;

	void setUserId(Handle, int64_t) const;

	uint64_t insertWord(Handle, StringView) const;

	const DriverSym *getHandle() const { return _handle; }

protected:
	Driver(pool_t *, ApplicationInterface *, StringView, DriverSym *);

	bool _init = false;
	DriverSym *_handle = nullptr;
};

class SP_PUBLIC ResultCursor final : public db::ResultCursor {
public:
	static bool statusIsSuccess(int x);

	ResultCursor(const Driver *d, Driver::Connection, Driver::Result res, int);

	virtual ~ResultCursor();
	virtual bool isBinaryFormat(size_t field) const override;
	BackendInterface::StorageType getType(size_t field) const;
	virtual bool isNull(size_t field) const override;
	virtual StringView toString(size_t field) const override;
	virtual BytesView toBytes(size_t field) const override;
	virtual int64_t toInteger(size_t field) const override;
	virtual double toDouble(size_t field) const override;
	virtual bool toBool(size_t field) const override;
	virtual Value toTypedData(size_t field) const override;
	virtual Value toCustomData(size_t field, const FieldCustom *) const override;
	virtual int64_t toId() const override;
	virtual StringView getFieldName(size_t field) const override;
	virtual bool isSuccess() const override;
	virtual bool isEmpty() const override;
	virtual bool isEnded() const override;
	virtual size_t getFieldsCount() const override;
	virtual size_t getAffectedRows() const override;
	virtual size_t getRowsHint() const override;
	virtual Value getInfo() const override;
	virtual bool next() override;
	virtual void reset() override;
	virtual void clear() override;
	int getError() const;

public:
	const Driver *driver = nullptr;
	Driver::Connection conn = Driver::Connection(nullptr);
	Driver::Result result = Driver::Result(nullptr);
	int err = 0;
};

}

#endif /* STAPPLER_DB_SQLITE_SPSQLITEDRIVER_H_ */
