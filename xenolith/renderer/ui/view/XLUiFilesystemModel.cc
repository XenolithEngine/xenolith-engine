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

#include "XLUiFilesystemModel.h"

#include "SPFilepath.h"
#include "SPString.h"
#include "XLAppWindow.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

// Case-insensitive extension test; a linear walk over the short tables below.
static bool matchExtension(StringView ext, SpanView<StringView> list) {
	for (auto &it : list) {
		if (sp::platform::caseCompare_u(ext, it) == 0) {
			return true;
		}
	}
	return false;
}

static StringView s_imageExt[] = {"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tiff", "ico",
	"avif"};
static StringView s_audioExt[] = {"mp3", "ogg", "wav", "flac", "opus", "m4a", "aac", "mid"};
static StringView s_videoExt[] = {"mp4", "mkv", "avi", "mov", "webm", "m4v", "wmv"};
static StringView s_archiveExt[] = {"zip", "tar", "gz", "bz2", "xz", "7z", "rar", "zst", "tgz",
	"deb", "rpm"};
static StringView s_codeExt[] = {"c", "cc", "cpp", "cxx", "h", "hpp", "hxx", "m", "mm", "cs",
	"java", "js", "ts", "py", "rb", "go", "rs", "sh", "mk", "cmake", "lua", "json", "xml", "yaml",
	"yml", "toml", "ini", "css", "html", "htm", "pug", "sql", "wgsl", "glsl", "wit"};
static StringView s_textExt[] = {"txt", "md", "adoc", "rst", "log", "csv", "doc", "docx", "odt"};

} // namespace

auto FilesystemModel::getFileRef(const Node *node) -> FileRef * {
	// Every node this model builds carries a FileRef; other nodes (the synthetic root, caller
	// nodes) have no object and yield null.
	return node ? static_cast<FileRef *>(node->getObject()) : nullptr;
}

StringView FilesystemModel::getPath(const Node *node) {
	auto ref = getFileRef(node);
	return ref ? StringView(ref->path) : StringView();
}

IconName FilesystemModel::getIcon(const Node *node, bool expanded) {
	auto ref = getFileRef(node);
	if (!ref) {
		return IconName::None;
	}

	if (ref->dir) {
		return expanded ? IconName::File_folder_open_solid : IconName::File_folder_solid;
	}

	if (ref->stat.type == FileType::Link) {
		return IconName::Content_link_outline;
	}

	auto ext = filepath::lastExtension(StringView(ref->name));
	if (ext.empty()) {
		return IconName::Editor_insert_drive_file_outline;
	}

	if (matchExtension(ext, makeSpanView(s_imageExt))) {
		return IconName::Image_image_outline;
	}
	if (matchExtension(ext, makeSpanView(s_audioExt))) {
		return IconName::Image_audiotrack_outline;
	}
	if (matchExtension(ext, makeSpanView(s_videoExt))) {
		return IconName::Av_movie_outline;
	}
	if (matchExtension(ext, makeSpanView(s_archiveExt))) {
		return IconName::Content_archive_outline;
	}
	if (matchExtension(ext, makeSpanView(s_codeExt))) {
		return IconName::Action_code_outline;
	}
	if (matchExtension(ext, makeSpanView(s_textExt))) {
		return IconName::Action_description_outline;
	}
	if (sp::platform::caseCompare_u(ext, "pdf") == 0) {
		return IconName::Image_picture_as_pdf_outline;
	}

	return IconName::Editor_insert_drive_file_outline;
}

FilesystemModel::~FilesystemModel() { }

bool FilesystemModel::init() {
	if (!data::Model::init()) {
		return false;
	}

	// The root is a synthetic container with no FileRef: never listed, not a move target.
	return initFilesystem();
}

bool FilesystemModel::init(const FileInfo &root, StringView title) {
	auto ref = Rc<FileRef>::alloc();
	ref->path = root.path.str<Interface>();
	ref->name = title.empty() ? filepath::lastComponent(root.path).str<Interface>()
							  : title.str<Interface>();
	if (ref->name.empty()) {
		ref->name = ref->path; // "/" and friends have no last component
	}
	ref->dir = true;
	filesystem::stat(root, ref->stat);

	if (!data::Model::init(makeValue(*ref))) {
		return false;
	}

	auto node = getRoot();
	setNodeObject(node, ref);

	/* The model is captured raw (it owns the node that owns this callback). The path is read from
	the node's FileRef on each listing, so a move only has to update the FileRef. */
	node->setChildsCallback([this](Node *self, const Function<void()> &complete) {
		requestListing(self, Function<void()>(complete));
	});

	return initFilesystem();
}

bool FilesystemModel::initFilesystem() {
	setSlots(Slots{
		// Pure predicates, asked speculatively during a drag: no syscalls. Name collisions are
		// checked in performMove.
		.canMove =
				[this](const Node *node, const Node *dstParent, size_t) {
		if (!_moveEnabled) {
			return false;
		}
		auto src = getFileRef(node);
		auto dst = getFileRef(dstParent);
		return src != nullptr && dst != nullptr && dst->dir;
	},
		.canRemove =
				[this](const Node *node) {
		return _removeMode != RemoveMode::Deny && getFileRef(node) != nullptr;
	},
		.performMove =
				[this](Node *node, Node *dstParent, size_t,
						CompletionCallback &&done) { //
		performMove(node, dstParent, sp::move(done));
	},
		.performRemove =
				[this](Node *node, CompletionCallback &&done) { //
		performRemove(node, sp::move(done));
	},
	});
	return true;
}

auto FilesystemModel::addRoot(const FileInfo &info, StringView title) -> Node * {
	auto ref = Rc<FileRef>::alloc();
	ref->path = info.path.str<Interface>();
	ref->name = title.empty() ? filepath::lastComponent(info.path).str<Interface>()
							  : title.str<Interface>();
	if (ref->name.empty()) {
		ref->name = ref->path;
	}
	ref->dir = true;
	filesystem::stat(info, ref->stat);

	// Appended in call order, not sorted.
	return emplaceEntry(getRoot(), maxOf<size_t>(), sp::move(ref));
}

auto FilesystemModel::emplaceEntry(Node *parent, size_t index, Rc<FileRef> &&ref) -> Node * {
	auto value = makeValue(*ref);
	if (!ref->dir) {
		return emplaceItem(parent, index, sp::move(value), ref);
	}

	auto node = emplaceCategory(parent, index, sp::move(value), ref);
	if (node) {
		node->setChildsCallback([this](Node *self, const Function<void()> &complete) {
			requestListing(self, Function<void()>(complete));
		});
	}
	return node;
}

// --- options --------------------------------------------------------------------------------

void FilesystemModel::setShowHidden(bool value) { _showHidden = value; }
void FilesystemModel::setShowFiles(bool value) { _showFiles = value; }
void FilesystemModel::setStatEnabled(bool value) { _stat = value; }

void FilesystemModel::setFilterCallback(FilterCallback &&cb) { _filterCallback = sp::move(cb); }

void FilesystemModel::setSortField(SortField field, bool ascending) {
	_sortField = field;
	_sortAscending = ascending;
}

void FilesystemModel::setDirectoriesFirst(bool value) { _dirsFirst = value; }

void FilesystemModel::setCompareCallback(CompareCallback &&cb) { _compareCallback = sp::move(cb); }

void FilesystemModel::setAsyncEnabled(bool value) { _async = value; }

void FilesystemModel::setMoveEnabled(bool value) { _moveEnabled = value; }
void FilesystemModel::setRemoveMode(RemoveMode value) { _removeMode = value; }
void FilesystemModel::setOverwriteEnabled(bool value) { _overwrite = value; }
void FilesystemModel::setWindow(AppWindow *window) { _window = window; }

// --- listing --------------------------------------------------------------------------------

void FilesystemModel::readDirectory(StringView path, bool withStat, Vector<Entry> &out) {
	// Depth 1: own entries only. With dirFirst the walk also reports the directory itself; skip it.
	filesystem::ftw(FileInfo{path}, [&](const FileInfo &info, FileType type) -> bool {
		if (info.path == path) {
			return true;
		}

		Entry entry;
		entry.path = info.path.str<Interface>();
		entry.name = filepath::lastComponent(info.path).str<Interface>();

		// An unreadable directory is reported as a File, so it becomes an Item with no expander.
		entry.dir = type == FileType::Dir;

		if (withStat) {
			filesystem::stat(info, entry.stat);
		}

		out.emplace_back(sp::move(entry));
		return true;
	}, 1, true);
}

void FilesystemModel::requestListing(Node *dir, Function<void()> &&complete) {
	auto ref = getFileRef(dir);
	if (!ref || !ref->dir) {
		if (complete) {
			complete();
		}
		return;
	}

	const auto path = ref->path;
	const auto withStat = _stat;

	if (!_async) {
		Vector<Entry> entries;
		readDirectory(path, withStat, entries);
		applyListing(dir, sp::move(entries));
		if (complete) {
			complete();
		}
		return;
	}

	auto looper = sprt::dispatch::Looper::acquire();
	Rc<FilesystemModel> self(this);
	Rc<Node> node(dir);

	// Compared after the hop: a refresh during the walk makes this answer stale.
	const auto generation = dir->getChildsGeneration();

	auto st = looper->performAsync(
			[self, node, looper, path, withStat, generation, complete]() mutable {
		Vector<Entry> entries;
		readDirectory(path, withStat, entries);

		looper->performOnThread(
				[self, node, generation, complete, entries = sp::move(entries)]() mutable {
			if (self->isLive(node) && node->getChildsGeneration() == generation) {
				self->applyListing(node, sp::move(entries));
			}

			// Called either way to leave Loading; Node::requestChilds ignores a retired load.
			if (complete) {
				complete();
			}
		}, self);
	}, self);

	if (!sprt::status::isSuccessful(st)) {
		// No worker pool on this looper: fall back to the synchronous path.
		log::source().debug("FilesystemModel", "no worker pool, listing inline: ", path);

		Vector<Entry> entries;
		readDirectory(path, withStat, entries);
		applyListing(dir, sp::move(entries));
		if (complete) {
			complete();
		}
	}
}

void FilesystemModel::applyListing(Node *dir, Vector<Entry> &&entries) {
	/* A diff by path, not a rebuild: surviving entries keep their node and ItemId, and a node
	already inserted by a move is kept rather than duplicated. */
	Map<StringView, Rc<Node>> existing;
	for (auto &it : dir->getChildren()) {
		if (auto ref = getFileRef(it)) {
			existing.emplace(StringView(ref->path), it);
		}
	}

	Set<const Node *> kept;

	for (auto &entry : entries) {
		auto iit = existing.find(StringView(entry.path));
		if (iit != existing.end()) {
			auto node = iit->second.get();
			auto ref = getFileRef(node);

			// Same path but a different kind (file vs directory) is replaced.
			if (ref->dir == entry.dir) {
				ref->stat = entry.stat;

				// The name is not overwritten from the listing, so a root keeps its title.
				if (isVisible(*ref)) {
					updateNode(node, *ref);
					kept.emplace(node);
				}
				continue;
			}

			applyRemove(node);
			existing.erase(iit);
		}

		auto ref = Rc<FileRef>::alloc();
		ref->path = sp::move(entry.path);
		ref->name = sp::move(entry.name);
		ref->stat = entry.stat;
		ref->dir = entry.dir;

		if (!isVisible(*ref)) {
			continue;
		}

		if (auto node = emplaceEntry(dir, maxOf<size_t>(), sp::move(ref))) {
			kept.emplace(node);
		}
	}

	// Whatever the listing did not account for is gone from the disk, or has been filtered out.
	// Collected first: applyRemove() edits the vector this walks.
	Vector<Rc<Node>> gone;
	for (auto &it : dir->getChildren()) {
		if (kept.find(it.get()) == kept.end()) {
			gone.emplace_back(it);
		}
	}
	for (auto &it : gone) { applyRemove(it); }

	sortDirectory(dir);
}

bool FilesystemModel::isVisible(const FileRef &ref) const {
	if (!_showHidden && ref.isHidden()) {
		return false;
	}
	if (!_showFiles && !ref.dir) {
		return false;
	}
	// Asked last, after the standard rules.
	return _filterCallback ? _filterCallback(ref) : true;
}

bool FilesystemModel::compare(const FileRef &l, const FileRef &r) const {
	// Applied first and not reversed by the sort direction.
	if (_dirsFirst && l.dir != r.dir) {
		return l.dir;
	}

	if (_compareCallback) {
		return _compareCallback(l, r);
	}

	int cmp = 0;
	switch (_sortField) {
	case SortField::None: return false;
	case SortField::Name: break;
	case SortField::Size:
		cmp = l.stat.size < r.stat.size ? -1 : (l.stat.size > r.stat.size ? 1 : 0);
		break;
	case SortField::Time: {
		const auto lt = l.stat.mtime.toMicros();
		const auto rt = r.stat.mtime.toMicros();
		cmp = lt < rt ? -1 : (lt > rt ? 1 : 0);
		break;
	}
	case SortField::Extension:
		cmp = sp::platform::caseCompare_u(filepath::lastExtension(StringView(l.name)),
				filepath::lastExtension(StringView(r.name)));
		break;
	}

	if (cmp == 0) {
		cmp = sp::platform::caseCompare_u(StringView(l.name), StringView(r.name));
	}
	if (cmp == 0) {
		// Ties are not "less", keeping a strict weak ordering.
		return false;
	}
	return _sortAscending ? cmp < 0 : cmp > 0;
}

void FilesystemModel::sortDirectory(Node *dir) {
	if (_sortField == SortField::None && !_compareCallback && !_dirsFirst) {
		return; // keep the walk order and avoid a Structure update
	}

	sortChildren(dir, [this](const Node *l, const Node *r) {
		auto lr = getFileRef(l);
		auto rr = getFileRef(r);

		// Nodes without a FileRef sink to the end.
		if (!lr) {
			return false;
		}
		if (!rr) {
			return true;
		}
		return compare(*lr, *rr);
	});
}

auto FilesystemModel::makeValue(const FileRef &ref) -> Value {
	Value ret;
	ret.setString(ref.name, "name");
	ret.setString(ref.path, "path");
	ret.setBool(ref.dir, "dir");

	// Omitted when no stat was taken (setStatEnabled(false)).
	if (ref.stat.type != FileType::Unknown) {
		ret.setInteger(int64_t(ref.stat.size), "size");
		ret.setInteger(int64_t(ref.stat.mtime.toMicros()), "mtime");
	}
	return ret;
}

void FilesystemModel::updateNode(Node *node, const FileRef &ref) {
	auto value = makeValue(ref);

	// Only when it differs: setNodeData bumps the revision and rebuilds the row.
	if (!(value == node->getData())) {
		setNodeData(node, sp::move(value));
	}
}

void FilesystemModel::rebaseSubtree(Node *node, StringView oldPrefix, StringView newPrefix) {
	for (auto &child : node->getChildren()) {
		auto ref = getFileRef(child);
		if (!ref || !StringView(ref->path).starts_with(oldPrefix)) {
			continue;
		}

		// The remainder starts with the separator, so this is a concatenation rather than a merge.
		ref->path = newPrefix.str<Interface>() + ref->path.substr(oldPrefix.size());
		updateNode(child, *ref);

		if (ref->dir) {
			rebaseSubtree(child, oldPrefix, newPrefix);
		}
	}
}

// --- operations -----------------------------------------------------------------------------

static data::Model::Node *findNodeForPath(data::Model::Node *node, StringView path) {
	if (auto ref = FilesystemModel::getFileRef(node)) {
		if (StringView(ref->path) == path) {
			return node;
		}
		// Descend only into directories whose path is a prefix of the target.
		if (!ref->dir || !path.starts_with(StringView(ref->path))) {
			return nullptr;
		}
	}

	for (auto &it : node->getChildren()) {
		if (auto ret = findNodeForPath(it, path)) {
			return ret;
		}
	}
	return nullptr;
}

auto FilesystemModel::getNodeForPath(StringView path) const -> Node * {
	return findNodeForPath(getRoot(), path);
}

bool FilesystemModel::renameNode(Node *node, StringView newName) {
	auto ref = getFileRef(node);
	if (!ref || newName.empty() || newName == StringView(ref->name)) {
		return false;
	}

	const auto target = filepath::merge<Interface>(filepath::root(StringView(ref->path)), newName);
	if (target == ref->path) {
		return false;
	}

	if (!_overwrite && filesystem::exists(FileInfo{target})) {
		log::source().warn("FilesystemModel", "rename refused, target exists: ", target);
		return false;
	}

	if (!filesystem::move(FileInfo{ref->path}, FileInfo{target})) {
		log::source().error("FilesystemModel", "rename failed: ", ref->path, " -> ", target);
		return false;
	}

	const auto oldPath = ref->path;
	ref->path = target;
	ref->name = newName.str<Interface>();
	filesystem::stat(FileInfo{target}, ref->stat);
	updateNode(node, *ref);

	// The node keeps its id, so the row stays selected and open.
	if (ref->dir) {
		rebaseSubtree(node, oldPath, target);
	}
	if (auto parent = node->getParent()) {
		sortDirectory(parent);
	}
	return true;
}

auto FilesystemModel::createDirectory(Node *parent, StringView name) -> Node * {
	if (!parent) {
		parent = getRoot();
	}

	auto parentRef = getFileRef(parent);
	if (!parentRef || !parentRef->dir || name.empty()) {
		return nullptr;
	}

	const auto target = filepath::merge<Interface>(parentRef->path, name);
	if (filesystem::exists(FileInfo{target})) {
		return nullptr;
	}
	if (!filesystem::mkdir(FileInfo{target})) {
		log::source().error("FilesystemModel", "mkdir failed: ", target);
		return nullptr;
	}

	auto ref = Rc<FileRef>::alloc();
	ref->path = target;
	ref->name = name.str<Interface>();
	ref->dir = true;
	filesystem::stat(FileInfo{target}, ref->stat);

	// Safe for an unlisted parent: its later listing keeps this node by path.
	auto node = emplaceEntry(parent, maxOf<size_t>(), sp::move(ref));
	sortDirectory(parent);
	return node;
}

bool FilesystemModel::revealNode(Node *node) {
	auto ref = getFileRef(node);
	if (!ref) {
		return false;
	}
	return sprt::status::isSuccessful(
			openShellDialog(DialogType::RevealInFileManager, ref->path, nullptr));
}

void FilesystemModel::refresh(Node *dir) {
	if (!dir) {
		dir = getRoot();
	}

	// Only listed directories are refreshed.
	if (!dir->isCategory() || dir->getChildsState() != ChildsState::Loaded) {
		return;
	}

	requestListing(dir, nullptr);
}

void FilesystemModel::refreshAll() {
	// Collected first, since a refresh edits child vectors; top-down, and isLive skips removed ones.
	Vector<Rc<Node>> dirs;

	Function<void(Node *)> collect = [&](Node *node) {
		if (!node->isCategory()) {
			return;
		}

		// Continue through categories without a FileRef, such as the synthetic addRoot() root.
		if (getFileRef(node) && node->getChildsState() == ChildsState::Loaded) {
			dirs.emplace_back(node);
		}
		for (auto &it : node->getChildren()) { collect(it); }
	};

	collect(getRoot());

	for (auto &it : dirs) {
		if (isLive(it)) {
			refresh(it);
		}
	}
}

// --- slots ----------------------------------------------------------------------------------

void FilesystemModel::performMove(Node *node, Node *dstParent, CompletionCallback &&done) {
	auto src = getFileRef(node);
	auto dst = getFileRef(dstParent);
	if (!src || !dst || !dst->dir) {
		done(Status::ErrorInvalidArguemnt);
		return;
	}

	const auto name = filepath::lastComponent(StringView(src->path)).str<Interface>();
	const auto target = filepath::merge<Interface>(dst->path, name);

	if (target == src->path) {
		done(Status::Ok); // already where it was asked to go
		return;
	}

	// Refused: filesystem::move renames over an existing file.
	if (!_overwrite && filesystem::exists(FileInfo{target})) {
		log::source().warn("FilesystemModel", "move refused, target exists: ", target);
		done(Status::ErrorFileExists);
		return;
	}

	if (!filesystem::move(FileInfo{src->path}, FileInfo{target})) {
		log::source().error("FilesystemModel", "move failed: ", src->path, " -> ", target);
		// A failure status reverts the model's optimistic move.
		done(Status::ErrorNotPermitted);
		return;
	}

	const auto oldPath = src->path;
	src->path = target;

	// A node showing its own last component keeps doing so; a root showing a title given to it
	// keeps the title.
	if (StringView(src->name) == filepath::lastComponent(StringView(oldPath))) {
		src->name = name;
	}
	updateNode(node, *src);

	// Rewrite descendants' paths to keep the open subtree; the destination needs no re-walk.
	if (src->dir) {
		rebaseSubtree(node, oldPath, target);
	}

	// Completed before the sort: under MutationPolicy::Confirmed the node joins `dstParent` only
	// when the completion runs.
	done(Status::Ok);
	sortDirectory(dstParent);

	log::source().info("FilesystemModel", "moved ", name, " -> ", dst->path);
}

void FilesystemModel::performRemove(Node *node, CompletionCallback &&done) {
	auto ref = getFileRef(node);
	if (!ref) {
		done(Status::ErrorInvalidArguemnt);
		return;
	}

	switch (_removeMode) {
	case RemoveMode::Deny: done(Status::ErrorNotPermitted); break;
	case RemoveMode::Trash: {
		auto st = openShellDialog(DialogType::MoveToTrash, ref->path, sp::move(done));
		if (!sprt::status::isSuccessful(st)) {
			// openShellDialog has already answered through the completion.
			log::source().debug("FilesystemModel",
					"trash refused: ", sprt::status::getStatusName(st));
		}
		break;
	}
	case RemoveMode::Delete:
		if (!filesystem::remove(FileInfo{ref->path}, ref->dir)) {
			log::source().error("FilesystemModel", "remove failed: ", ref->path);
			done(Status::ErrorNotPermitted);
			break;
		}
		done(Status::Ok);
		break;
	}
}

Status FilesystemModel::openShellDialog(DialogType type, StringView path,
		CompletionCallback &&done) {
	if (!_window) {
		// Shell actions need a parent window.
		if (done) {
			done(Status::ErrorInvalidArguemnt);
		}
		return Status::ErrorInvalidArguemnt;
	}

	auto request = Rc<sprt::window::DialogRequest>::create();
	request->type = type;
	request->paths.emplace_back(path.data(), path.size());

	// The model is held until the answer arrives; the cycle with _dialogs is broken by the erase
	// in the callback, which runs exactly once.
	Rc<FilesystemModel> self(this);
	auto req = request.get();

	request->callback = [self, req, done = sp::move(done)](
								const sprt::window::DialogResult &res) mutable {
		for (auto it = self->_dialogs.begin(); it != self->_dialogs.end(); ++it) {
			if (it->get() == req) {
				self->_dialogs.erase(it);
				break;
			}
		}

		// Passed through: Declined reverts the optimistic removal.
		if (done) {
			done(res.status);
		}
	};

	// The request is the cancellation token, kept until its completion has run.
	_dialogs.emplace_back(request);

	// On failure openDialog still calls the callback, which removes the request from _dialogs.
	return _window->openDialog(request);
}

} // namespace stappler::xenolith::ui
