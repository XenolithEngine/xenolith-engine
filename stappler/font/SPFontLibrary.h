/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_FONT_SPFONTLIBRARY_H_
#define CORE_FONT_SPFONTLIBRARY_H_

#include "SPFontFace.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::font {

class FontLibrary;

class SP_PUBLIC FontFaceObjectHandle : public Ref,
									   public InterfaceObject<memory::StandartInterface> {
public:
	virtual ~FontFaceObjectHandle();

	bool init(const Rc<FontLibrary> &, Rc<FontFaceObject> &&,
			Function<void(const FontFaceObjectHandle *)> &&onDestroy);

	FT_Face getFace() const { return _face->getFace(); }

	bool acquireTexture(char32_t, const Callback<void(const CharTexture &)> &);

protected:
	Rc<FontLibrary> _library;
	Rc<FontFaceObject> _face;
	Function<void(const FontFaceObjectHandle *)> _onDestroy;
};

class SP_PUBLIC FontLibrary : public Ref, public InterfaceObject<memory::StandartInterface> {
public:
	enum class DefaultFontName {
		None,
		RobotoFlex_VariableFont,
		RobotoMono_VariableFont,
		RobotoMono_Italic_VariableFont,
		DejaVuSans
	};

	struct FontData {
		bool persistent;
		BytesView view;
		Bytes bytes;
		Function<Bytes()> callback;

		FontData(BytesView v, bool p) : persistent(p) {
			if (persistent) {
				view = v;
			} else {
				bytes = v.bytes<Interface>();
				view = bytes;
			}
		}
		FontData(Bytes &&b) : persistent(false), bytes(sp::move(b)) { view = bytes; }
		FontData(Function<Bytes()> &&cb) : persistent(true), callback(sp::move(cb)) { }
	};

	static BytesView getFont(DefaultFontName);
	static StringView getFontName(DefaultFontName);

	FontLibrary();
	virtual ~FontLibrary();

	Rc<FontFaceData> openFontData(StringView, FontLayoutParameters params,
			bool isParamsPreconfigured, const Callback<FontData()> & = nullptr);

	Rc<FontFaceObject> openFontFace(StringView, const FontSpecializationVector &,
			const Callback<FontData()> &);
	Rc<FontFaceObject> openFontFace(const Rc<FontFaceData> &, const FontSpecializationVector &);

	void invalidate();

	void update();

	uint16_t getNextId();
	void releaseId(uint16_t);

	Rc<FontFaceObjectHandle> makeThreadHandle(const Rc<FontFaceObject> &);

protected:
	FT_Face newFontFace(BytesView);
	void doneFontFace(FT_Face);

	mem_std::Mutex _mutex;
	std::shared_mutex _sharedMutex;
	Map<StringView, Rc<FontFaceObject>> _faces;
	Map<StringView, Rc<FontFaceData>> _data;
	Map<FontFaceObject *, Map<thread::Thread::Id, Rc<FontFaceObjectHandle>>> _threads;
	FT_Library _library = nullptr;

	std::bitset<1'024 * 16> _fontIds;
};

} // namespace stappler::font

#endif /* CORE_FONT_SPFONTLIBRARY_H_ */
