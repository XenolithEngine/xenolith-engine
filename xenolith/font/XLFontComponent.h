/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_FONT_XLCOMPONENT_H_
#define XENOLITH_FONT_XLCOMPONENT_H_

#include "XLContext.h"
#include "XLFontController.h"
#include "XLCoreAttachment.h"
#include "SPFontLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class FontComponent;

struct SP_PUBLIC RenderFontInput : public core::AttachmentInputData {
	Rc<event::Looper> queue;
	Rc<core::DynamicImage> image;
	Rc<FontComponent> ext;
	Vector<FontUpdateRequest> requests;
	Function<void(const core::ImageInfoData &, BytesView)> output;
};

class SP_PUBLIC FontComponent : public ContextComponent {
public:
	using DefaultFontName = FontLibrary::DefaultFontName;

	static Rc<ContextComponent> createFontComponent(Context *);

	static Rc<FontController> createDefaultController(FontComponent *, event::Looper *looper,
			StringView);

	static FontController::Builder makeDefaultControllerBuilder(StringView);

	static Rc<core::DynamicImage> makeInitialImage(StringView name);

	virtual ~FontComponent() = default;

	bool init(Context *);

	Context *getContext() const { return _context; }
	FontLibrary *getLibrary() const { return _library; }
	core::Queue *getQueue() const { return _queue; }

	virtual void handleStart(Context *) override;
	virtual void handleStop(Context *) override;

	virtual void handleSystemNotification(Context *a, SystemNotification) override;

	virtual void update();

	Rc<FontController> acquireController(event::Looper *, FontController::Builder &&);

	// run font rendering query for DynamicImage
	// Looper will be used for async font rendering via FreeType
	// Rendering will use all of Looper's async threads, so, avoid to stall main (GL) looper
	void updateImage(event::Looper *, const Rc<core::DynamicImage> &, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&, Function<void(bool)> &&complete);

protected:
	void handleActivated();

	struct ImageQuery {
		Rc<event::Looper> looper;
		Rc<core::DynamicImage> image;
		Vector<FontUpdateRequest> chars;
		Rc<core::DependencyEvent> dependency;
		Function<void(bool)> complete;
	};

	bool _active = false;
	Context *_context = nullptr;
	Rc<FontLibrary> _library;
	Rc<core::Queue> _queue;
	Vector<ImageQuery> _pendingImageQueries;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLCOMPONENT_H_ */
