/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_DB_SPDBSIMPLESERVER_H_
#define CORE_DB_SPDBSIMPLESERVER_H_

#include "SPSqlDriver.h"
#include "SPDbScheme.h"

namespace STAPPLER_VERSIONIZED stappler::db {

struct SP_PUBLIC SimpleServerData : AllocBase {
	pool_t *staticPool = nullptr;
	pool_t *contextPool = nullptr;
	pool_t *updatePool = nullptr;

	String documentRoot;
	AccessRoleId defaultRole;
	sql::Driver *driver = nullptr;
	sql::Driver::Handle handle;
	BackendInterface::Config interfaceConfig;

	Scheme users = Scheme("__users");
	Scheme files = Scheme("__files");
	Scheme errors = Scheme("__error");

	mutable Vector<Function<void(const Transaction &)>> *asyncTasks = nullptr;
};

class SP_PUBLIC SimpleServer : public Ref, public ApplicationInterface {
public:
	virtual ~SimpleServer();
	SimpleServer();

	virtual bool init(const Value &, StringView root, AccessRoleId, SpanView<const db::Scheme *>);

	virtual void update();
	virtual void perform(const Callback<bool(const db::Transaction &)> &cb);

	virtual void scheduleAyncDbTask(const Callback<Function<void(const Transaction &)>(pool_t *)> &setupCb) const override;

	virtual StringView getDocumentRoot() const override;
	virtual const Scheme *getFileScheme() const override;
	virtual const Scheme *getUserScheme() const override;

	virtual void initTransaction(db::Transaction &) const override;

	virtual void pushErrorMessage(Value &&) const override;
	virtual void pushDebugMessage(Value &&) const override;

	StringView getDatabaseName() const;

protected:
	SimpleServerData *_data = nullptr;
};

}

#endif /* CORE_DB_SPDBSIMPLESERVER_H_ */
