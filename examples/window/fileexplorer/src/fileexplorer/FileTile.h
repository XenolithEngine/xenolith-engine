/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILETILE_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILETILE_H_

#include "fileexplorer/ThumbnailCache.h"
#include "XLUiFilesystemModel.h"
#include "XLUiPanel.h"
#include "XL2dIconSprite.h"
#include "XL2dSprite.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

class IconGridView;

/* One cell of the icon grid: a picture and a name.

A tile is REUSED. Rows are recycled as the grid scrolls, so every piece of state a tile carries is
re-assigned by update() rather than set once in init(), and every answer a tile is waiting for is
matched against the epoch it was asked in - a thumbnail that arrives for the file this tile used to
show must not be drawn over the file it shows now.

What is under the picture depends on the entry:

  * a file gets its by-extension icon, and an image file gets a thumbnail over it once one is
    decoded. The icon stays until then, and comes back if the decode fails;
  * a directory gets the folder icon, with up to ThumbnailCache::FolderPreviewCount of the
    pictures inside it laid over its face. They are SIBLINGS of the icon rather than its children:
    each waits for its own texture and appears on its own, and the folder glyph is a VectorSprite
    that rasterizes deferred.

Every overlapping child is given an explicit ZOrder: a tile outlives the row it is in only by
being reused, and child insertion order is not what decides drawing order after that. */
class FileTile : public ui::Panel {
public:
	virtual ~FileTile() = default;

	virtual bool init(IconGridView *, ThumbnailCache *);

	virtual void handleContentSizeDirty() override;

	// The entry this tile shows from now on. The model node rather than its Value: the icon,
	// the path and the mtime all come from ui::FilesystemModel's own accessors. `index` is the
	// tile's place in the grid, which is what a tap reports back to the view.
	void update(ui::FilesystemModel::Node *, size_t index, float iconSize);

	void setSelected(bool);
	bool isSelected() const { return _selected; }

	ui::FilesystemModel::Node *getNode() const { return _node; }
	StringView getPath() const { return _path; }
	StringView getEntryName() const { return _name; }
	bool isDirectory() const { return _dir; }
	size_t getIndex() const { return _index; }

protected:
	void applyEntry();
	void requestThumbnail();
	void requestFolderPreview();

	void showThumbnail(Rc<Texture> &&);
	void showPreview(size_t slot, Rc<Texture> &&);

	IconGridView *_view = nullptr;
	ThumbnailCache *_cache = nullptr;

	basic2d::IconSprite *_icon = nullptr;
	basic2d::Sprite *_thumb = nullptr;
	Vector<basic2d::Sprite *> _previews;
	basic2d::Label *_label = nullptr;
	InputListener *_listener = nullptr;

	Rc<ui::FilesystemModel::Node> _node;
	String _path;
	String _name;
	int64_t _mtime = 0;
	size_t _index = 0;
	float _iconSize = 96.0f;

	// Bumped by every update(). A late answer carrying an older epoch belongs to an entry this
	// tile no longer shows.
	uint64_t _epoch = 0;

	bool _dir = false;
	bool _selected = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILETILE_H_
