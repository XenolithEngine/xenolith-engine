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

	Rc<FontLibrary> library;
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

	// One face of a loaded layout, as reported by enumerateLayouts.
	struct FaceInfo {
		StringView name;
		StringView source; // FontFaceData the face was opened from
		uint16_t id = 0;
		uint16_t plane = 0;
		FontFaceObject::Usage usage;
	};

	// One entry of _layouts: a FontFaceSet - one family at one specialization (size, style, weight,
	// stretch, grade and density) with the faces opened for it and their caches.
	struct LayoutInfo {
		StringView name; // the layout key: family.size.style.weight.stretch.grade.density
		StringView family;
		FontSpecializationVector spec;
		Metrics metrics;

		// Holders besides _layouts itself. Zero means the next removeUnusedLayouts() drops this
		// layout.
		uint32_t users = 0;
		bool persistent = false;
		uint64_t idleTime = 0; // microseconds since the last getLayout() touch

		SpanView<FaceInfo> faces;
	};

	// Controller-wide totals that go with the per-layout report.
	struct ControllerInfo {
		StringView name;
		bool loaded = false;
		bool dirty = false;
		size_t layouts = 0;
		size_t faces = 0;
		size_t families = 0;
		size_t aliases = 0;
		size_t chars = 0; // cached shaping entries across every face
		size_t charsMemory = 0;
		size_t kerningPairs = 0;
		size_t requiredChars = 0; // glyphs the atlas is asked to hold
		/* Number of batches submitted (each gates a frame); unlike `glyphGeneration`, which counts
		requests, this is machine-independent and assertable. */
		uint64_t batches = 0;
		uint64_t glyphGeneration = 0;
		uint64_t submittedGeneration = 0;
		uint64_t uploadedGeneration = 0;
		uint32_t uploadsInFlight = 0;
		uint32_t atlasWidth = 0;
		uint32_t atlasHeight = 0;

		// What the eviction policy sees: `cachePressure` is compared against `evictionThreshold`.
		// `atlasOccupancy` is a diagnostic, not the gate (see config::FontCacheAtlasBudget);
		// negative when there is no atlas image.
		uint64_t atlasBytes = 0;
		uint64_t atlasBudget = 0;
		float cachePressure = -1.0f;
		float atlasOccupancy = -1.0f;
		float evictionThreshold = 0.0f;
		bool evictAlways = false;
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

	// Reads the eviction debug knobs from the environment; the rest is set up by the leaf's
	// initialize().
	FontController();

	virtual ~FontController() = default;

	// Re-apply an extend() Builder against this controller. The base assembles the Builder; the
	// leaf (applyBuilder) routes it to its source loader (local: FontComponent::acquireController).
	void extend(AppThread *app, const Callback<bool(FontController::Builder &)> &);

	// GPU touchpoints implemented by the concrete leaf: local (FontComponentLocal -> gl Loop) or
	// remote (FontControllerRemote -> server). The base owns only positioning and source state.
	virtual void initialize(AppThread *) override = 0;
	virtual void invalidate(AppThread *) override = 0;

	bool isLoaded() const { return _loaded; }
	virtual const Rc<core::DynamicImage> &getImage() const = 0;
	virtual const Rc<Texture> &getTexture() const = 0;

	Rc<FontFaceSet> getLayout(FontParameters f);
	Rc<FontFaceSet> getLayoutForString(const FontParameters &f, const CharVector &);

	Rc<core::DependencyEvent> addTextureChars(const Rc<FontFaceSet> &, SpanView<CharLayoutData>);

	// The glyph set a node laid out against. Record it at layout time and pass it to
	// isGlyphGenerationUploaded() every later frame: the shader resolves CharIds through whatever
	// atlas instance is current, so vertex data cannot tell whether the glyphs are still there.
	uint64_t getGlyphGeneration() const { return _glyphGeneration; }

	// True when everything required up to `gen` is confirmed present in the atlas and nothing is
	// being uploaded. False means a node that laid out at `gen` must gate its frames.
	bool isGlyphGenerationUploaded(uint64_t gen) const {
		return _uploadsInFlight.load() == 0 && _uploadedGeneration.load() >= gen;
	}

	// A dependency to hold frames back until the atlas catches up, for a node whose generation
	// isGlyphGenerationUploaded() rejects. If some laid-out glyph is not sent yet, a batch is
	// opened (or the accumulating one extended); otherwise the batch in flight is returned, since a
	// flush submits the whole required set. Never mint a batch unconditionally: labels re-arm every
	// frame, and the in-flight upload would always have a queued successor.
	Rc<core::DependencyEvent> acquireGatingDependency();

	uint32_t getFamilyIndex(StringView) const;

	// Forget the font sets already built for a family, so the next request rebuilds them with
	// whatever faces it has now. Called by addFont; the lock is the caller's.
	void dropLayoutsForFamily(StringView family);
	StringView getFamilyName(uint32_t idx) const;

	// What is loaded right now (the inspector's `fonts` command). LayoutInfo points into live
	// layouts: valid only during the callback, under the shared lock, and it holds no reference so
	// the report does not change what removeUnusedLayouts() drops.
	void enumerateLayouts(const Callback<void(const LayoutInfo &)> &) const;

	// The totals for the same walk. Counts every cached entry, so it is a report, not a per-frame
	// call.
	ControllerInfo getControllerInfo() const;

	// How full the glyph cache is, 0..1 (past 1 when a budget is exceeded); a font set nobody holds
	// is kept until this crosses the threshold. The higher of: atlas image size against
	// config::FontCacheAtlasBudget, and live set count against config::FontCacheMaxLayouts (the
	// only bound without a local atlas - the software rasterizer and a remote client).
	virtual float getCachePressure() const;

	// Debug: drop every unused font set on every update, ignoring the threshold. Seeded from
	// XL_FONT_EVICT_ALWAYS.
	void setEvictAlways(bool value) { _evictAlways.store(value); }
	bool isEvictAlways() const { return _evictAlways.load(); }

	// The threshold getCachePressure() is compared against. Defaults to
	// config::FontCacheEvictionThreshold, overridable at runtime and from XL_FONT_EVICT_THRESHOLD.
	void setEvictionThreshold(float value) { _evictionThreshold = value; }
	float getEvictionThreshold() const { return _evictionThreshold; }

	virtual void update(AppThread *, const UpdateTime &clock, bool) override;

	// Submit pending (dirty) glyphs to the gAPI endpoint now, gated by the current dependency.
	// Called from update(); the remote client also calls it before sending a FrameInput, so the
	// server registers the gating dependency before the frame references it.
	virtual void flushPendingGlyphs(AppThread *);

	// Route a remote::Domain::Font notification addressed at this controller (AtlasReady, etc.).
	// The base ignores it; FontControllerRemote drives the client protocol. Generic signature so
	// the base needs no remote:: types.
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

	// Forget every submission, so the next flush sends the full set again. For a failed batch:
	// the flush skips a set that has not grown, so nothing else would re-send it. App thread.
	void resetSubmittedGlyphs();

	// Leaf hooks. submitGlyphs hands glyph-raster requests (+ the gating dependency) to the leaf's
	// gAPI endpoint (local: FontComponent -> gl Loop / VkFontQueue; remote: proxy -> server).
	// makeDependency builds the DependencyEvent gating that atlas update. applyBuilder routes an
	// extend() Builder to the leaf's source loader.
	virtual void submitGlyphs(AppThread *, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&) = 0;
	virtual Rc<core::DependencyEvent> makeDependency() = 0;

	// Submissions, not requests - see ControllerInfo::batches.
	uint64_t _submittedBatches = 0;
	virtual void applyBuilder(AppThread *app, Builder &&) = 0;

	bool _loaded = false;
	String _name;
	sprt::atomic<uint64_t> _clock;
	String _defaultFontFamily = "default";

	// Eviction policy, seeded from the environment by the constructor. Atomic because a setter may
	// be driven from a socket hop, while removeUnusedLayouts() reads it on the app thread.
	sprt::atomic<bool> _evictAlways;
	float _evictionThreshold;

	// FreeType library used to open faces for metrics/layout. Provided by the leaf (local: the
	// FontComponent's shared library; remote: the controller's own headless library).
	FontLibrary *_library = nullptr;

	Map<String, String> _aliases;
	Vector<StringView> _familiesNames;
	Map<String, FamilySpec> _families;
	HashMap<StringView, Rc<FontFaceSet>> _layouts;
	// The batch being accumulated for the next flush. Handed to submitGlyphs() and dropped there,
	// so it reaches the frames built before that flush and never a later one.
	Rc<core::DependencyEvent> _dependency;

	// The last batch handed to the rasterizer, kept until it signals. A caller with nothing new to
	// send waits on it (acquireGatingDependency) instead of opening another batch. Dropped as soon
	// as it fires.
	Rc<core::DependencyEvent> _submittedDependency;

	// Whether every required glyph is in the atlas the shader samples (FontFaceObject::_required
	// is process-wide and cannot answer that). _glyphGeneration is bumped when any face gains a
	// required glyph; a batch carries the generation current at submission. A generation is
	// confirmed only once nothing is in flight: batches overlap and can land out of order.
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
