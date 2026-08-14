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

/* The standard data::Model over a directory tree: hand one to a TreeView or a TableView and it
shows the filesystem, lazily, with the moves and the deletions actually happening on disk.

WHAT IT ADDS TO data::Model. A Model is a structure with an opaque `Ref` per element and a set of
slots that turn a mutation into an external action; it deliberately knows nothing about what those
elements are. This binds all of it to one concrete outside world: a directory becomes a Category, a
file becomes an Item, the opaque object becomes a FileRef holding the path and the stat, and the
slots become rename(2), a listing refresh and the shell's Trash.

LISTING IS LAZY AND IDENTITY-PRESERVING. A directory is walked on its first expand, and re-walked by
refresh(). A re-walk is a DIFF, not a rebuild: an entry that is still there keeps the node it had,
which means it keeps its ItemId, and therefore keeps its expansion, its selection and its whole open
subtree. That is what makes refresh() usable on a directory the user is looking at — and what makes
a move into a directory that was never listed correct without any special case, since the later
listing recognizes the node that is already there instead of adding a second one.

ORDER IS THE MODEL'S. Directories and files are sorted TOGETHER by one comparator and interleaved in
the result; `setDirectoriesFirst(true)` is a choice, not the only shape available. (A data::Source
could not express the interleaved form at all — subcategories were a separate list that always came
first.)

THE APP THREAD, OR NOT. By default a directory is walked inline, because a local `getdents` is
faster than the frame it would be deferred to. `setAsyncEnabled(true)` moves the walk to the worker
pool for trees that may live on a network mount, where the same call can block for seconds; the
model then fills in when the answer lands, and a branch refreshed in the meantime discards the
stale answer rather than showing it (Node::getChildsGeneration).

EVERY NODE'S VALUE carries `name`, `path`, `dir`, and — unless setStatEnabled(false) — `size` and
`mtime` (microseconds). Anything more specific belongs to the caller: read the FileRef.

    auto model = Rc<ui::FilesystemModel>::create(FileInfo{path});
    model->setWindow(window); // needed only for Trash / Reveal
    tree->setSource(model);
*/
class SP_PUBLIC FilesystemModel : public data::Model {
public:
	using Node = data::Model::Node;
	using DialogType = sprt::window::DialogType;

	/* What a row stands for: the file itself, as of the last time it was listed.

	This is the opaque object a Model node carries beside its Value, and the reason Model has one — a
	move or a deletion acts on THIS, never on a path parsed back out of presentation data. It is also
	stable across a refresh: an entry that survives a re-listing keeps the same FileRef object, with
	its `stat` updated in place. */
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

	/* What removeNode() means on disk.

	Trash is the default deliberately. A tree widget with a Delete key wired to it is one keystroke
	away from an unrecoverable loss, and the shell's Trash is the one deletion the user can undo with
	something they already know how to use. Delete is a real unlink/rmdir -r and is opt-in. */
	enum class RemoveMode {
		Deny, // removal is refused outright: a read-only tree
		Trash, // sprt::window::DialogType::MoveToTrash — recoverable, needs a window
		Delete, // filesystem::remove, recursive for a directory
	};

	// Return false to leave the entry out of the listing entirely. Runs on the app thread, after the
	// walk, so it may do anything a listing may do.
	using FilterCallback = Function<bool(const FileRef &)>;

	// Strict weak ordering over one directory's entries. Replaces SortField; setDirectoriesFirst()
	// still applies FIRST, so a callback only has to answer for two entries of the same kind.
	using CompareCallback = Function<bool(const FileRef &, const FileRef &)>;

	// The file behind a node, or null for a node this model did not build.
	static FileRef *getFileRef(const Node *);
	static StringView getPath(const Node *);

	// A reasonable icon for a row: the folder pair for a directory, and a by-extension guess for a
	// file. A caller that knows its own file types overrides it in the row callback.
	static IconName getIcon(const Node *, bool expanded = false);

	virtual ~FilesystemModel();

	// An empty tree. Add the directories it shows with addRoot() — that is the "Places" shape, where
	// the model root is a synthetic container nobody sees.
	virtual bool init() override;

	// The model root IS this directory: row 0 of a tree with setRootVisible(true). `title` overrides
	// the displayed name, which defaults to the last path component — pass the whole path where the
	// root row has to say WHERE the tree is rooted rather than just what the folder is called.
	virtual bool init(const FileInfo &root, StringView title = StringView());

	// Show another directory as a child of the model root. That is the "Places" shape: several
	// unrelated directories side by side, each with its own label.
	Node *addRoot(const FileInfo &, StringView title = StringView());

	// --- listing ---------------------------------------------------------------------------------
	//
	// These describe how the NEXT listing is built. Directories already on screen keep what they
	// have until refresh() re-walks them — which is a diff, so changing the sort does not cost the
	// user their expanded subtrees.

	void setShowHidden(bool);
	bool isShowHidden() const { return _showHidden; }

	// false: directories only, which is the whole of a folder picker.
	void setShowFiles(bool);
	bool isShowFiles() const { return _showFiles; }

	// One stat() per entry, for `size` and `mtime`. On by default: a file table without them is
	// rarely what anyone wanted, and the call is cheap next to the getdents that found the entry.
	void setStatEnabled(bool);
	bool isStatEnabled() const { return _stat; }

	void setFilterCallback(FilterCallback &&);

	void setSortField(SortField, bool ascending = true);
	SortField getSortField() const { return _sortField; }
	bool isSortAscending() const { return _sortAscending; }

	void setDirectoriesFirst(bool);
	bool isDirectoriesFirst() const { return _dirsFirst; }

	void setCompareCallback(CompareCallback &&);

	// Walk directories on the worker pool instead of inline. Correct either way; it only matters
	// where a listing can block — a network mount, a spun-down disk, a directory with 100k entries.
	void setAsyncEnabled(bool);
	bool isAsyncEnabled() const { return _async; }

	// --- actions ---------------------------------------------------------------------------------
	//
	// These tune the slots this model installs on itself. A caller with entirely different ideas
	// about what a move means calls Model::setSlots() and replaces them wholesale.

	void setMoveEnabled(bool);
	bool isMoveEnabled() const { return _moveEnabled; }

	void setRemoveMode(RemoveMode);
	RemoveMode getRemoveMode() const { return _removeMode; }

	// false (the default): a move onto an existing name is refused rather than silently clobbering
	// it, because filesystem::move renames over its destination without asking.
	void setOverwriteEnabled(bool);
	bool isOverwriteEnabled() const { return _overwrite; }

	/* The window a Trash or a Reveal is parented to. Not owned, and it cannot be: the window owns
	the scene that owns the view that owns this model, so an Rc here would close that loop.

	Without one, RemoveMode::Trash and revealNode() answer ErrorInvalidArguemnt — they are OS shell
	actions and the OS wants to know which window asked. */
	void setWindow(AppWindow *);
	AppWindow *getWindow() const { return _window; }

	// --- operations ------------------------------------------------------------------------------

	// Among the nodes that have been LISTED — an entry inside a directory nobody has opened does not
	// exist yet as far as the model is concerned. Descends by path prefix, so it costs the depth of
	// the tree rather than its size.
	Node *getNodeForPath(StringView) const;

	/* Rename in place. Synchronous, because rename(2) is: it either happened by the time this
	returns false or it did not happen at all.

	The node keeps its id, so a renamed row stays selected and stays open. */
	bool renameNode(Node *, StringView newName);

	// mkdir + the node for it. Null if the directory could not be created, or if one of that name is
	// already listed there. `parent` null means the model root.
	Node *createDirectory(Node *parent, StringView name);

	// Ask the desktop's file manager to show this entry. Needs a window; see setWindow().
	bool revealNode(Node *);

	/* Re-walk one directory and reconcile it with what is on screen.

	Entries that are still there keep their nodes — and therefore their ids, their expansion and
	their open subtrees; entries that are gone are removed; new ones are inserted in sorted order.
	A directory that has never been listed is left alone: there is nothing to reconcile, and walking
	it here would defeat the laziness.

	This is the call a file-system watcher drives. */
	void refresh(Node *dir);

	// Every directory that has been listed, from the root down.
	void refreshAll();

protected:
	using data::Model::init;

	// One directory entry, as the walk produced it. Deliberately plain data with no Ref in it: on
	// the async path this is what crosses from the worker back to the app thread.
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

	// The walk itself. Static and touching nothing but the filesystem, because on the async path it
	// runs on a worker thread and the model is app-thread-only.
	static void readDirectory(StringView path, bool withStat, Vector<Entry> &out);

	// Reconcile `dir`'s children with a fresh listing: keep what survived, drop what is gone, add
	// what is new, then order the result. This is both the first load and every refresh.
	void applyListing(Node *dir, Vector<Entry> &&);

	bool isVisible(const FileRef &) const;
	bool compare(const FileRef &, const FileRef &) const;
	void sortDirectory(Node *dir);

	static Value makeValue(const FileRef &);

	// Push the FileRef's current path/name/stat into the node's Value, but only when it actually
	// differs: setNodeData bumps the revision, and a revision that changes for nothing rebuilds a
	// row node for nothing.
	void updateNode(Node *, const FileRef &);

	// A moved directory's descendants were listed under the old path. Rewriting them — rather than
	// dropping them with resetChilds() — is what keeps the subtree the user had open, open.
	void rebaseSubtree(Node *, StringView oldPrefix, StringView newPrefix);

	void performMove(Node *, Node *dstParent, CompletionCallback &&);
	void performRemove(Node *, CompletionCallback &&);

	// Both shell actions ride the same seam; the request doubles as the cancellation token, so it is
	// parked in _dialogs until its completion has run.
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
