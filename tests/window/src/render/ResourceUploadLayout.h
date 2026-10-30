#ifndef TESTS_WINDOW_SRC_RENDER_RESOURCEUPLOADLAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_RESOURCEUPLOADLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class ResourceUploadLayout : public TestLayout {
public:
	bool init() override;
	void handleEnter(Scene *) override;
	void handleContentSizeDirty() override;

protected:
	void registerCommands() override;
	basic2d::Sprite *_sprite = nullptr;
	Rc<Texture> _texture;
};

} // namespace stappler::xenolith::app

#endif
