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


// Waiters for replies of one connection. A server keeps one table per client, and serials are
// counted per connection, so equal serials in two tables must never reach each other's waiters.

#include "SPCommon.h"

#include "XLRemoteReplyTable.h"
#include "XLRemotePeer.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;

static MessageHeader makeReply(uint32_t serial, MessageType type = MessageType::ServerReply) {
	MessageHeader h{};
	h.msgtype = toInt(type);
	h.domain = toInt(Domain::Window);
	h.serial = serial;
	return h;
}

void performReplyTableTests() {
	sprt::cout << "--- remote reply table ---\n";

	{
		ReplyTable table;
		uint32_t got = 0;
		table.wait(7, [&](const MessageHeader &h, BytesView) { got = h.serial; }, 0);
		check(!table.dispatch(makeReply(8), BytesView()) && got == 0,
				"replytable: a reply to another serial is not consumed");
		check(table.dispatch(makeReply(7), BytesView()) && got == 7 && table.empty(),
				"replytable: the reply reaches its waiter once and leaves the table");
		check(!table.dispatch(makeReply(7), BytesView()),
				"replytable: a second reply with the same serial is not consumed");
	}

	{
		ReplyTable table;
		bool called = false;
		table.wait(3, [&](const MessageHeader &, BytesView) { called = true; }, 0);
		MessageHeader request{};
		request.msgtype = toInt(MessageType::Server);
		request.serial = 3;
		check(!table.dispatch(request, BytesView()) && !called,
				"replytable: a request carrying a waited serial is not a reply");
	}

	{
		// Two connections, the same serial in each.
		ReplyTable first;
		ReplyTable second;
		uint32_t firstCalls = 0;
		uint32_t secondCalls = 0;
		first.wait(1, [&](const MessageHeader &, BytesView) { ++firstCalls; }, 0);
		second.wait(1, [&](const MessageHeader &, BytesView) { ++secondCalls; }, 0);
		second.dispatch(makeReply(1), BytesView());
		check(firstCalls == 0 && secondCalls == 1 && first.size() == 1,
				"replytable: equal serials in two tables are independent");
	}

	{
		ReplyTable table;
		MessageHeader seen{};
		bool expired = false;
		table.wait(5, [&](const MessageHeader &h, BytesView) {
			expired = true;
			seen = h;
		}, 1'000);
		table.wait(6, [](const MessageHeader &, BytesView) { }, 0);
		check(!table.failExpired(999, MessageType::ClientError) && !expired,
				"replytable: nothing expires before its deadline");
		check(table.failExpired(1'000, MessageType::ClientError) && expired && seen.serial == 5
						&& seen.msgtype == toInt(MessageType::ClientError)
						&& seen.code == toInt(GlobalError::NetworkBackend),
				"replytable: at the deadline the waiter gets a local error");
		check(table.size() == 1 && !table.failExpired(1'000'000, MessageType::ClientError),
				"replytable: a waiter without a deadline never expires");
	}

	{
		ReplyTable table;
		bool errored = false;
		table.wait(9, [&](const MessageHeader &h, BytesView) { errored = isError(h); }, 0);
		check(table.dispatch(makeReply(9, MessageType::ServerError), BytesView()) && errored,
				"replytable: an error in place of a reply reaches the waiter");
	}

	{
		ReplyTable table;
		bool called = false;
		table.wait(2, [&](const MessageHeader &, BytesView) { called = true; }, 10);
		table.clear();
		check(table.empty() && !table.failExpired(100, MessageType::ClientError) && !called,
				"replytable: clear drops waiters without calling them");
	}

	{
		// An application request that went unanswered is completed, but does not condemn the peer.
		ReplyTable table;
		MessageHeader seen{};
		table.wait(4, [&](const MessageHeader &h, BytesView) { seen = h; }, 100, false);
		check(!table.failExpired(100, MessageType::ClientError) && seen.serial == 4
						&& table.empty(),
				"replytable: a non-fatal waiter is completed at its deadline without failing");
		check(getAppReplyStatus(seen) == Status::ErrorTimeout,
				"replytable: an expired app request reads as a timeout");
	}

	{
		MessageHeader refused = makeReply(1, MessageType::ServerError);
		refused.domain = toInt(Domain::Global);
		refused.code = toInt(GlobalError::NotImplemented);
		check(getAppReplyStatus(makeReply(1)) == Status::Ok
						&& getAppReplyStatus(refused) == Status::ErrorNotImplemented,
				"replytable: an app reply is Ok, a refusal without a handler is NotImplemented");
	}
}

} // namespace stappler::xenolith::remote
