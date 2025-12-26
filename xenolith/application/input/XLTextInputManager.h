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

#ifndef XENOLITH_APPLICATION_INPUT_XLTEXTINPUTMANAGER_H_
#define XENOLITH_APPLICATION_INPUT_XLTEXTINPUTMANAGER_H_

#include "XLInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;
class TextInputManager;

// TextInputHandler used to capture text input
// - Only one handler can be active for one view, new running handler will displace old one
// - Owner is responsible for handler's existence and callback's correctness when handler
// is active (attached to manager)
// - setString/setCursor has no meaning if handler is not active
// - Whole input string from handler will be transferred to device input manager, so, try to
// keep it small (e.g. when working with paragraphs, send only current paragraph). Large
// strings can significantly reduce performance
struct SP_PUBLIC TextInputHandler {
	Function<void(const TextInputState &)> onData;

	Rc<TextInputManager> manager;

	~TextInputHandler();

	bool run(TextInputManager *, TextInputRequest &&);
	void cancel();

	// only if this handler is active
	bool update(TextInputRequest &&);

	WideStringView getString() const;
	TextCursor getCursor() const;
	TextCursor getMarked() const;

	bool isActive() const;
};

class SP_PUBLIC TextInputManager final : public Ref {
public:
	virtual ~TextInputManager() = default;

	bool init(NotNull<Director>);

	bool run(TextInputHandler *, TextInputRequest &&);
	bool update(TextInputHandler *, TextInputRequest &&);

	WideStringView getStringByRange(TextCursor);
	WideStringView getString() const;
	TextCursor getCursor() const;
	TextCursor getMarked() const;

	void cancel();

	bool isEnabled() const;

	TextInputHandler *getHandler() const { return _handler; }

	void handleInputUpdate(const TextInputState &);

protected:
	Director *_director = nullptr;
	TextInputHandler *_handler = nullptr;

	TextInputState _state;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLTEXTINPUTMANAGER_H_ */
