/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_FONT_XLFONTCONTROLLER_H_
#define XENOLITH_FONT_XLFONTCONTROLLER_H_

#include "XLFontConfig.h" // IWYU pragma: keep
#include "XLEvent.h"
#include "XLResourceCache.h"
#include "XLApplicationExtension.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class FontComponent;

struct SP_PUBLIC FontUpdateRequest {
	Rc<FontFaceObject> object;
	Vector<char32_t> chars;
	bool persistent = false;
};

class SP_PUBLIC FontController : public ApplicationExtension {
public:
	static EventHeader onLoaded;
	static EventHeader onFontSourceUpdated;

	struct FontSource {
		String fontFilePath;
		Bytes fontMemoryData;
		BytesView fontExternalData;
		Function<Bytes()> fontCallback;
		Rc<FontFaceData> data;
		FontLayoutParameters params;
		bool preconfiguredParams = true;
	};

	struct FamilyQuery {
		String family;
		Vector<const FontSource *> sources;
		bool addInFront = false;
	};

	struct FamilySpec {
		Vector<Rc<FontFaceData>> data;
	};

	class Builder {
	public:
		struct Data;

		~Builder();

		Builder(StringView);
		Builder(FontController *);

		Builder(Builder &&);
		Builder &operator=(Builder &&);

		Builder(const Builder &) = delete;
		Builder &operator=(const Builder &) = delete;

		StringView getName() const;
		FontController *getTarget() const;

		const FontSource *addFontSource(StringView name, BytesView data);
		const FontSource *addFontSource(StringView name, Bytes &&data);
		const FontSource *addFontSource(StringView name, const FileInfo &data);
		const FontSource *addFontSource(StringView name, Function<Bytes()> &&cb);

		const FontSource *addFontSource(StringView name, BytesView data, FontLayoutParameters);
		const FontSource *addFontSource(StringView name, Bytes &&data, FontLayoutParameters);
		const FontSource *addFontSource(StringView name, const FileInfo &data,
				FontLayoutParameters);
		const FontSource *addFontSource(StringView name, Function<Bytes()> &&cb,
				FontLayoutParameters);

		const FontSource *getFontSource(StringView) const;

		const FamilyQuery *addFontFaceQuery(StringView family, const FontSource *,
				bool front = false);
		const FamilyQuery *addFontFaceQuery(StringView family, Vector<const FontSource *> &&,
				bool front = false);

		bool addAlias(StringView newAlias, StringView familyName);

		Vector<const FamilyQuery *> getFontFamily(StringView family) const;

		Map<String, FontSource> &getDataQueries();
		Map<String, FamilyQuery> &getFamilyQueries();
		Map<String, String> &getAliases();

		Data *getData() const { return _data; }

	protected:
		void addSources(FamilyQuery *, Vector<const FontSource *> &&, bool front);

		Data *_data;
	};

	virtual ~FontController();

	bool init(FontComponent *, StringView name);

	void extend(AppThread *app, const Callback<bool(FontController::Builder &)> &);

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	bool isLoaded() const { return _loaded; }
	const Rc<core::DynamicImage> &getImage() const { return _image; }
	const Rc<Texture> &getTexture() const { return _texture; }

	Rc<FontFaceSet> getLayout(FontParameters f);
	Rc<FontFaceSet> getLayoutForString(const FontParameters &f, const CharVector &);

	Rc<core::DependencyEvent> addTextureChars(const Rc<FontFaceSet> &, SpanView<CharLayoutData>);

	uint32_t getFamilyIndex(StringView) const;
	StringView getFamilyName(uint32_t idx) const;

	virtual void update(AppThread *, const UpdateTime &clock, bool) override;

protected:
	friend class FontComponent;

	void addFont(StringView family, Rc<FontFaceData> &&, bool front = false);
	void addFont(StringView family, Vector<Rc<FontFaceData>> &&, bool front = false);

	// replaces previous alias
	bool addAlias(StringView newAlias, StringView familyName);

	void setImage(Rc<core::DynamicImage> &&);
	void setLoaded(bool);

	void sendFontUpdatedEvent();

	// FontLayout * getFontLayout(const FontParameters &style);

	void setAliases(Map<String, String> &&);

	FontSpecializationVector findSpecialization(const FamilySpec &, const FontParameters &,
			Vector<Rc<FontFaceData>> *);
	void removeUnusedLayouts();

	void initDependency();

	bool _loaded = false;
	String _name;
	std::atomic<uint64_t> _clock;
	TimeInterval _unusedInterval = 100_msec;
	String _defaultFontFamily = "default";
	Rc<Texture> _texture;
	Rc<core::DynamicImage> _image;
	Rc<FontComponent> _component;

	Map<String, String> _aliases;
	Vector<StringView> _familiesNames;
	Map<String, FamilySpec> _families;
	HashMap<StringView, Rc<FontFaceSet>> _layouts;
	Rc<core::DependencyEvent> _dependency;

	bool _dirty = false;
	mutable std::shared_mutex _layoutSharedMutex;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTCONTROLLER_H_ */
