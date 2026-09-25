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

#ifndef XENOLITH_REMOTE_XLREMOTEBEARERKEYS_H_
#define XENOLITH_REMOTE_XLREMOTEBEARERKEYS_H_

#include "XLRemoteProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// Bearer keys a server hands out with a label. A process the server launched presents its own key,
// and the session learns what it is from the label: identity comes from whoever issued the key, not
// from anything the client claims.
class SP_PUBLIC BearerKeyTable {
public:
	// Replaces a key added earlier with the same label.
	void add(BytesView key, StringView label, bool singleUse);

	bool remove(StringView label);

	// The label of the key `presented` equals. Every key is compared in full, so the time taken does
	// not depend on which one matched.
	bool match(BytesView presented, String &outLabel) const;

	// A session presenting `label` was established: a single-use key stops matching.
	void consume(StringView label);

	bool empty() const { return _entries.empty(); }
	size_t size() const { return _entries.size(); }

protected:
	struct Entry {
		Bytes key;
		String label;
		bool singleUse = true;
	};

	Vector<Entry> _entries;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEBEARERKEYS_H_ */
