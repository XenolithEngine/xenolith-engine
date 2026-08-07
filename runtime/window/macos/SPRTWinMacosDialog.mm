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

#define __SPRT_BUILD 1

// AppKit only exists on macOS; the window module is built for every Apple target, so the whole
// unit is guarded. __sprt_def.h comes first so SPRT_MACOS is defined before the guard.
#include <sprt/c/bits/__sprt_def.h>

#if SPRT_MACOS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wavailability"

#import <AppKit/NSApplication.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSColorPanel.h>
#import <AppKit/NSColorSpace.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSFontManager.h>
#import <AppKit/NSFontPanel.h>
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSSavePanel.h>
#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "SPRTWinMacosDialog.h"
#include "SPRTWinMacosWindow.h"

#include <sprt/runtime/window/native_window.h>

namespace sprt::window {

namespace {

NSString *toNSString(StringView str) {
	return [[NSString alloc] initWithBytes:str.data() length:str.size()
			encoding:NSUTF8StringEncoding];
}

String fromNSString(NSString *str) {
	if (!str) {
		return String();
	}
	auto utf8 = [str UTF8String];
	return utf8 ? String(utf8) : String();
}

// AppKit hands paths back as file URLs; the API contract is native paths.
String pathFromURL(NSURL *url) {
	if (!url || ![url isFileURL]) {
		return String();
	}
	return fromNSString([url path]);
}

// NSColor lives in whatever space the picker was in; convert before reading components, or the
// getter throws.
Color4F fromNSColor(NSColor *color) {
	if (!color) {
		return Color4F::WHITE;
	}
	NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	if (!rgb) {
		return Color4F::WHITE;
	}
	return Color4F(float([rgb redComponent]), float([rgb greenComponent]),
			float([rgb blueComponent]), float([rgb alphaComponent]));
}

// Build the file-type list. macOS wants UTIs; a glob has to be reduced to its extension, which is
// all NSSavePanel's allowedContentTypes can express.
NSArray<UTType *> *contentTypesFor(SpanView<FileFilter> filters) {
	NSMutableArray<UTType *> *types = [NSMutableArray array];
	for (auto &filter : filters) {
		for (auto &mime : filter.mimeTypes) {
			if (auto *type = [UTType typeWithMIMEType:toNSString(mime)]) {
				[types addObject:type];
			}
		}
		for (auto &pattern : filter.patterns) {
			StringView ext(pattern);
			auto dot = ext.rfind('.');
			if (dot == Max<size_t>) {
				continue;
			}
			ext = StringView(ext.data() + dot + 1, ext.size() - dot - 1);
			// "*.*" and friends carry no type information, so they simply widen the panel.
			if (ext.empty() || ext == "*") {
				return nil;
			}
			if (auto *type = [UTType typeWithFilenameExtension:toNSString(ext)]) {
				[types addObject:type];
			}
		}
	}
	return [types count] > 0 ? types : nil;
}

} // namespace

WindowCapabilities getMacosDialogCapabilities() {
	// NativeDialogParenting is honest here in a way it is not everywhere: a sheet really is owned
	// by its window, so AppKit blocks the parent and raises the sheet on a click with no help.
	return WindowCapabilities::FileDialogs | WindowCapabilities::ColorDialog
			| WindowCapabilities::FontDialog | WindowCapabilities::SystemFileActions
			| WindowCapabilities::NativeDialogParenting;
}

MacosDialogHandle::~MacosDialogHandle() {
	if (_observer) {
		// __bridge_transfer hands the retain we took back to ARC, which then releases it.
		id observer = (__bridge_transfer id)_observer;
		_observer = nullptr;
		[[NSNotificationCenter defaultCenter] removeObserver:observer];
	}
	if (_panel) {
		id panel = (__bridge_transfer id)_panel;
		_panel = nullptr;
		(void)panel;
	}
}

bool MacosDialogHandle::init(NotNull<ContextController> controller,
		NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req, NativeWindow *parent) {
	if (!DialogHandle::init(controller, target, sprt::move(req), parent)) {
		return false;
	}

	if (parent) {
		// Every NativeWindow on this platform is a MacosWindow.
		_parentWindow = (__bridge void *)static_cast<MacosWindow *>(parent)->getWindow();
	}

	switch (_request->type) {
	case DialogType::OpenFile:
	case DialogType::OpenDirectory:
	case DialogType::SaveFile: return openFilePanel();
	case DialogType::Color: return openColorPanel();
	case DialogType::Font: return openFontPanel();
	case DialogType::RevealInFileManager:
		if (_request->paths.empty()) {
			return false;
		}
		// Answers on the spot. finalize() posts to the target looper, so the completion still
		// arrives asynchronously, exactly as for a panel — the caller cannot tell them apart.
		finalize(runReveal());
		return true;
	case DialogType::MoveToTrash:
		if (_request->paths.empty()) {
			return false;
		}
		finalize(runTrash());
		return true;
	}
	return false;
}

bool MacosDialogHandle::openFilePanel() {
	const auto &req = *_request;
	const bool save = req.type == DialogType::SaveFile;

	// NSOpenPanel derives from NSSavePanel, so everything shared is set through the base.
	NSSavePanel *panel = save ? [NSSavePanel savePanel] : [NSOpenPanel openPanel];
	if (!panel) {
		return false;
	}

	if (!save) {
		auto *open = (NSOpenPanel *)panel;
		const bool directory = req.type == DialogType::OpenDirectory;
		[open setCanChooseFiles:directory ? NO : YES];
		[open setCanChooseDirectories:directory ? YES : NO];
		[open setAllowsMultipleSelection:hasFlag(req.flags, DialogFlags::Multiple) ? YES : NO];
	} else {
		[panel setCanCreateDirectories:YES];
	}

	if (!req.title.empty()) {
		// `title` is the sheet's own caption; `message` is the line above the browser, which is
		// where a sheet actually shows text — set both so it reads right either way.
		[panel setTitle:toNSString(req.title)];
		[panel setMessage:toNSString(req.title)];
	}
	if (!req.acceptLabel.empty()) {
		[panel setPrompt:toNSString(req.acceptLabel)];
	}
	if (!req.filename.empty()) {
		[panel setNameFieldStringValue:toNSString(req.filename)];
	}
	if (!req.path.empty()) {
		[panel setDirectoryURL:[NSURL fileURLWithPath:toNSString(req.path) isDirectory:YES]];
	}
	[panel setShowsHiddenFiles:hasFlag(req.flags, DialogFlags::ShowHidden) ? YES : NO];

	if (req.type != DialogType::OpenDirectory) {
		if (auto *types = contentTypesFor(req.filters)) {
			[panel setAllowedContentTypes:types];
			[panel setAllowsOtherFileTypes:YES];
		}
	}

	_panel = (__bridge_retained void *)panel;

	// The completion handler runs on the main run loop, which is the context looper — so it can
	// finalize directly. `guard` keeps the handle alive until then even if the registry drops it.
	auto handler = ^(NSModalResponse response) {
		Rc<MacosDialogHandle> guard(this);
		if (!isActive()) {
			return;
		}

		DialogResult result;
		if (response == NSModalResponseOK) {
			if (!save && hasFlag(_request->flags, DialogFlags::Multiple)) {
				for (NSURL *url in [(NSOpenPanel *)panel URLs]) {
					auto path = pathFromURL(url);
					if (!path.empty()) {
						result.paths.emplace_back(sprt::move(path));
					}
				}
			} else {
				auto path = pathFromURL([panel URL]);
				if (!path.empty()) {
					result.paths.emplace_back(sprt::move(path));
				}
			}
			result.status = result.paths.empty() ? Status::Declined : Status::Ok;
		} else {
			// NSModalResponseCancel covers both the user dismissing it and our own cancel(); the
			// latter has already answered, so finalize() drops this.
			result.status = Status::Declined;
		}
		finalize(sprt::move(result));
	};

	if (_parentWindow && hasFlag(req.flags, DialogFlags::Modal)) {
		// A real sheet: AppKit owns the parent relationship, so it blocks and raises for us.
		[panel beginSheetModalForWindow:(__bridge NSWindow *)_parentWindow completionHandler:handler];
	} else {
		// No parent (or not modal): a free-standing panel, which does not block anything.
		[panel beginWithCompletionHandler:handler];
	}
	return true;
}

bool MacosDialogHandle::openColorPanel() {
	// NSColorPanel is a process-wide singleton with no completion handler: it reports through the
	// target/action of whatever set it, and closing is a window notification. So the answer is
	// assembled from both — the colour at the moment the panel closes.
	NSColorPanel *panel = [NSColorPanel sharedColorPanel];
	if (!panel) {
		return false;
	}

	[panel setShowsAlpha:hasFlag(_request->flags, DialogFlags::AlphaChannel) ? YES : NO];

	auto &c = _request->color;
	[panel setColor:[NSColor colorWithSRGBRed:c.r green:c.g blue:c.b alpha:c.a]];
	if (!_request->title.empty()) {
		[panel setTitle:toNSString(_request->title)];
	}

	_panel = (__bridge_retained void *)panel;

	auto *observer = [[NSNotificationCenter defaultCenter]
			addObserverForName:NSWindowWillCloseNotification object:panel queue:nil
			usingBlock:^(NSNotification *) {
		Rc<MacosDialogHandle> guard(this);
		if (!isActive()) {
			return;
		}
		DialogResult result;
		result.status = Status::Ok;
		result.color = fromNSColor([panel color]);
		finalize(sprt::move(result));
	}];
	_observer = (__bridge_retained void *)observer;

	[panel makeKeyAndOrderFront:nil];
	return true;
}

bool MacosDialogHandle::openFontPanel() {
	// Same shape as the colour panel: a shared panel that talks through the font manager, and a
	// close notification that tells us the user is done.
	NSFontManager *manager = [NSFontManager sharedFontManager];
	NSFontPanel *panel = [manager fontPanel:YES];
	if (!panel) {
		return false;
	}

	auto &f = _request->font;
	NSFont *initial = nil;
	if (!f.family.empty()) {
		initial = [NSFont fontWithName:toNSString(f.family) size:(f.size > 0.0f ? f.size : 12.0)];
	}
	if (!initial) {
		initial = [NSFont systemFontOfSize:(f.size > 0.0f ? f.size : 12.0)];
	}
	[manager setSelectedFont:initial isMultiple:NO];

	_panel = (__bridge_retained void *)panel;

	auto *observer = [[NSNotificationCenter defaultCenter]
			addObserverForName:NSWindowWillCloseNotification object:panel queue:nil
			usingBlock:^(NSNotification *) {
		Rc<MacosDialogHandle> guard(this);
		if (!isActive()) {
			return;
		}

		NSFont *chosen = [manager convertFont:[manager selectedFont]];
		DialogResult result;
		if (!chosen) {
			finalize(Status::Declined);
			return;
		}

		result.status = Status::Ok;
		result.font.family = fromNSString([chosen familyName]);
		result.font.size = float([chosen pointSize]);

		auto traits = [manager traitsOfFont:chosen];
		result.font.bold = (traits & NSBoldFontMask) != 0;
		result.font.italic = (traits & NSItalicFontMask) != 0;

		// The PostScript name is the descriptor that round-trips back into NSFont fontWithName:.
		result.font.description = fromNSString([chosen fontName]);
		finalize(sprt::move(result));
	}];
	_observer = (__bridge_retained void *)observer;

	[panel makeKeyAndOrderFront:nil];
	return true;
}

DialogResult MacosDialogHandle::runReveal() {
	DialogResult result;

	NSMutableArray<NSURL *> *urls = [NSMutableArray array];
	for (auto &path : _request->paths) {
		if (auto *url = [NSURL fileURLWithPath:toNSString(path)]) {
			[urls addObject:url];
		}
	}
	if ([urls count] == 0) {
		result.status = Status::ErrorInvalidArguemnt;
		return result;
	}

	// Selects the items inside their folder — "reveal", as opposed to opening the folder itself.
	[[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:urls];
	result.status = Status::Ok;
	return result;
}

DialogResult MacosDialogHandle::runTrash() {
	DialogResult result;
	result.status = Status::Ok;

	auto *manager = [NSFileManager defaultManager];
	for (auto &path : _request->paths) {
		auto *url = [NSURL fileURLWithPath:toNSString(path)];
		NSError *error = nil;
		if (!url || ![manager trashItemAtURL:url resultingItemURL:nullptr error:&error]) {
			// Report the whole request as failed, matching the other backends: partial success is
			// not something the caller can act on.
			result.status = Status::ErrorUnknown;
		}
	}
	return result;
}

Status MacosDialogHandle::cancel(Status st) {
	if (!isActive()) {
		return Status::ErrorAlreadyPerformed;
	}

	// Take the panel down first. Its completion handler / close notification still fires, but
	// finalize() below has already answered by then, so the late result is dropped.
	if (_panel) {
		switch (_request->type) {
		case DialogType::OpenFile:
		case DialogType::OpenDirectory:
		case DialogType::SaveFile:
			// Ends the sheet and invokes the completion handler with NSModalResponseCancel.
			[(__bridge NSSavePanel *)_panel cancel:nil];
			break;
		default:
			// The shared colour and font panels are ordinary windows; closing one fires the
			// notification we are listening for.
			[(__bridge NSWindow *)_panel close];
			break;
		}
	}

	if (_observer) {
		// __bridge_transfer hands the retain we took back to ARC, which then releases it.
		id observer = (__bridge_transfer id)_observer;
		_observer = nullptr;
		[[NSNotificationCenter defaultCenter] removeObserver:observer];
	}

	return DialogHandle::cancel(st);
}

void MacosDialogHandle::raise() {
	if (_panel) {
		[(__bridge NSWindow *)_panel makeKeyAndOrderFront:nil];
	}
}

} // namespace sprt::window

#pragma clang diagnostic pop

#endif // SPRT_MACOS
