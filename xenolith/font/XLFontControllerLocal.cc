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

#include "XLFontControllerLocal.h"

#include "XLAppThread.h"
#include "XLTexture.h"
#include "XLFontComponent.h"
#include "XLFontGapi.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

FontControllerLocal::~FontControllerLocal() { invalidate(nullptr); }

bool FontControllerLocal::init(FontComponent *comp, StringView name, FontLibrary *lib) {
	_component = comp;
	// Local gAPI endpoint: the FontComponent (direct to the gl Loop / VkFontQueue).
	_gapi = comp;
	_name = name.str<Interface>();
	// Default to the component's shared library; a separate library isolates this controller's FaceId
	// space (the server's network-serving controller passes its own).
	_library = lib ? lib : comp->getLibrary();
	return true;
}

void FontControllerLocal::initialize(AppThread *app) {
	_image = FontComponent::makeInitialImage(_name);
	// gAPI endpoint: compile the atlas image on the gl Loop.
	_gapi->compileImage(_image, [app = Rc<AppThread>(app)](bool success) { });
	_texture = Rc<Texture>::create(_image);
}

void FontControllerLocal::invalidate(AppThread *) {
	if (_image) {
		// image need to be finalized to remove cycled refs
		_image->finalize();
		_image = nullptr;
	}
}

void FontControllerLocal::setImage(Rc<core::DynamicImage> &&image) {
	_image = sp::move(image);
	_texture = Rc<Texture>::create(_image);
}

void FontControllerLocal::submitGlyphs(AppThread *app, Vector<FontUpdateRequest> &&objects,
		Rc<core::DependencyEvent> &&dep) {
	_gapi->updateImage(app->getLooper(), _image, sp::move(objects), sp::move(dep),
			[app = Rc<AppThread>(app)](bool) {
		// perform views update
		app->wakeup();
	});
}

Rc<core::DependencyEvent> FontControllerLocal::makeDependency() {
	return Rc<core::DependencyEvent>::alloc(core::DependencyEvent::QueueSet{_component->getQueue()},
			"FontController");
}

void FontControllerLocal::applyBuilder(AppThread *app, Builder &&b) {
	_component->acquireController(app->getLooper(), move(b));
}

} // namespace stappler::xenolith::font
