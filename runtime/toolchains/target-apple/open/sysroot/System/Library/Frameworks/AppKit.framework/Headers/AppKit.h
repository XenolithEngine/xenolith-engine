/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <AppKit/AppKit.h> for the Xcode-SDK-free macOS target
(*-apple-macosx+open). AppKit is a closed Objective-C framework; this reconstructs
ONLY the window/view/event/pasteboard/cursor surface the runtime's macOS window
backend (runtime/window/macos) references. Every class/selector resolves at run
time from the real AppKit.dylib (baked AppKit.tbd). Enum values mirror the
documented AppKit ABI.
**/

#ifndef __SPRT_OPEN_APPKIT_H_
#define __SPRT_OPEN_APPKIT_H_
#include <__sprt_abi/AppKit/AppKit.h>

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <IOKit/hidsystem/IOLLEvent.h> /* NX_DEVICE*KEYMASK (as the real NSEvent.h pulls) */

// Window levels, in CGWindowLevel units (CGWindowLevel.h), as the SDK derives them.
static const NSWindowLevel NSNormalWindowLevel = 0;
static const NSWindowLevel NSFloatingWindowLevel = 3;
static const NSWindowLevel NSTornOffMenuWindowLevel = 3;
static const NSWindowLevel NSSubmenuWindowLevel = 3;
static const NSWindowLevel NSModalPanelWindowLevel = 8;
static const NSWindowLevel NSMainMenuWindowLevel = 24;
static const NSWindowLevel NSStatusWindowLevel = 25;
static const NSWindowLevel NSPopUpMenuWindowLevel = 101;
static const NSWindowLevel NSScreenSaverWindowLevel = 1000;

/* ---- forward decls ------------------------------------------------------- */
@class NSWindow, NSView, NSScreen, NSColor, NSEvent, NSCursor, NSTrackingArea;
@class NSApplication, NSPasteboard, NSPasteboardItem, NSAppearance, NSTextInputContext;
@class NSMenu, NSGraphicsContext;
@protocol NSTextInputClient;

/* ---- drag and drop (NSDragging.h) ----------------------------------------- */
/* The destination half only: an NSView receives OS drops. The source side
   (NSDraggingSource / NSDraggingSession / NSDraggingItem) is not used. */
@protocol NSDraggingInfo <NSObject>
@required
@property(nullable, readonly) NSWindow *draggingDestinationWindow;
@property(readonly) NSDragOperation draggingSourceOperationMask;
@property(readonly) NSPoint draggingLocation;
@property(readonly) NSPoint draggedImageLocation;
@property(readonly) NSPasteboard *draggingPasteboard;
@property(nullable, readonly) id draggingSource;
@property(readonly) NSInteger draggingSequenceNumber;
@property NSInteger numberOfValidItemsForDrop;
@end

@protocol NSDraggingDestination <NSObject>
@optional
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender;
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender;
- (void)draggingExited:(nullable id<NSDraggingInfo>)sender;
- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender;
- (void)concludeDragOperation:(nullable id<NSDraggingInfo>)sender;
- (void)draggingEnded:(id<NSDraggingInfo>)sender;
- (BOOL)wantsPeriodicDraggingUpdates;
@end

/* ---- NSResponder + event chain ------------------------------------------- */
@interface NSResponder : NSObject
- (BOOL)acceptsFirstResponder;
- (BOOL)becomeFirstResponder;
- (BOOL)resignFirstResponder;
- (void)mouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;
- (void)mouseMoved:(NSEvent *)event;
- (void)mouseEntered:(NSEvent *)event;
- (void)mouseExited:(NSEvent *)event;
- (void)rightMouseDown:(NSEvent *)event;
- (void)rightMouseUp:(NSEvent *)event;
- (void)rightMouseDragged:(NSEvent *)event;
- (void)otherMouseDown:(NSEvent *)event;
- (void)otherMouseUp:(NSEvent *)event;
- (void)otherMouseDragged:(NSEvent *)event;
- (void)scrollWheel:(NSEvent *)event;
- (void)keyDown:(NSEvent *)event;
- (void)keyUp:(NSEvent *)event;
- (void)flagsChanged:(NSEvent *)event;
- (void)magnifyWithEvent:(NSEvent *)event;
- (void)interpretKeyEvents:(NSArray<NSEvent *> *)eventArray;
- (void)doCommandBySelector:(SEL)selector;
- (void)noResponderFor:(SEL)eventSelector;
- (void)flushBufferedKeyEvents;
@property(nullable, readonly) NSTextInputContext *inputContext;

/* Standard editing actions. interpretKeyEvents: turns a keystroke into one of these and sends it
   through doCommandBySelector:, so a text client overrides the ones it implements and lets the
   rest fall through. */
- (void)insertText:(id)insertString;
- (void)insertNewline:(nullable id)sender;
- (void)insertLineBreak:(nullable id)sender;
- (void)insertParagraphSeparator:(nullable id)sender;
- (void)insertTab:(nullable id)sender;
- (void)insertBacktab:(nullable id)sender;
- (void)deleteBackward:(nullable id)sender;
- (void)deleteForward:(nullable id)sender;
- (void)deleteWordBackward:(nullable id)sender;
- (void)deleteWordForward:(nullable id)sender;
- (void)deleteToBeginningOfLine:(nullable id)sender;
- (void)deleteToEndOfLine:(nullable id)sender;
- (void)moveLeft:(nullable id)sender;
- (void)moveRight:(nullable id)sender;
- (void)moveUp:(nullable id)sender;
- (void)moveDown:(nullable id)sender;
- (void)moveLeftAndModifySelection:(nullable id)sender;
- (void)moveRightAndModifySelection:(nullable id)sender;
- (void)moveUpAndModifySelection:(nullable id)sender;
- (void)moveDownAndModifySelection:(nullable id)sender;
- (void)moveWordLeft:(nullable id)sender;
- (void)moveWordRight:(nullable id)sender;
- (void)moveToBeginningOfLine:(nullable id)sender;
- (void)moveToEndOfLine:(nullable id)sender;
- (void)moveToBeginningOfDocument:(nullable id)sender;
- (void)moveToEndOfDocument:(nullable id)sender;
- (void)selectAll:(nullable id)sender;
- (void)selectLine:(nullable id)sender;
- (void)selectWord:(nullable id)sender;
- (void)cancelOperation:(nullable id)sender;
@end

/* ---- NSView -------------------------------------------------------------- */
@interface NSView : NSResponder
- (instancetype)initWithFrame:(NSRect)frameRect;
@property NSRect frame;
@property NSRect bounds;
@property(readonly) NSRect visibleRect;
@property(nullable, strong) CALayer *layer;
@property BOOL wantsLayer;
- (nullable CALayer *)makeBackingLayer;
@property(nullable, readonly) NSWindow *window;
@property(nullable, readonly) NSView *superview;
- (void)addSubview:(NSView *)view;
- (void)setNeedsDisplay:(BOOL)flag;
@property BOOL needsDisplay;
- (void)addTrackingArea:(NSTrackingArea *)trackingArea;
- (void)removeTrackingArea:(NSTrackingArea *)trackingArea;
@property(readonly, copy) NSArray<NSTrackingArea *> *trackingAreas;
- (void)updateTrackingAreas;
- (NSSize)convertSizeToBacking:(NSSize)size;
- (NSSize)convertSizeFromBacking:(NSSize)size;
- (NSPoint)convertPointToBacking:(NSPoint)point;
- (NSPoint)convertPointFromBacking:(NSPoint)point;
- (NSRect)convertRectToBacking:(NSRect)rect;
- (NSRect)convertRectFromBacking:(NSRect)rect;
- (NSPoint)convertPoint:(NSPoint)point fromView:(nullable NSView *)view;
- (NSPoint)convertPoint:(NSPoint)point toView:(nullable NSView *)view;
- (NSRect)convertRect:(NSRect)rect fromView:(nullable NSView *)view;
- (NSRect)convertRect:(NSRect)rect toView:(nullable NSView *)view;
- (NSSize)convertSize:(NSSize)size fromView:(nullable NSView *)view;
- (NSSize)convertSize:(NSSize)size toView:(nullable NSView *)view;
@property NSAutoresizingMaskOptions autoresizingMask;
@property NSViewLayerContentsRedrawPolicy layerContentsRedrawPolicy;
@property NSViewLayerContentsPlacement layerContentsPlacement;
@property(getter=inLiveResize, readonly) BOOL inLiveResize;
- (void)display;
@property(readonly, copy) NSArray<NSPasteboardType> *registeredDraggedTypes;
- (void)registerForDraggedTypes:(NSArray<NSPasteboardType> *)newTypes;
- (void)unregisterDraggedTypes;
@end

/* ---- NSTextInputContext --------------------------------------------------- */
@interface NSTextInputContext : NSObject
@property(class, nullable, readonly) NSTextInputContext *currentInputContext;
- (void)activate;
- (void)deactivate;
- (void)discardMarkedText;
- (void)invalidateCharacterCoordinates;
- (BOOL)handleEvent:(NSEvent *)event;
@property(readonly, weak) id<NSTextInputClient> client;
@property(nullable, copy) NSArray<NSString *> *allowedInputSourceLocales;
@property(readonly) NSArray<NSString *> *keyboardInputSources;
@property(nullable, copy) NSString *selectedKeyboardInputSource;
@end

/* ---- NSViewController ---------------------------------------------------- */
@interface NSViewController : NSResponder
@property(null_resettable, strong) NSView *view;
- (void)viewDidLoad;
- (void)viewWillAppear;
- (void)viewDidAppear;
- (void)viewWillDisappear;
- (void)viewDidDisappear;
@end

/* ---- NSWindow ------------------------------------------------------------ */
@interface NSWindow : NSResponder
- (instancetype)initWithContentRect:(NSRect)contentRect
						  styleMask:(NSWindowStyleMask)style
							backing:(NSBackingStoreType)backingStoreType
							  defer:(BOOL)flag;
@property(nullable, strong) NSView *contentView;
@property(nullable, strong) NSViewController *contentViewController;
@property(copy) NSString *title;
@property NSWindowStyleMask styleMask;
@property NSRect frame;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag animate:(BOOL)animateFlag;
- (void)setFrameOrigin:(NSPoint)point;
- (void)setContentSize:(NSSize)size;
@property NSSize minSize;
@property NSSize maxSize;
@property NSSize contentMinSize;
@property NSSize contentMaxSize;
@property(nullable, readonly) NSScreen *screen;
@property(readonly) CGFloat backingScaleFactor;
- (NSRect)contentRectForFrameRect:(NSRect)frameRect;
- (NSRect)frameRectForContentRect:(NSRect)contentRect;
@property(readonly) NSRect contentLayoutRect;
@property(weak, nullable) id delegate;
@property(getter=isVisible, readonly) BOOL visible;
@property(getter=isKeyWindow, readonly) BOOL keyWindow;
@property(getter=isMainWindow, readonly) BOOL mainWindow;
@property(getter=isZoomed, readonly) BOOL zoomed;
@property(getter=isOpaque) BOOL opaque;
- (void)makeKeyAndOrderFront:(nullable id)sender;
- (void)orderFront:(nullable id)sender;
- (void)orderOut:(nullable id)sender;
- (void)close;
- (void)performClose:(nullable id)sender;
- (void)miniaturize:(nullable id)sender;
- (void)deminiaturize:(nullable id)sender;
- (void)toggleFullScreen:(nullable id)sender;
- (void)zoom:(nullable id)sender;
- (void)center;
@property(nullable, weak) NSResponder *firstResponder;
- (BOOL)makeFirstResponder:(nullable NSResponder *)responder;
@property NSWindowLevel level;
@property NSWindowCollectionBehavior collectionBehavior;
@property BOOL acceptsMouseMovedEvents;
- (NSPoint)convertPointToScreen:(NSPoint)point;
- (NSPoint)convertPointFromScreen:(NSPoint)point;
- (NSRect)convertRectToScreen:(NSRect)rect;
- (NSRect)convertRectFromScreen:(NSRect)rect;
- (nullable NSView *)standardWindowButton:(NSWindowButton)b;
- (void)performWindowDragWithEvent:(NSEvent *)event;
- (void)orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)otherWin;
- (NSTimeInterval)animationResizeTime:(NSRect)newFrame;
- (nullable id)animationForKey:(NSString *)key;
@property(readonly, strong) id animator;
@property BOOL hidesOnDeactivate;
@property BOOL hasShadow;
- (void)addChildWindow:(NSWindow *)childWin ordered:(NSWindowOrderingMode)place;
- (void)removeChildWindow:(NSWindow *)childWin;
@property(readonly, copy) NSArray<NSWindow *> *childWindows;
@property(nullable, weak) NSWindow *parentWindow;
@property BOOL canHide;
@property BOOL displaysWhenScreenProfileChanges;
@property BOOL releasedWhenClosed;
@property BOOL restorable;
@property(nullable, strong) NSAppearance *appearance;
@property(readonly, strong) NSAppearance *effectiveAppearance;
- (void)invalidateShadow;
- (void)display;
- (void)orderFrontRegardless;
@property(nullable, copy) NSColor *backgroundColor;
- (CADisplayLink *)displayLinkWithTarget:(id)target selector:(SEL)selector;
@end

/* ---- NSScreen ------------------------------------------------------------ */
@interface NSScreen : NSObject
@property(class, readonly, copy) NSArray<NSScreen *> *screens;
@property(class, readonly, nullable) NSScreen *mainScreen;
@property(readonly) NSRect frame;
@property(readonly) NSRect visibleFrame;
@property(readonly) CGFloat backingScaleFactor;
@property(readonly, copy) NSDictionary<NSString *, id> *deviceDescription;
@property(readonly, copy) NSString *localizedName;
@property(readonly) NSInteger maximumFramesPerSecond;
- (NSRect)convertRectToBacking:(NSRect)rect;
- (NSRect)convertRectFromBacking:(NSRect)rect;
@end

/* ---- NSEvent ------------------------------------------------------------- */
@interface NSEvent : NSObject
@property(readonly) NSEventType type;
@property(readonly) NSPoint locationInWindow;
@property(readonly) NSEventModifierFlags modifierFlags;
@property(readonly) NSTimeInterval timestamp;
@property(nullable, readonly, weak) NSWindow *window;
@property(readonly) unsigned short keyCode;
@property(nullable, readonly, copy) NSString *characters;
@property(nullable, readonly, copy) NSString *charactersIgnoringModifiers;
@property(readonly, getter=isARepeat) BOOL ARepeat;
@property(readonly) NSInteger buttonNumber;
@property(readonly) NSInteger clickCount;
@property(readonly) float pressure;
@property(readonly) CGFloat deltaX;
@property(readonly) CGFloat deltaY;
@property(readonly) CGFloat deltaZ;
@property(readonly) CGFloat scrollingDeltaX;
@property(readonly) CGFloat scrollingDeltaY;
@property(readonly) BOOL hasPreciseScrollingDeltas;
@property(readonly) NSEventPhase phase;
@property(readonly) NSEventPhase momentumPhase;
@property(readonly) CGFloat magnification;
@property(class, readonly) NSPoint mouseLocation;
@property(class, readonly) NSEventModifierFlags modifierFlags;
@property(class, readonly) NSTimeInterval doubleClickInterval;
+ (nullable id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask
											handler:(NSEvent *_Nullable (^)(NSEvent *event))block;
+ (void)removeMonitor:(id)eventMonitor;
@end

/* ---- NSCursor ------------------------------------------------------------ */
@interface NSCursor : NSObject
@property(class, readonly) NSCursor *currentCursor;
@property(class, readonly) NSCursor *arrowCursor;
@property(class, readonly) NSCursor *IBeamCursor;
@property(class, readonly) NSCursor *crosshairCursor;
@property(class, readonly) NSCursor *closedHandCursor;
@property(class, readonly) NSCursor *openHandCursor;
@property(class, readonly) NSCursor *pointingHandCursor;
@property(class, readonly) NSCursor *resizeLeftCursor;
@property(class, readonly) NSCursor *resizeRightCursor;
@property(class, readonly) NSCursor *resizeLeftRightCursor;
@property(class, readonly) NSCursor *resizeUpCursor;
@property(class, readonly) NSCursor *resizeDownCursor;
@property(class, readonly) NSCursor *resizeUpDownCursor;
@property(class, readonly) NSCursor *operationNotAllowedCursor;
@property(class, readonly) NSCursor *dragLinkCursor;
@property(class, readonly) NSCursor *dragCopyCursor;
@property(class, readonly) NSCursor *contextualMenuCursor;
@property(class, readonly) NSCursor *disappearingItemCursor;
@property(class, readonly) NSCursor *IBeamCursorForVerticalLayout;
@property(class, readonly) NSCursor *zoomInCursor;
@property(class, readonly) NSCursor *zoomOutCursor;
@property(class, readonly) NSCursor *columnResizeCursor;
@property(class, readonly) NSCursor *rowResizeCursor;
- (void)set;
+ (void)hide;
+ (void)unhide;
- (void)push;
- (void)pop;
@end

@class NSColorSpace;

/* ---- NSColor ------------------------------------------------------------- */
@interface NSColor : NSObject
@property(class, readonly) NSColor *clearColor;
@property(class, readonly) NSColor *whiteColor;
@property(class, readonly) NSColor *blackColor;
@property(class, readonly) NSColor *windowBackgroundColor;
@property(readonly) CGColorRef CGColor;
+ (NSColor *)colorWithWhite:(CGFloat)white alpha:(CGFloat)alpha;
+ (NSColor *)colorWithCalibratedRed:(CGFloat)red
							  green:(CGFloat)green
							   blue:(CGFloat)blue
							  alpha:(CGFloat)alpha;
+ (NSColor *)colorWithSRGBRed:(CGFloat)red
						green:(CGFloat)green
						 blue:(CGFloat)blue
						alpha:(CGFloat)alpha;
- (void)set;
/* Reading components is only defined once the colour has been converted into a component-based
   space — a colour straight out of the picker may be a pattern or a catalogue entry. */
- (nullable NSColor *)colorUsingColorSpace:(NSColorSpace *)space;
@property(readonly) CGFloat redComponent;
@property(readonly) CGFloat greenComponent;
@property(readonly) CGFloat blueComponent;
@property(readonly) CGFloat alphaComponent;
@end

/* ---- NSColorSpace -------------------------------------------------------- */
@interface NSColorSpace : NSObject
@property(class, readonly) NSColorSpace *sRGBColorSpace;
@property(class, readonly) NSColorSpace *genericRGBColorSpace;
@end

@class UTType;

/* ---- NSTrackingArea ------------------------------------------------------ */
@interface NSTrackingArea : NSObject
- (instancetype)initWithRect:(NSRect)rect
					 options:(NSTrackingAreaOptions)options
					   owner:(nullable id)owner
					userInfo:(nullable NSDictionary *)userInfo;
@property(readonly) NSRect rect;
@property(readonly) NSTrackingAreaOptions options;
@property(nullable, readonly) id owner;
@end

/* ---- NSPasteboard -------------------------------------------------------- */
@interface NSPasteboard : NSObject
@property(class, readonly) NSPasteboard *generalPasteboard;
- (NSInteger)clearContents;
- (BOOL)writeObjects:(NSArray *)objects;
- (nullable NSArray *)
		readObjectsForClasses:(NSArray *)classArray
					  options:(nullable NSDictionary<NSPasteboardReadingOptionKey, id> *)options;
- (nullable NSString *)stringForType:(NSPasteboardType)dataType;
- (BOOL)setString:(NSString *)string forType:(NSPasteboardType)dataType;
- (nullable NSData *)dataForType:(NSPasteboardType)dataType;
- (BOOL)setData:(NSData *)data forType:(NSPasteboardType)dataType;
- (NSInteger)declareTypes:(NSArray<NSPasteboardType> *)newTypes owner:(nullable id)newOwner;
- (BOOL)canReadItemWithDataConformingToTypes:(NSArray<NSString *> *)types;
- (BOOL)canReadObjectForClasses:(NSArray *)classArray options:(nullable NSDictionary *)options;
- (NSInteger)prepareForNewContentsWithOptions:(NSPasteboardContentsOptions)options;
- (nullable NSPasteboardType)availableTypeFromArray:(NSArray<NSPasteboardType> *)types;
@property(nullable, readonly, copy) NSArray<NSPasteboardType> *types;
@property(nullable, readonly, copy) NSArray<NSPasteboardItem *> *pasteboardItems;
@property(readonly) NSInteger changeCount;
@end

@protocol NSPasteboardItemDataProvider <NSObject>
- (void)pasteboard:(nullable NSPasteboard *)pasteboard
					  item:(NSPasteboardItem *)item
		provideDataForType:(NSPasteboardType)type;
@optional
- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard;
@end

@interface NSPasteboardItem : NSObject
- (BOOL)setData:(NSData *)data forType:(NSPasteboardType)type;
- (BOOL)setString:(NSString *)string forType:(NSPasteboardType)type;
- (BOOL)setDataProvider:(id<NSPasteboardItemDataProvider>)dataProvider
			   forTypes:(NSArray<NSPasteboardType> *)types;
- (nullable NSData *)dataForType:(NSPasteboardType)type;
- (nullable NSString *)stringForType:(NSPasteboardType)type;
- (nullable NSPasteboardType)availableTypeFromArray:(NSArray<NSPasteboardType> *)types;
@property(readonly, copy) NSArray<NSPasteboardType> *types;
@end

/* ---- NSAppearance -------------------------------------------------------- */
@interface NSAppearance : NSObject
@property(class, readonly, strong) NSAppearance *currentDrawingAppearance;
+ (nullable NSAppearance *)appearanceNamed:(NSAppearanceName)name;
@property(readonly) NSAppearanceName name;
- (nullable NSAppearanceName)bestMatchFromAppearancesWithNames:
		(NSArray<NSAppearanceName> *)appearances;
@end

/* ---- NSAnimationContext -------------------------------------------------- */
@interface NSAnimationContext : NSObject
@property(class, readonly, strong) NSAnimationContext *currentContext;
+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes
		completionHandler:(nullable void (^)(void))completionHandler;
+ (void)beginGrouping;
+ (void)endGrouping;
@property NSTimeInterval duration;
@end

/* ---- NSApplication ------------------------------------------------------- */
@class NSImage;

@interface NSApplication : NSResponder
@property(class, readonly, strong) NSApplication *sharedApplication;
@property(weak, nullable) id delegate;
- (void)run;
- (void)stop:(nullable id)sender;
- (void)terminate:(nullable id)sender;
- (BOOL)setActivationPolicy:(NSApplicationActivationPolicy)activationPolicy;
- (void)activateIgnoringOtherApps:(BOOL)flag;
- (void)finishLaunching;
@property(nullable, strong) NSMenu *mainMenu;
/* The Dock / Cmd-Tab tile. Set explicitly because CFBundleIconFile alone is often ignored for
   linker- or ad-hoc-signed bundles until the IconServices caches catch up. */
@property(nullable, strong) NSImage *applicationIconImage;
@property(nullable, readonly, weak) NSWindow *keyWindow;
@property(nullable, readonly, weak) NSWindow *mainWindow;
@property(readonly, copy) NSArray<NSWindow *> *windows;
@property(readonly, strong) NSAppearance *effectiveAppearance;
@property NSApplicationPresentationOptions presentationOptions;
- (void)sendEvent:(NSEvent *)event;
- (void)activate;
- (NSInteger)runModalForWindow:(NSWindow *)window;
- (void)abortModal;
- (void)stopModal;
@end

SPRT_FOUNDATION_EXTERN NSApplication *NSApp;

@interface NSWorkspace : NSObject
@property(class, readonly, strong) NSWorkspace *sharedWorkspace;
@property(readonly, strong) NSNotificationCenter *notificationCenter;
- (BOOL)openURL:(NSURL *)url;
/* Reveal: opens each item's containing folder with the item selected. */
- (void)activateFileViewerSelectingURLs:(NSArray<NSURL *> *)fileURLs;
@end

@interface NSRunningApplication : NSObject
@property(class, readonly, nullable) NSRunningApplication *currentApplication;
@property(nullable, readonly, copy) NSString *bundleIdentifier;
@property(nullable, readonly, copy) NSString *localizedName;
@property(readonly) int processIdentifier;
@property(readonly, getter=isFinishedLaunching) BOOL finishedLaunching;
- (BOOL)activateWithOptions:(NSUInteger)options;
@end

/* ---- protocols ----------------------------------------------------------- */
@protocol NSApplicationDelegate <NSObject>
@optional
- (void)applicationDidFinishLaunching:(NSNotification *)notification;
- (void)applicationWillTerminate:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender;
@end

@protocol NSWindowDelegate <NSObject>
@optional
- (void)windowDidResize:(NSNotification *)notification;
- (void)windowDidMove:(NSNotification *)notification;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;
- (void)windowDidBecomeMain:(NSNotification *)notification;
- (void)windowDidResignMain:(NSNotification *)notification;
- (void)windowDidMiniaturize:(NSNotification *)notification;
- (void)windowDidDeminiaturize:(NSNotification *)notification;
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidChangeScreen:(NSNotification *)notification;
- (void)windowDidChangeBackingProperties:(NSNotification *)notification;
- (void)windowWillEnterFullScreen:(NSNotification *)notification;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowWillExitFullScreen:(NSNotification *)notification;
- (void)windowDidExitFullScreen:(NSNotification *)notification;
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize;
- (nullable NSArray<NSWindow *> *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window;
- (nullable NSArray<NSWindow *> *)customWindowsToExitFullScreenForWindow:(NSWindow *)window;
@end

@protocol NSViewLayerContentScaleDelegate <NSObject>
@optional
- (BOOL)layer:(CALayer *)layer
		shouldInheritContentsScale:(CGFloat)newScale
						fromWindow:(NSWindow *)window;
@end

@protocol NSTextInputClient <NSObject>
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange;
- (void)doCommandBySelector:(SEL)selector;
- (void)setMarkedText:(id)string
		   selectedRange:(NSRange)selectedRange
		replacementRange:(NSRange)replacementRange;
- (void)unmarkText;
- (NSRange)selectedRange;
- (NSRange)markedRange;
- (BOOL)hasMarkedText;
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
														 actualRange:(nullable NSRangePointer)
																			 actualRange;
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText;
- (NSRect)firstRectForCharacterRange:(NSRange)range
						 actualRange:(nullable NSRangePointer)actualRange;
- (NSUInteger)characterIndexForPoint:(NSPoint)point;

@optional
/* Not required, but an input method asks for them: attributedString feeds inline candidate
   display, the two geometry callbacks let it place a candidate window against a glyph, and
   windowLevel keeps that window above a full-screen surface. */
- (NSAttributedString *)attributedString;
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point;
- (CGFloat)baselineDeltaForCharacterAtIndex:(NSUInteger)anIndex;
- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex;
- (NSInteger)windowLevel;
@end

@interface NSImageRep : NSObject
@end

@interface NSImage : NSObject
- (instancetype)initWithSize:(NSSize)size;
- (nullable instancetype)initWithContentsOfFile:(NSString *)fileName;
- (void)addRepresentation:(NSImageRep *)imageRep;
@property NSSize size;
@end

@interface NSBitmapImageRep : NSImageRep
- (nullable instancetype)initWithBitmapDataPlanes:(unsigned char *_Nullable *_Nullable)planes
									   pixelsWide:(NSInteger)width
									   pixelsHigh:(NSInteger)height
									bitsPerSample:(NSInteger)bps
								  samplesPerPixel:(NSInteger)spp
										 hasAlpha:(BOOL)alpha
										 isPlanar:(BOOL)isPlanar
								   colorSpaceName:(NSColorSpaceName)colorSpaceName
									 bitmapFormat:(NSBitmapFormat)bitmapFormat
									  bytesPerRow:(NSInteger)rBytes
									 bitsPerPixel:(NSInteger)pBits;
@property(nullable, readonly) unsigned char *bitmapData;
@end

@interface NSBundle (SPRTAppKitImages)
- (nullable NSImage *)imageForResource:(NSString *)name;
@end

/* ---- NSPanel and the system dialogs -------------------------------------- */

/* A panel is an auxiliary window; every dialog below is one, which is what makes
   makeKeyAndOrderFront:/close work on them. Deriving from it is also the only way to get the
   panel-only style bits (utility title bar, non-activating) - and note the inherited defaults
   differ from NSWindow: hidesOnDeactivate is YES, releasedWhenClosed is NO. */
@interface NSPanel : NSWindow
@property(getter=isFloatingPanel) BOOL floatingPanel;
@property BOOL becomesKeyOnlyIfNeeded;
@property BOOL worksWhenModal;
@end

/* NSSavePanel doubles as the base of NSOpenPanel, so everything shared lives here. Note the two
   ways to run one: beginSheetModalForWindow: attaches it to a parent (a real sheet, which the OS
   blocks and raises for us), beginWithCompletionHandler: leaves it free-standing. Neither blocks. */
@interface NSSavePanel : NSPanel
+ (NSSavePanel *)savePanel;
@property(copy) NSString *message;
@property(copy) NSString *prompt;
@property(copy) NSString *nameFieldStringValue;
@property(nullable, copy) NSURL *directoryURL;
@property BOOL canCreateDirectories;
@property BOOL showsHiddenFiles;
@property BOOL allowsOtherFileTypes;
@property(copy) NSArray<UTType *> *allowedContentTypes;
@property(nullable, readonly, copy) NSURL *URL;
- (void)beginSheetModalForWindow:(NSWindow *)window
			   completionHandler:(void (^)(NSModalResponse result))handler;
- (void)beginWithCompletionHandler:(void (^)(NSModalResponse result))handler;
- (void)cancel:(nullable id)sender;
@end

@interface NSOpenPanel : NSSavePanel
+ (NSOpenPanel *)openPanel;
@property BOOL canChooseFiles;
@property BOOL canChooseDirectories;
@property BOOL allowsMultipleSelection;
@property(readonly, copy) NSArray<NSURL *> *URLs;
@end

/* Both of these are process-wide singletons with no completion handler: they report through the
   shared font manager / their own colour property, and "the user is done" is the window closing. */
@interface NSColorPanel : NSPanel
@property(class, readonly, strong) NSColorPanel *sharedColorPanel;
@property BOOL showsAlpha;
@property(copy) NSColor *color;
@end

@interface NSFontPanel : NSPanel
@end

@interface NSFont : NSObject
+ (nullable NSFont *)fontWithName:(NSString *)fontName size:(CGFloat)fontSize;
+ (NSFont *)systemFontOfSize:(CGFloat)fontSize;
@property(readonly, copy) NSString *fontName;
@property(readonly, nullable, copy) NSString *familyName;
@property(readonly) CGFloat pointSize;
@end

@interface NSFontManager : NSObject
@property(class, readonly, strong) NSFontManager *sharedFontManager;
- (NSFontPanel *)fontPanel:(BOOL)create;
- (void)setSelectedFont:(NSFont *)fontObj isMultiple:(BOOL)flag;
- (nullable NSFont *)selectedFont;
/* Applies whatever the user changed in the panel to `fontObj` — the documented way to read the
   panel's choice, since the panel itself exposes no font. */
- (NSFont *)convertFont:(NSFont *)fontObj;
- (NSFontTraitMask)traitsOfFont:(NSFont *)fontObj;
@end

/* ---- pasteboard type + appearance-name constants ------------------------- */
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeString;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeURL;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeFileURL;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypePNG;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeTIFF;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeTabularText;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeRTF;
SPRT_FOUNDATION_EXTERN NSPasteboardType const NSPasteboardTypeHTML;
SPRT_FOUNDATION_EXTERN NSPasteboardReadingOptionKey const NSPasteboardURLReadingFileURLsOnlyKey;
SPRT_FOUNDATION_EXTERN NSAppearanceName const NSAppearanceNameAqua;
SPRT_FOUNDATION_EXTERN NSAppearanceName const NSAppearanceNameDarkAqua;
SPRT_FOUNDATION_EXTERN NSColorSpaceName const NSDeviceRGBColorSpace;

/* ---- window notifications ------------------------------------------------ */
SPRT_FOUNDATION_EXTERN NSNotificationName const NSWindowWillCloseNotification;

#endif /* __SPRT_OPEN_APPKIT_H_ */
