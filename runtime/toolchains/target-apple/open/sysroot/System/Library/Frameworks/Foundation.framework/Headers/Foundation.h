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

/*
	Minimal hand-written <Foundation/Foundation.h> for the Xcode-SDK-free macOS
	target (*-apple-macosx+open). Foundation is a closed Objective-C framework with
	no open-source headers, so this reconstructs ONLY the surface the runtime
	actually uses (runtime/libc_wrapper/runtime/SPRuntimeFilesystem-macos.mm): the
	base value classes, NSProcessInfo, NSFileManager, NSBundle, and the standard
	NSSearchPath* path-discovery API.

	The declarations only have to satisfy the compiler — every class, selector and
	function is provided at run time by the real Foundation.dylib (the generated
	.tbd stub plus the link-time -undefined dynamic_lookup catch-all carry the
	symbols). The enum values and signatures mirror the documented Foundation ABI,
	so the calls are binary-correct on a real system (we build/link here only; the
	target cannot be executed on Linux).

	Built on the apple-oss objc4 runtime headers (<objc/*>) and the swift-corelibs
	CoreFoundation headers already installed in this sysroot.
*/

#ifndef _SPRT_OPEN_FOUNDATION_FOUNDATION_H_
#define _SPRT_OPEN_FOUNDATION_FOUNDATION_H_

#import <objc/objc.h>
#import <objc/NSObject.h>
#import <objc/NSObjCRuntime.h>
#import <CoreFoundation/CoreFoundation.h>
/* SDK parity: the Foundation umbrella exposes libdispatch types (dispatch_queue_t
 * appears in Foundation API and in consumers' own declarations — e.g. lldb's
 * CoreSimulator protocol mirrors). */
#import <dispatch/dispatch.h>

#if defined(__cplusplus)
#define SPRT_FOUNDATION_EXTERN extern "C"
#else
#define SPRT_FOUNDATION_EXTERN extern
#endif

/* NS_ENUM / NS_OPTIONS: Foundation's <NSObjCRuntime.h> provides these on macOS,
   but the apple-oss objc4 <objc/NSObjCRuntime.h> does not. Canonical definition
   (used as `typedef NS_ENUM(NSUInteger, Name) { ... };`). */
#ifndef NS_ENUM
#define NS_ENUM(_type, _name)    enum _name : _type _name; enum _name : _type
/* NS_OPTIONS: in C++ the name is a plain typedef of the underlying integer type
   (the enumerators live in an anonymous fixed-type enum) — this is what lets
   `A | B` of two flags stay assignable to the option-typed lvalue. In C it is a
   named fixed-type enum like NS_ENUM. Matches Apple's CF_OPTIONS. */
#if defined(__cplusplus)
#define NS_OPTIONS(_type, _name) _type _name; enum : _type
#else
#define NS_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
#endif
#endif
#ifndef NS_ASSUME_NONNULL_BEGIN
#define NS_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define NS_ASSUME_NONNULL_END   _Pragma("clang assume_nonnull end")
#endif

/* No-op shims for the Apple availability / Swift-bridging / Metal decoration macros the
   SDK supplies via <os/availability.h>, <Foundation/NSObjCRuntime.h> and <Metal/MTLDefines.h>.
   In the +open sysroot the framework headers are hand-written and carry no availability data,
   so these expand to nothing (or the plain C keyword). MTL_INLINE MUST expand to `static
   inline` (not empty) or the header's inline helpers would emit external definitions in every
   TU and collide at link. Foundation.h is the universal first include of every framework
   header, so defining them here makes every hand-written framework header self-sufficient. */
#ifndef API_AVAILABLE
#define API_AVAILABLE(...)
#define API_DEPRECATED(...)
#define API_DEPRECATED_WITH_REPLACEMENT(...)
#define API_UNAVAILABLE(...)
#endif
#ifndef NS_SWIFT_NAME
#define NS_SWIFT_NAME(...)
#define NS_SWIFT_UNAVAILABLE(...)
#endif
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif
#ifndef NS_REFINED_FOR_SWIFT
#define NS_REFINED_FOR_SWIFT
#endif
#ifndef NS_RETURNS_INNER_POINTER
#define NS_RETURNS_INNER_POINTER
#endif
#ifndef NS_UNAVAILABLE
#define NS_UNAVAILABLE
#endif
#ifndef MTL_EXPORT
#define MTL_EXPORT extern
#endif
#ifndef MTL_INLINE
#define MTL_INLINE static inline
#endif
#ifndef ABS
#define ABS(A)    ((A) < 0 ? (-(A)) : (A))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

/* --- fast enumeration: required for `for (id x in collection)` lowering --- */
typedef struct {
	unsigned long state;
	id __unsafe_unretained *itemsPtr;
	unsigned long *mutationsPtr;
	unsigned long extra[5];
} NSFastEnumerationState;

@protocol NSFastEnumeration
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
								  objects:(id __unsafe_unretained *)buffer
									count:(NSUInteger)len;
@end

/* Marker protocols the SDK declares in <Foundation/NSObject.h> — the closed-framework
   descriptor/value classes (e.g. Metal's MTL*Descriptor) declare `<NSCopying>` &c.
   conformance. We never call copyWithZone:/encode: here, so empty bodies suffice. */
@protocol NSCopying @end
@protocol NSMutableCopying @end
@protocol NSCoding @end
@protocol NSSecureCoding <NSCoding> @end

/* --- base value classes (only the members the runtime touches) --- */
@interface NSString : NSObject
@property (readonly) const char *UTF8String;
@end

@interface NSArray<__covariant ObjectType> : NSObject <NSFastEnumeration>
@property (readonly) NSUInteger count;
- (ObjectType)objectAtIndex:(NSUInteger)index;
- (ObjectType)objectAtIndexedSubscript:(NSUInteger)idx;
+ (instancetype)array;
+ (instancetype)arrayWithObjects:(const ObjectType _Nonnull [_Nullable])objects count:(NSUInteger)cnt;
+ (instancetype)arrayWithObject:(ObjectType)anObject;
- (BOOL)containsObject:(ObjectType)anObject;
- (NSUInteger)indexOfObject:(ObjectType)anObject;
- (NSUInteger)indexOfObjectIdenticalTo:(ObjectType)anObject;
@end

@interface NSDictionary<__covariant KeyType, __covariant ObjectType> : NSObject <NSFastEnumeration>
- (nullable ObjectType)objectForKey:(KeyType)aKey;
- (nullable ObjectType)objectForKeyedSubscript:(KeyType)key;
@property (readonly) NSUInteger count;
@property (readonly, copy) NSArray<KeyType> *allKeys;
@property (readonly, copy) NSArray<ObjectType> *allValues;
+ (instancetype)dictionaryWithObjects:(const ObjectType _Nonnull [_Nullable])objects forKeys:(const KeyType _Nonnull [_Nullable])keys count:(NSUInteger)cnt;
@end

/* --- process / filesystem / bundle services --- */
typedef struct {
	NSInteger majorVersion;
	NSInteger minorVersion;
	NSInteger patchVersion;
} NSOperatingSystemVersion;

@interface NSProcessInfo : NSObject
@property (class, readonly) NSProcessInfo *processInfo;
@property (readonly) NSDictionary *environment;
@property (readonly) NSOperatingSystemVersion operatingSystemVersion;
@property (readonly) NSUInteger activeProcessorCount;
@property (readonly) NSUInteger processorCount;
@property (readonly) unsigned long long physicalMemory;
- (BOOL)isOperatingSystemAtLeastVersion:(NSOperatingSystemVersion)version;
@end

@interface NSFileManager : NSObject
@property (class, readonly) NSFileManager *defaultManager;
@property (readonly, copy) NSString *currentDirectoryPath;
- (BOOL)isWritableFileAtPath:(NSString *)path;
- (BOOL)isReadableFileAtPath:(NSString *)path;
- (BOOL)fileExistsAtPath:(NSString *)path;
- (BOOL)fileExistsAtPath:(NSString *)path isDirectory:(BOOL *)isDirectory;
@end

@interface NSBundle : NSObject
@property (class, readonly) NSBundle *mainBundle;
@property (readonly) NSString *resourcePath;
@end

/* --- standard directory search API (NSPathUtilities.h) --- */
typedef enum : NSUInteger {
	NSApplicationDirectory = 1,
	NSDemoApplicationDirectory = 2,
	NSDeveloperApplicationDirectory = 3,
	NSAdminApplicationDirectory = 4,
	NSLibraryDirectory = 5,
	NSDeveloperDirectory = 6,
	NSUserDirectory = 7,
	NSDocumentationDirectory = 8,
	NSDocumentDirectory = 9,
	NSCoreServiceDirectory = 10,
	NSAutosavedInformationDirectory = 11,
	NSDesktopDirectory = 12,
	NSCachesDirectory = 13,
	NSApplicationSupportDirectory = 14,
	NSDownloadsDirectory = 15,
	NSInputMethodsDirectory = 16,
	NSMoviesDirectory = 17,
	NSMusicDirectory = 18,
	NSPicturesDirectory = 19,
	NSPrinterDescriptionDirectory = 20,
	NSSharedPublicDirectory = 21,
	NSPreferencePanesDirectory = 22,
	NSApplicationScriptsDirectory = 23,
	NSItemReplacementDirectory = 99,
	NSAllApplicationsDirectory = 100,
	NSAllLibrariesDirectory = 101,
	NSTrashDirectory = 102,
} NSSearchPathDirectory;

typedef enum : NSUInteger {
	NSUserDomainMask = 1,
	NSLocalDomainMask = 2,
	NSNetworkDomainMask = 4,
	NSSystemDomainMask = 8,
	NSAllDomainsMask = 0x0ffff,
} NSSearchPathDomainMask;

/* domainMask is typed NSUInteger (not NSSearchPathDomainMask): callers OR several
   mask enumerators together, and in C++ that promotes to the underlying integer
   type, which would not implicitly convert back to the enum. Binary-identical. */
SPRT_FOUNDATION_EXTERN NSArray *NSSearchPathForDirectoriesInDomains(
		NSSearchPathDirectory directory, NSUInteger domainMask, BOOL expandTilde);

SPRT_FOUNDATION_EXTERN NSString *NSTemporaryDirectory(void);
SPRT_FOUNDATION_EXTERN void NSLog(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));

/* ======================================================================== *
 *  Window-backend surface (runtime/window/macos) — extends the base above.
 * ======================================================================== */

#include <CoreFoundation/CFCGTypes.h>   /* CGPoint/CGSize/CGRect/CGFloat */

#define NS_INLINE static __inline__ __attribute__((always_inline))
#ifndef NSIntegerMax
#define NSIntegerMax  __LONG_MAX__
#define NSIntegerMin  (-__LONG_MAX__ - 1L)
#define NSUIntegerMax (~0UL)
#endif
#define NSNotFound NSIntegerMax

typedef double NSTimeInterval;

typedef enum : NSInteger {
	NSOrderedAscending = -1L, NSOrderedSame, NSOrderedDescending
} NSComparisonResult;

/* --- geometry: on 64-bit macOS the NS* geometry types ARE the CG* types --- */
typedef CGPoint NSPoint;
typedef CGSize  NSSize;
typedef CGRect  NSRect;
typedef CGPoint *NSPointPointer;
typedef CGSize  *NSSizePointer;
typedef CGRect  *NSRectPointer;
typedef NSUInteger NSRectEdge;

typedef struct _NSRange { NSUInteger location; NSUInteger length; } NSRange;
typedef NSRange *NSRangePointer;

NS_INLINE NSRange NSMakeRange(NSUInteger loc, NSUInteger len) { NSRange r; r.location = loc; r.length = len; return r; }
NS_INLINE NSPoint NSMakePoint(CGFloat x, CGFloat y) { NSPoint p; p.x = x; p.y = y; return p; }
NS_INLINE NSSize  NSMakeSize(CGFloat w, CGFloat h) { NSSize s; s.width = w; s.height = h; return s; }
NS_INLINE NSRect  NSMakeRect(CGFloat x, CGFloat y, CGFloat w, CGFloat h) { NSRect r; r.origin.x = x; r.origin.y = y; r.size.width = w; r.size.height = h; return r; }
NS_INLINE CGFloat NSMaxX(NSRect r) { return r.origin.x + r.size.width; }
NS_INLINE CGFloat NSMaxY(NSRect r) { return r.origin.y + r.size.height; }
NS_INLINE CGFloat NSMinX(NSRect r) { return r.origin.x; }
NS_INLINE CGFloat NSMinY(NSRect r) { return r.origin.y; }
NS_INLINE CGFloat NSWidth(NSRect r) { return r.size.width; }
NS_INLINE CGFloat NSHeight(NSRect r) { return r.size.height; }

/* --- common string-typed aliases --- */
typedef NSString *NSNotificationName;
typedef NSString *NSErrorUserInfoKey;
typedef NSString *NSAttributedStringKey;
typedef NSString *NSPasteboardType;
typedef NSString *NSRunLoopMode;

typedef NS_ENUM(NSUInteger, NSStringEncoding) {
	NSASCIIStringEncoding = 1, NSUTF8StringEncoding = 4,
	NSUnicodeStringEncoding = 10, NSUTF16StringEncoding = 10,
	NSUTF16LittleEndianStringEncoding = 0x94000100,
	NSUTF32StringEncoding = 0x8c000100,
};

/* NSObject additions (Foundation's NSObject.h category over the objc4 root). */
@interface NSObject (SPRTFoundationPerform)
- (void)performSelector:(SEL)aSelector withObject:(nullable id)anArgument afterDelay:(NSTimeInterval)delay;
+ (void)cancelPreviousPerformRequestsWithTarget:(id)aTarget;
+ (void)cancelPreviousPerformRequestsWithTarget:(id)aTarget selector:(SEL)aSelector object:(nullable id)anArgument;
@end

/* --- string (extends the base NSString above via a category-style re-open) --- */
@interface NSString ()
@property (readonly) NSUInteger length;
+ (instancetype)stringWithUTF8String:(const char *)nullTerminatedCString;
+ (instancetype)stringWithCString:(const char *)cString encoding:(NSStringEncoding)enc;
+ (instancetype)stringWithString:(NSString *)string;
- (instancetype)initWithUTF8String:(const char *)nullTerminatedCString;
- (instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)len encoding:(NSStringEncoding)encoding;
- (BOOL)isEqualToString:(NSString *)aString;
- (NSString *)stringByAppendingString:(NSString *)aString;
- (unsigned short)characterAtIndex:(NSUInteger)index;
- (NSString *)substringWithRange:(NSRange)range;
+ (instancetype)stringWithFormat:(NSString *)format, ...;
- (NSComparisonResult)compare:(NSString *)string;
- (NSComparisonResult)caseInsensitiveCompare:(NSString *)string;
/* NSPathUtilities category (MoltenVK shader-cache path handling). */
@property (readonly) NSString *lastPathComponent;
@property (readonly) NSString *pathExtension;
@property (readonly) NSString *stringByDeletingPathExtension;
@property (readonly) NSString *stringByExpandingTildeInPath;
@property (readonly) NSString *absolutePath;
- (NSString *)stringByAppendingPathComponent:(NSString *)str;
- (NSString *)stringByAppendingPathExtension:(NSString *)str;
@end

NS_INLINE BOOL NSPointInRect(NSPoint p, NSRect r) {
	return p.x >= r.origin.x && p.y >= r.origin.y
		&& p.x < r.origin.x + r.size.width && p.y < r.origin.y + r.size.height;
}
NS_INLINE BOOL NSEqualRects(NSRect a, NSRect b) {
	return a.origin.x == b.origin.x && a.origin.y == b.origin.y
		&& a.size.width == b.size.width && a.size.height == b.size.height;
}
NS_INLINE BOOL NSEqualSizes(NSSize a, NSSize b) { return a.width == b.width && a.height == b.height; }

@interface NSValue : NSObject
+ (NSValue *)valueWithRange:(NSRange)range;
@property (readonly) NSRange rangeValue;
@end

@interface NSNumber : NSValue
+ (NSNumber *)numberWithBool:(BOOL)value;
+ (NSNumber *)numberWithInt:(int)value;
+ (NSNumber *)numberWithUnsignedInt:(unsigned int)value;
+ (NSNumber *)numberWithInteger:(NSInteger)value;
+ (NSNumber *)numberWithUnsignedInteger:(NSUInteger)value;
+ (NSNumber *)numberWithDouble:(double)value;
@property (readonly) BOOL boolValue;
@property (readonly) int intValue;
@property (readonly) unsigned int unsignedIntValue;
@property (readonly) NSInteger integerValue;
@property (readonly) NSUInteger unsignedIntegerValue;
@property (readonly) double doubleValue;
@end

@class NSError;   /* full @interface appears further down; forward-declared for NSData below */

typedef NS_ENUM(NSInteger, NSDataCompressionAlgorithm) {
	NSDataCompressionAlgorithmLZFSE = 0,
	NSDataCompressionAlgorithmLZ4,
	NSDataCompressionAlgorithmLZMA,
	NSDataCompressionAlgorithmZlib,
};

@interface NSData : NSObject
- (instancetype)initWithBytes:(nullable const void *)bytes length:(NSUInteger)length;
+ (instancetype)dataWithBytes:(nullable const void *)bytes length:(NSUInteger)length;
@property (readonly) const void *bytes;
@property (readonly) NSUInteger length;
- (nullable NSData *)compressedDataUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error;
- (nullable NSData *)decompressedDataUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error;
@end

@interface NSMutableArray<ObjectType> : NSArray<ObjectType>
+ (instancetype)array;
+ (instancetype)arrayWithCapacity:(NSUInteger)numItems;
- (instancetype)initWithCapacity:(NSUInteger)numItems;
- (void)addObject:(ObjectType)anObject;
- (void)removeAllObjects;
- (void)removeObjectAtIndex:(NSUInteger)index;
@end

@interface NSMutableString : NSString
+ (instancetype)stringWithCapacity:(NSUInteger)capacity;
- (void)appendString:(NSString *)aString;
- (void)appendFormat:(NSString *)format, ...;
@end

/* <Foundation/NSByteOrder.h> host/big/little swappers MoltenVK uses to pack pipeline-cache
   UUIDs. The +open macOS targets (x86_64/arm64) are always little-endian, so host->little is
   identity and host->big is a byte reversal. */
static inline unsigned int       NSSwapInt(unsigned int x)            { return __builtin_bswap32(x); }
static inline unsigned long long NSSwapLongLong(unsigned long long x) { return __builtin_bswap64(x); }
static inline unsigned int       NSSwapHostIntToBig(unsigned int x)   { return NSSwapInt(x); }
static inline unsigned int       NSSwapHostIntToLittle(unsigned int x){ return x; }
static inline unsigned int       NSSwapLittleIntToHost(unsigned int x){ return x; }
static inline unsigned int       NSSwapBigIntToHost(unsigned int x)   { return NSSwapInt(x); }
static inline unsigned long long NSSwapHostLongLongToBig(unsigned long long x) { return NSSwapLongLong(x); }
static inline unsigned long long NSSwapBigLongLongToHost(unsigned long long x) { return NSSwapLongLong(x); }

@interface NSMutableDictionary<KeyType, ObjectType> : NSDictionary<KeyType, ObjectType>
+ (instancetype)dictionary;
- (void)setObject:(ObjectType)anObject forKey:(KeyType)aKey;
- (void)setObject:(ObjectType)obj forKeyedSubscript:(KeyType)key;
@end

@interface NSDate : NSObject
@property (class, readonly) NSDate *date;
@property (readonly) NSTimeInterval timeIntervalSince1970;
+ (NSTimeInterval)timeIntervalSinceReferenceDate;
@end

@interface NSNotification : NSObject
@property (readonly, copy) NSNotificationName name;
@property (readonly, nullable) id object;
@property (readonly, nullable) NSDictionary *userInfo;
@end

@interface NSNotificationCenter : NSObject
@property (class, readonly) NSNotificationCenter *defaultCenter;
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject;
- (void)removeObserver:(id)observer;
- (void)postNotificationName:(NSNotificationName)aName object:(nullable id)anObject;
@end

@interface NSRunLoop : NSObject
@property (class, readonly) NSRunLoop *mainRunLoop;
@property (class, readonly) NSRunLoop *currentRunLoop;
@end
SPRT_FOUNDATION_EXTERN NSRunLoopMode const NSRunLoopCommonModes;
SPRT_FOUNDATION_EXTERN NSRunLoopMode const NSDefaultRunLoopMode;

@interface NSAttributedString : NSObject
- (instancetype)initWithString:(NSString *)str;
@property (readonly, copy) NSString *string;
@property (readonly) NSUInteger length;
- (NSAttributedString *)attributedSubstringFromRange:(NSRange)range;
- (id)attribute:(NSAttributedStringKey)attrName atIndex:(NSUInteger)location effectiveRange:(nullable NSRangePointer)range;
@end

@interface NSMutableAttributedString : NSAttributedString
@end

@interface NSURL : NSObject
+ (nullable instancetype)fileURLWithPath:(NSString *)path;
+ (nullable instancetype)URLWithString:(NSString *)URLString;
@property (readonly, copy, nullable) NSString *path;
@property (readonly, copy, nullable) NSString *absoluteString;
@end

@interface NSError : NSObject
@property (readonly) NSInteger code;
@property (readonly, copy) NSString *domain;
@property (readonly, copy) NSString *localizedDescription;
@property (readonly, copy) NSDictionary *userInfo;
@end
extern NSErrorUserInfoKey const NSLocalizedDescriptionKey;

@interface NSUUID : NSObject <NSCopying>
- (instancetype)initWithUUIDBytes:(const unsigned char *)bytes;
- (void)getUUIDBytes:(unsigned char *)uuid;
@property (readonly, copy) NSString *UUIDString;
@end

@interface NSThread : NSObject
@property (class, readonly, strong) NSThread *currentThread;
@property (class, readonly) BOOL isMainThread;
@property (class, readonly, strong) NSThread *mainThread;
+ (void)sleepForTimeInterval:(NSTimeInterval)ti;
+ (void)detachNewThreadSelector:(SEL)selector toTarget:(id)target withObject:(nullable id)argument;
@end

/* --- key-value observing (NSObject additions + options) --- */
typedef NS_OPTIONS(NSUInteger, NSKeyValueObservingOptions) {
	NSKeyValueObservingOptionNew = 0x01, NSKeyValueObservingOptionOld = 0x02,
	NSKeyValueObservingOptionInitial = 0x04, NSKeyValueObservingOptionPrior = 0x08,
};
typedef NSString *NSKeyValueChangeKey;

@interface NSObject (SPRTFoundationKVO)
- (void)addObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath
		options:(NSKeyValueObservingOptions)options context:(nullable void *)context;
- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath;
- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath context:(nullable void *)context;
- (void)observeValueForKeyPath:(nullable NSString *)keyPath ofObject:(nullable id)object
		change:(nullable NSDictionary<NSKeyValueChangeKey, id> *)change context:(nullable void *)context;
@end

/* Runtime class lookup (real header: Foundation/NSObjCRuntime.h). */
SPRT_FOUNDATION_EXTERN Class _Nullable NSClassFromString(NSString *aClassName);

/* NSAppleScript (real header: Foundation/NSAppleScript.h) — the surface lldb's
 * Host.mm uses to open a new Terminal tab: init from source + fire-and-forget
 * execute. The result descriptor type is left as `id` (nobody inspects it). */
@interface NSAppleScript : NSObject
- (nullable instancetype)initWithSource:(NSString *)source;
- (id)executeAndReturnError:(NSDictionary * _Nullable * _Nullable)errorInfo;
@end

/* SDK parity: on macOS the Foundation umbrella pulls in CoreServices — that is
 * how lldb's Host.mm reaches the AE/LaunchServices declarations with only a
 * <Foundation/Foundation.h> include. */
#import <CoreServices/CoreServices.h>

#endif /* _SPRT_OPEN_FOUNDATION_FOUNDATION_H_ */
