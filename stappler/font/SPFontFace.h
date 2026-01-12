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

#ifndef CORE_FONT_XLFONTFACE_H_
#define CORE_FONT_XLFONTFACE_H_

#include "SPFontTextLayout.h"

typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;

namespace STAPPLER_VERSIONIZED stappler::font {

class FontLibrary;

class SP_PUBLIC FontFaceData : public Ref, public InterfaceObject<memory::StandartInterface> {
public:
	virtual ~FontFaceData() = default;

	bool init(StringView, BytesView, bool);
	bool init(StringView, Bytes &&);
	bool init(StringView, Function<Bytes()> &&);

	FontLayoutParameters acquireDefaultParams(FT_Face);

	void inspectVariableFont(FontLayoutParameters params, FT_Library lib, FT_Face);

	StringView getName() const { return _name; }
	BytesView getView() const;

	const FontVariations &getVariations() const { return _variations; }

	FontSpecializationVector getSpecialization(const FontSpecializationVector &) const;

protected:
	bool _persistent = false;
	String _name;
	BytesView _view;
	Bytes _data;
	FontVariations _variations;
	FontLayoutParameters _params;
};

class SP_PUBLIC FontFaceObject : public Ref, public InterfaceObject<memory::StandartInterface> {
public:
	virtual ~FontFaceObject() = default;

	bool init(StringView, const Rc<FontFaceData> &, FT_Library, FT_Face,
			const FontSpecializationVector &, uint16_t, uint16_t plane = 0);

	StringView getName() const { return _name; }
	uint16_t getId() const { return _id; }
	uint16_t getPlane() const { return _plane; }
	FT_Face getFace() const { return _face; }

	// get char id on FontFaceObject's plane
	char16_t getCharId(char32_t) const;

	const Rc<FontFaceData> &getData() const { return _data; }
	const FontSpecializationVector &getSpec() const { return _spec; }

	bool acquireTexture(char32_t, const Callback<void(const CharTexture &)> &);
	bool acquireTextureUnsafe(char32_t, const Callback<void(const CharTexture &)> &);

	// returns true if updated
	bool addChars(const Vector<char32_t> &chars, bool expand, Vector<char32_t> *failed);
	bool addCharGroup(CharGroupId, Interface::VectorType<char32_t> *failed);

	// plane will be added for char automatically
	bool addRequiredChar(char32_t);

	Interface::VectorType<char32_t> getRequiredChars() const;
	size_t getRequiredCharsCount() const;

	CharShape getChar(char32_t c) const;
	int16_t getKerningAmount(char32_t first, char32_t second) const;

	Metrics getMetrics() const { return _metrics; }

protected:
	bool addChar(char16_t, bool &updated);

	Interface::StringType _name;
	Rc<FontFaceData> _data;
	uint16_t _id = 0;
	uint16_t _plane = 0;
	FT_Face _face = nullptr;
	FontSpecializationVector _spec;
	Metrics _metrics;
	Interface::VectorType<char32_t> _required;
	FontCharStorage<CharShape16> _chars;
	mem_std::HashMap<uint32_t, int16_t> _kerning;
	mem_std::Mutex _faceMutex;
	mutable std::shared_mutex _charsMutex;
	mutable mem_std::Mutex _requiredMutex;
};

class SP_PUBLIC FontFaceSet : public Ref, public InterfaceObject<memory::StandartInterface> {
public:
	static String constructName(StringView, const FontSpecializationVector &);

	virtual ~FontFaceSet() = default;
	FontFaceSet() { }

	bool init(String &&, StringView family, FontSpecializationVector &&, Rc<FontFaceData> &&data,
			FontLibrary *);
	bool init(String &&, StringView family, FontSpecializationVector &&,
			Vector<Rc<FontFaceData>> &&data, FontLibrary *);

	void touch(uint64_t clock, bool persistent);

	uint64_t getAccessTime() const { return _accessTime; }
	bool isPersistent() const { return _persistent; }

	StringView getName() const { return _name; }
	StringView getFamily() const { return _family; }

	const FontSpecializationVector &getSpec() const { return _spec; }

	size_t getFaceCount() const;

	Rc<FontFaceData> getSource(size_t) const;
	FontLibrary *getLibrary() const { return _library; }

	bool addString(const CharVector &);
	bool addString(const CharVector &, Vector<char32_t> &failed);

	uint16_t getFontHeight() const;
	int16_t getKerningAmount(char32_t first, char32_t second, uint16_t face) const;
	Metrics getMetrics() const;
	CharShape getChar(char32_t, uint16_t &face) const;

	size_t getRequiredCharsCount() const;

	bool addTextureChars(SpanView<CharLayoutData>) const;

	const Vector<Rc<FontFaceObject>> &getFaces() const;

protected:
	std::atomic<uint64_t> _accessTime;
	std::atomic<bool> _persistent = false;

	String _name;
	String _family;
	Metrics _metrics;
	FontSpecializationVector _spec;
	Vector<Rc<FontFaceData>> _sources;
	Vector<Rc<FontFaceObject>> _faces;
	FontLibrary *_library = nullptr;

	mutable size_t _texturesCount = 0;
	mutable std::shared_mutex _mutex;
};

} // namespace stappler::font

#endif /* CORE_FONT_XLFONTFACE_H_ */
