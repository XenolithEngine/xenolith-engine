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

#include "XLCommon.h" // IWYU pragma: keep

#include "fileexplorer/FilePlaces.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

struct PlaceInfo {
	FileCategory category;
	StringView title; // a locale tag: the label follows the language, the contents never do
};

static constexpr PlaceInfo s_places[] = {
	{FileCategory::UserHome, StringView("@Locale:FE:Places:Home")},
	{FileCategory::UserDesktop, StringView("@Locale:FE:Places:Desktop")},
	{FileCategory::UserDocuments, StringView("@Locale:FE:Places:Documents")},
	{FileCategory::UserDownload, StringView("@Locale:FE:Places:Downloads")},
	{FileCategory::UserMusic, StringView("@Locale:FE:Places:Music")},
	{FileCategory::UserPictures, StringView("@Locale:FE:Places:Pictures")},
	{FileCategory::UserVideos, StringView("@Locale:FE:Places:Videos")},
};

} // namespace

Rc<ui::FilesystemModel> makePlacesModel() {
	auto model = Rc<ui::FilesystemModel>::create();
	if (!model) {
		return nullptr;
	}

	model->setShowFiles(false);
	model->setStatEnabled(false);
	model->setDirectoriesFirst(true);
	model->setAsyncEnabled(true);

	// Nothing in this example writes to disk. Saying so on the model is what keeps the tree's
	// drag & drop and its Trash slot out of a real home directory.
	model->setMoveEnabled(false);
	model->setRemoveMode(ui::FilesystemModel::RemoveMode::Deny);

	for (auto &place : s_places) {
		auto path = filesystem::findPath<Interface>(place.category);
		if (path.empty() || !filesystem::exists(FileInfo{path})) {
			continue;
		}
		model->addRoot(FileInfo{path}, place.title);
	}

	/* The root of the filesystem. Paths are POSIX everywhere in the runtime - a Windows drive is
	reached as /c - so "/" is the root on every target. What no target has is an enumeration of
	volumes: there is no API here for the drives or the mount points below it, so they appear only
	as the children this listing finds. */
	model->addRoot(FileInfo{StringView("/")}, StringView("@Locale:FE:Places:Filesystem"));

	return model;
}

} // namespace stappler::xenolith::examples
