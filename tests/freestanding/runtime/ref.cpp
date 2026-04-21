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

#include <sprt/runtime/stream.h>
#include <sprt/runtime/ref.h>

namespace sprt {

class InternalObject {
public:
	struct Data; // private platform data

	static Rc<SharedRef<InternalObject>> create(StringView key);

	~InternalObject() { }

	InternalObject(Ref *ref, memory::pool_t *p) : _ref(ref), _pool(p) { }

	bool init(StringView key) {
		_keyString = key.str<decltype(_keyString)>();
		return true;
	}

	Ref *getRef() const { return _ref; }

	memory::pool_t *getPool() const { return _pool; }

	StringView getKey() const { return _keyString; }

	bool validate() { return _keyString.get_allocator() == _pool; }

protected:
	Ref *_ref = nullptr;
	memory::pool_t *_pool = nullptr;
	__pool_string _keyString;
};

using InternalObjectRef = SharedRef<InternalObject>;

void performRefTests() {
	// Memory pool subsystem requires direct intialization and termination
	sprt::memory::pool::initialize();

	sprt::cout << "\n== runtime Ref/Rc tests ==\n";

	auto ref = Rc<InternalObjectRef>::create("TestLKey");

	sprt::cout << "InternalObjectRef::getKey(): " << ref->getKey() << "\n";
	sprt::cout << "InternalObjectRef::validate(): " << ref->validate() << "\n";

	ref = nullptr;

	sprt::memory::pool::terminate();
}

} // namespace sprt
