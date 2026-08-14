/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef XENOLITH_FONT_XLFONTCONTROLLER_H_
#define XENOLITH_FONT_XLFONTCONTROLLER_H_

#include "XLFontConfig.h" // IWYU pragma: keep
#include "XLEvent.h"
#include "XLResourceCache.h"
#include "XLApplicationExtension.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class FontComponent;
class FontGapi;

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

	virtual ~FontController() = default;

	// Re-apply an extend() Builder against this controller. The base assembles the Builder; the leaf
	// (applyBuilder) routes it to its source loader (local: FontComponent::acquireController).
	void extend(AppThread *app, const Callback<bool(FontController::Builder &)> &);

	// GPU touchpoints implemented by the concrete leaf: local (FontComponentLocal -> gl Loop) or remote
	// (FontControllerRemote -> server). The base owns only positioning + source state and never touches
	// the GPU directly.
	virtual void initialize(AppThread *) override = 0;
	virtual void invalidate(AppThread *) override = 0;

	bool isLoaded() const { return _loaded; }
	virtual const Rc<core::DynamicImage> &getImage() const = 0;
	virtual const Rc<Texture> &getTexture() const = 0;

	Rc<FontFaceSet> getLayout(FontParameters f);
	Rc<FontFaceSet> getLayoutForString(const FontParameters &f, const CharVector &);

	Rc<core::DependencyEvent> addTextureChars(const Rc<FontFaceSet> &, SpanView<CharLayoutData>);

	// The glyph set a node laid out against. Record it at layout time and hand it back to
	// isGlyphGenerationUploaded() on every later frame - a node cannot tell from its own vertex data
	// whether the atlas still holds its glyphs, because the shader resolves them by CharId through
	// whatever atlas instance is current when the frame runs.
	uint64_t getGlyphGeneration() const { return _glyphGeneration; }

	// True when everything required up to `gen` is confirmed present in the atlas and nothing is
	// being uploaded. False means a node that laid out at `gen` must gate its frames.
	bool isGlyphGenerationUploaded(uint64_t gen) const {
		return _uploadsInFlight.load() == 0 && _uploadedGeneration.load() >= gen;
	}

	// A dependency to hold frames back until the atlas catches up, for a node whose generation
	// isGlyphGenerationUploaded() rejects.
	//
	// Which event that is depends on whether anything is still waiting to be SENT. If some glyph has
	// been laid out but not yet handed to the rasterizer, the caller has to wait for the batch that
	// will carry it, so one is opened (or the accumulating one extended). If everything required is
	// already on its way, the caller waits for THAT batch instead - a flush always submits the whole
	// required set, so a batch in flight covers every glyph required when it left.
	//
	// The distinction is the whole point. Every label re-arms its gate on every frame for as long as
	// the atlas is behind, so minting a batch here unconditionally meant the in-flight upload always
	// had a successor queued before it landed: _uploadsInFlight never reached zero, the generation
	// never caught up, and the atlas was rebuilt (and every window's materials recompiled) six times
	// a second for as long as anything was on screen.
	Rc<core::DependencyEvent> acquireGatingDependency();

	uint32_t getFamilyIndex(StringView) const;
	StringView getFamilyName(uint32_t idx) const;

	virtual void update(AppThread *, const UpdateTime &clock, bool) override;

	// Submit any pending (dirty) glyphs to the gAPI endpoint now, gated by the current dependency. Called
	// from update() on the app-update cadence; the remote client also calls it from its frame-production
	// path (before the FrameInput is sent) so the server registers the gating dependency before the frame
	// references it -- otherwise the frame's reconcile finds nothing and the glyphs are not actually gated.
	virtual void flushPendingGlyphs(AppThread *);

	// Route a remote::Domain::Font notification addressed at this controller (server->client AtlasReady,
	// etc.). The base consumes-and-ignores; FontControllerRemote overrides to drive the client protocol.
	// Generic primitive signature so the base needs no remote:: types.
	virtual bool dispatchFontMessage(uint8_t code, uint32_t serial, BytesView payload) {
		return true;
	}

protected:
	friend class FontComponent;

	void addFont(StringView family, Rc<FontFaceData> &&, bool front = false);
	void addFont(StringView family, Vector<Rc<FontFaceData>> &&, bool front = false);

	// replaces previous alias
	bool addAlias(StringView newAlias, StringView familyName);

	void setLoaded(bool);

	void sendFontUpdatedEvent();

	// FontLayout * getFontLayout(const FontParameters &style);

	void setAliases(Map<String, String> &&);

	FontSpecializationVector findSpecialization(const FamilySpec &, const FontParameters &,
			Vector<Rc<FontFaceData>> *);
	void removeUnusedLayouts();

	void initDependency();

	// Is any face carrying a character that has not been handed to the rasterizer yet? App thread
	// (it walks _layouts under the shared lock). See FontFaceObject::hasPendingChars.
	bool hasPendingGlyphs() const;

	// Forget every submission, so the next flush sends the full set again. For a batch that failed:
	// its characters are required but no longer on their way, and nothing else would ever re-send
	// them - the flush skips a set that has not grown. App thread.
	void resetSubmittedGlyphs();

	// Leaf hooks. submitGlyphs hands a batch of glyph-raster requests (+ the gating dependency) to the
	// leaf's gAPI endpoint (local: FontComponent -> gl Loop / VkFontQueue; remote: proxy -> server).
	// makeDependency builds the DependencyEvent that gates the atlas update covering those glyphs
	// (local: signalled by the FontQueue; remote: reconciled + signalled on the server). applyBuilder
	// routes an extend() Builder to the leaf's source loader.
	virtual void submitGlyphs(AppThread *, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&) = 0;
	virtual Rc<core::DependencyEvent> makeDependency() = 0;
	virtual void applyBuilder(AppThread *app, Builder &&) = 0;

	bool _loaded = false;
	String _name;
	sprt::atomic<uint64_t> _clock;
	TimeInterval _unusedInterval = 100_msec;
	String _defaultFontFamily = "default";

	// FreeType library used to open faces for metrics/layout. Provided by the leaf (local: the
	// FontComponent's shared library; remote: the controller's own headless library).
	FontLibrary *_library = nullptr;

	Map<String, String> _aliases;
	Vector<StringView> _familiesNames;
	Map<String, FamilySpec> _families;
	HashMap<StringView, Rc<FontFaceSet>> _layouts;
	// The batch being accumulated for the next flush. Handed to submitGlyphs() and dropped there, so
	// it reaches the frames built before that flush and never a later one.
	Rc<core::DependencyEvent> _dependency;

	// The last batch that WAS handed to the rasterizer, kept until it signals. This is what a caller
	// with nothing new to send waits on (acquireGatingDependency): its glyphs are already inside it,
	// so re-using it costs nothing, while opening another batch costs a full atlas rebuild in every
	// window. Dropped as soon as it fires, so a caller never waits on a batch that has landed.
	Rc<core::DependencyEvent> _submittedDependency;

	// "Is every required glyph actually in the atlas the shader samples?" - the question
	// addTextureChars() has to answer, and the one FontFaceObject::_required cannot: that set is
	// permanent and process-wide, so it says "already required" to a window that has never had its
	// glyphs uploaded.
	//
	// _glyphGeneration is bumped whenever any face gains a new required glyph, and a batch carries
	// the generation that was current when it was submitted.
	//
	// A generation may only be declared present once NOTHING is in flight. Confirming it per batch
	// is wrong: flushes overlap (a batch is submitted every tick for as long as the atlas is
	// behind), and a later batch can reach the atlas first - it then confirmed a generation whose
	// own, slower batch was still rasterising, and the gate opened too early.
	uint64_t _glyphGeneration = 0;
	sprt::atomic<uint64_t> _submittedGeneration = 0;
	sprt::atomic<uint64_t> _uploadedGeneration = 0;
	sprt::atomic<uint32_t> _uploadsInFlight = 0;

	// Raised on the signalling thread when a batch reports failure, consumed by the next flush on
	// the app thread. Not a hop, just a note: the reset itself walks the layouts and belongs there.
	sprt::atomic<bool> _uploadFailed = false;

	bool _dirty = false;
	mutable sprt::shared_mutex _layoutSharedMutex;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTCONTROLLER_H_ */
