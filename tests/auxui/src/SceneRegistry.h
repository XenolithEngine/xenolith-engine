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

#ifndef TESTS_AUXUI_SRC_SCENEREGISTRY_H_
#define TESTS_AUXUI_SRC_SCENEREGISTRY_H_

#include "XLCommon.h"
#include "XL2dSceneLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class AuxBaseScene;

// Per-window-id content registry: when Root scene opens an auxiliary window it
// registers the builder under the same `WindowInfo::id` it passes to
// `Context::createWindow`. The auxiliary scene then asks the registry for that
// id to obtain its content. This keeps the factory signature (which has no
// WindowInfo parameter) free of inline scene-building logic.
//
// The registry is process-global and mutex-guarded; entries are erased when the
// auxiliary scene is torn down, so the same id can be reused for the next open.
class SceneRegistry {
public:
	// Builds the content layout that an auxiliary scene will push as its content.
	// Receives the new scene so the builder can wire dismissal (e.g. a menu item
	// that closes its own window) and read the WindowInfo::id it was registered for.
	using Builder = Function<Rc<basic2d::SceneLayout2d>(AuxBaseScene *scene, StringView id)>;

	// Register a builder under `id`. Overwrites any previous entry.
	static void set(StringView id, Builder &&);

	// Take ownership of the builder for `id` (removes it from the registry).
	// Returns null if nothing was registered — the caller falls back to a placeholder.
	static Builder take(StringView id);

	// Removes the entry without running it; used on cleanup paths.
	static void erase(StringView id);
};

} // namespace stappler::xenolith::app

#endif // TESTS_AUXUI_SRC_SCENEREGISTRY_H_
