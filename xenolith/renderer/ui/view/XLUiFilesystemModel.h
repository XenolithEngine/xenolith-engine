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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUIFILESYSTEMMODEL_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUIFILESYSTEMMODEL_H_

#include "SPDataModel.h"
#include "SPFilesystem.h"
#include "XLUiConfig.h" // `using namespace basic2d` for this namespace
#include "XL2dIconSprite.h" // IconName

#include <sprt/runtime/window/dialog.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A data::Model over a directory tree, for TreeView or TableView. Directories are Categories,
files are Items, each node carries a FileRef, and the slots perform rename(2), refresh and Trash.

A directory is listed on first expand; refresh() diffs, so surviving entries keep their node and
ItemId (expansion, selection, open subtree). Directories and files are sorted together unless
setDirectoriesFirst(true). Listing is inline by default; setAsyncEnabled(true) walks on the worker
pool, and a stale answer for a refreshed branch is dropped (Node::getChildsGeneration).

Each node's Value has `name`, `path`, `dir`, and, unless setStatEnabled(false), `size` and
`mtime` (microseconds).

    auto model = Rc<ui::FilesystemModel>::create(FileInfo{path});
    model->setWindow(window); // needed only for Trash / Reveal
    tree->setSource(model);
*/
class SP_PUBLIC FilesystemModel : public data::Model {
public:
	using Node = data::Model::Node;
	using DialogType = sprt::window::DialogType;

	/* The file behind a node, as of its last listing. Moves and deletions act on this, not on the
	Value. The same object survives a refresh, with `stat` updated in place. */
	struct SP_PUBLIC FileRef : public Ref {
		String path; // absolute, POSIX form, as everywhere else in the runtime
		String name; // what the row shows: the last component, or a root's given title
		filesystem::Stat stat;
		bool dir = false;

		bool isHidden() const { return !name.empty() && name.front() == '.'; }
	};

	enum class SortField {
		None, // whatever order the directory walk produced
		Name, // case-insensitive, unicode-aware
		Size,
		Time, // mtime
		Extension, // then by name within one extension
	};

	// What removeNode() does on disk. Trash (recoverable) is the default; Delete is opt-in.
	enum class RemoveMode {
		Deny, // removal is refused outright: a read-only tree
		Trash, // sprt::window::DialogType::MoveToTrash — recoverable, needs a window
		Delete, // filesystem::remove, recursive for a directory
	};

	// Return false to leave the entry out of the listing. Runs on the app thread, after the walk.
	using FilterCallback = Function<bool(const FileRef &)>;

	// Strict weak ordering over one directory's entries. Replaces SortField; setDirectoriesFirst()
	// still applies first.
	using CompareCallback = Function<bool(const FileRef &, const FileRef &)>;

	// The file behind a node, or null for a node this model did not build.
	static FileRef *getFileRef(const Node *);
	static StringView getPath(const Node *);

	// A default row icon: the folder pair for a directory, a by-extension guess for a file.
	static IconName getIcon(const Node *, bool expanded = false);

	virtual ~FilesystemModel();

	// An empty tree with a hidden synthetic root; add directories with addRoot().
	virtual bool init() override;

	// The model root is this directory. `title` overrides the displayed name, which defaults to the
	// last path component.
	virtual bool init(const FileInfo &root, StringView title = StringView());

	// Add another directory as a child of the model root, with its own label.
	Node *addRoot(const FileInfo &, StringView title = StringView());

	// --- listing ---------------------------------------------------------------------------------
	//
	// These apply to the next listing; listed directories change on refresh().

	void setShowHidden(bool);
	bool isShowHidden() const { return _showHidden; }

	// false: directories only.
	void setShowFiles(bool);
	bool isShowFiles() const { return _showFiles; }

	// One stat() per entry, for `size` and `mtime`. On by default.
	void setStatEnabled(bool);
	bool isStatEnabled() const { return _stat; }

	void setFilterCallback(FilterCallback &&);

	void setSortField(SortField, bool ascending = true);
	SortField getSortField() const { return _sortField; }
	bool isSortAscending() const { return _sortAscending; }

	void setDirectoriesFirst(bool);
	bool isDirectoriesFirst() const { return _dirsFirst; }

	void setCompareCallback(CompareCallback &&);

	// Walk directories on the worker pool instead of inline, for listings that may block.
	void setAsyncEnabled(bool);
	bool isAsyncEnabled() const { return _async; }

	// --- actions ---------------------------------------------------------------------------------
	//
	// These tune the slots this model installs; Model::setSlots() replaces them entirely.

	void setMoveEnabled(bool);
	bool isMoveEnabled() const { return _moveEnabled; }

	void setRemoveMode(RemoveMode);
	RemoveMode getRemoveMode() const { return _removeMode; }

	// false (the default): a move onto an existing name is refused, since filesystem::move
	// overwrites its destination.
	void setOverwriteEnabled(bool);
	bool isOverwriteEnabled() const { return _overwrite; }

	/* The window a Trash or a Reveal is parented to. Not owned (the window indirectly owns this
	model). Without one, RemoveMode::Trash and revealNode() fail with ErrorInvalidArguemnt. */
	void setWindow(AppWindow *);
	AppWindow *getWindow() const { return _window; }

	// --- operations ------------------------------------------------------------------------------

	// Only among listed nodes. Descends by path prefix.
	Node *getNodeForPath(StringView) const;

	// Rename in place, synchronously. The node keeps its id, so the row stays selected and open.
	bool renameNode(Node *, StringView newName);

	// mkdir + the node for it. Null if the directory could not be created, or if one of that name is
	// already listed there. `parent` null means the model root.
	Node *createDirectory(Node *parent, StringView name);

	// Ask the desktop's file manager to show this entry. Needs a window; see setWindow().
	bool revealNode(Node *);

	/* Re-walk one directory and reconcile its children: survivors keep their nodes, gone entries
	are removed, new ones inserted in order. A never-listed directory is left alone. */
	void refresh(Node *dir);

	// Every directory that has been listed, from the root down.
	void refreshAll();

protected:
	using data::Model::init;

	// One directory entry from the walk. Plain data: it crosses from the worker to the app thread.
	struct Entry {
		String path;
		String name;
		filesystem::Stat stat;
		bool dir = false;
	};

	// Shared tail of both init()s: installs the slots that make this a filesystem model.
	bool initFilesystem();

	// Attach a directory (with its lazy-children callback) or a file to `parent`.
	Node *emplaceEntry(Node *parent, size_t index, Rc<FileRef> &&);

	// Kick off a listing of `dir` — inline or on the worker pool — and call `complete` when the
	// children are in. Always calls it exactly once, including on failure.
	void requestListing(Node *dir, Function<void()> &&complete);

	// The walk itself. Static: it may run on a worker thread, and the model is app-thread-only.
	static void readDirectory(StringView path, bool withStat, Vector<Entry> &out);

	// Reconcile `dir`'s children with a fresh listing, then sort. Used for first load and refresh.
	void applyListing(Node *dir, Vector<Entry> &&);

	bool isVisible(const FileRef &) const;
	bool compare(const FileRef &, const FileRef &) const;
	void sortDirectory(Node *dir);

	static Value makeValue(const FileRef &);

	// Push the FileRef's path/name/stat into the node's Value only when it differs: setNodeData
	// bumps the revision, which rebuilds the row.
	void updateNode(Node *, const FileRef &);

	// Rewrite the paths of a moved directory's listed descendants, keeping the subtree open.
	void rebaseSubtree(Node *, StringView oldPrefix, StringView newPrefix);

	void performMove(Node *, Node *dstParent, CompletionCallback &&);
	void performRemove(Node *, CompletionCallback &&);

	// Runs a shell action; the request is the cancellation token and stays in _dialogs until its
	// completion has run.
	Status openShellDialog(DialogType, StringView path, CompletionCallback &&);

	FilterCallback _filterCallback;
	CompareCallback _compareCallback;

	Vector<Rc<sprt::window::DialogRequest>> _dialogs;

	AppWindow *_window = nullptr;

	SortField _sortField = SortField::Name;
	RemoveMode _removeMode = RemoveMode::Trash;

	bool _sortAscending = true;
	bool _dirsFirst = false;
	bool _showHidden = false;
	bool _showFiles = true;
	bool _stat = true;
	bool _async = false;
	bool _moveEnabled = true;
	bool _overwrite = false;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUIFILESYSTEMMODEL_H_ */
