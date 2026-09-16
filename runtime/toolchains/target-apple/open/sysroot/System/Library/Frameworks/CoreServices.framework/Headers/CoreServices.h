/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <CoreServices/CoreServices.h> for *-apple-macosx+open. Closed
 * umbrella (LaunchServices/AE); reconstructs ONLY the surface lldb's Host.mm
 * uses: the AEDesc/AEKeyDesc descriptor pair with AECreateDesc/AEDisposeDesc,
 * FSRef/ProcessSerialNumber, the LSLaunchFlags/LSRolesMask constants, the
 * LSApplicationParameters block, and the three LS launch calls. All types,
 * layouts and constant values match the 14.5 SDK (AEDataModel.h / LSOpen*.h);
 * symbols resolve from the baked CoreServices.tbd. */
#ifndef __SPRT_OPEN_CORESERVICES_H_
#define __SPRT_OPEN_CORESERVICES_H_

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>
/* SDK parity: the real CoreServices umbrella (via its OSServices sub-framework)
 * pulls the Security Authorization/AuthSession API — lldb's Host.mm relies on
 * that transitive include. Both headers come from apple-oss Security. */
#include <Security/Authorization.h>
#include <Security/AuthorizationDB.h>
#include <Security/AuthSession.h>

CF_EXTERN_C_BEGIN

/* --- AppleEvents descriptors (AEDataModel.h; 64-bit opaque-storage branch) --- */
typedef FourCharCode ResType;
typedef ResType      DescType;
typedef FourCharCode AEKeyword;

typedef struct OpaqueAEDataStorageType *AEDataStorageType;
typedef AEDataStorageType *AEDataStorage;

struct AEDesc {
	DescType      descriptorType;
	AEDataStorage dataHandle;
};
typedef struct AEDesc AEDesc;
typedef AEDesc        AppleEvent;

struct AEKeyDesc {
	AEKeyword descKey;
	AEDesc    descContent;
};
typedef struct AEKeyDesc AEKeyDesc;

enum {
	typeUTF8Text = 'utf8'
};

/* AERegistry.h */
enum {
	keyAEPosition = 'kpos'
};

extern OSErr AECreateDesc(DescType typeCode, const void *dataPtr,
		Size dataSize, AEDesc *result);
extern OSErr AEDisposeDesc(AEDesc *theAEDesc);

/* --- classic file handle (MacTypes provides ProcessSerialNumber) ------------ */
struct FSRef {
	UInt8 hidden[80];
};
typedef struct FSRef FSRef;

/* --- LaunchServices (LSInfo.h / LSOpenDeprecated.h) -------------------------- */
typedef OptionBits LSLaunchFlags;
enum {
	kLSLaunchDefaults         = 0x00000001,
	kLSLaunchDontAddToRecents = 0x00000100,
	kLSLaunchDontSwitch       = 0x00000200
};

typedef OptionBits LSRolesMask;
enum {
	kLSRolesNone   = 0x00000001,
	kLSRolesViewer = 0x00000002,
	kLSRolesEditor = 0x00000004,
	kLSRolesShell  = 0x00000008,
	kLSRolesAll    = (UInt32)0xFFFFFFFF
};

enum {
	kLSUnknownCreator = 0
};

typedef struct LSApplicationParameters {
	CFIndex         version;          /* must be zero */
	LSLaunchFlags   flags;
	const FSRef *   application;
	void *          asyncLaunchRefCon;
	CFDictionaryRef environment;
	CFArrayRef      argv;
	AppleEvent *    initialEvent;
} LSApplicationParameters;

extern OSStatus LSFindApplicationForInfo(OSType inCreator, CFStringRef inBundleID,
		CFStringRef inName, FSRef *outAppRef, CFURLRef *outAppURL);
extern OSStatus LSOpenURLsWithRole(CFArrayRef inURLs, LSRolesMask inRole,
		const AEKeyDesc *inAEParam, const LSApplicationParameters *inAppParams,
		ProcessSerialNumber *outPSNs, CFIndex inMaxPSNCount);
extern OSStatus LSOpenCFURLRef(CFURLRef inURL, CFURLRef *outLaunchedURL);

/* --- FSEvents (FSEvents.framework sub-umbrella) ------------------------------
 * The surface clang's DirectoryWatcher-mac.cpp uses; types, layouts and constant
 * values match the 14.5 SDK <FSEvents.h>. Needs <dispatch/dispatch.h> for
 * FSEventStreamSetDispatchQueue (the real umbrella exposes it the same way). */
#include <dispatch/dispatch.h>

typedef struct __FSEventStream *FSEventStreamRef;
typedef const struct __FSEventStream *ConstFSEventStreamRef;
typedef UInt32 FSEventStreamCreateFlags;
typedef UInt32 FSEventStreamEventFlags;
typedef UInt64 FSEventStreamEventId;

enum {
	kFSEventStreamCreateFlagNoDefer    = 0x00000002,
	kFSEventStreamCreateFlagFileEvents = 0x00000010
};

enum {
	kFSEventStreamEventFlagMustScanSubDirs = 0x00000001,
	kFSEventStreamEventFlagUserDropped     = 0x00000002,
	kFSEventStreamEventFlagKernelDropped   = 0x00000004,
	kFSEventStreamEventFlagItemCreated     = 0x00000100,
	kFSEventStreamEventFlagItemRemoved     = 0x00000200,
	kFSEventStreamEventFlagItemRenamed     = 0x00000800,
	kFSEventStreamEventFlagItemModified    = 0x00001000,
	kFSEventStreamEventFlagItemIsFile      = 0x00010000
};

enum {
	kFSEventStreamEventIdSinceNow = 0xFFFFFFFFFFFFFFFFULL
};

struct FSEventStreamContext {
	CFIndex version;
	void *info;
	CFAllocatorRetainCallBack retain;
	CFAllocatorReleaseCallBack release;
	CFAllocatorCopyDescriptionCallBack copyDescription;
};
typedef struct FSEventStreamContext FSEventStreamContext;

typedef void (*FSEventStreamCallback)(ConstFSEventStreamRef streamRef,
		void *clientCallBackInfo, size_t numEvents, void *eventPaths,
		const FSEventStreamEventFlags *eventFlags,
		const FSEventStreamEventId *eventIds);

extern FSEventStreamRef FSEventStreamCreate(CFAllocatorRef allocator,
		FSEventStreamCallback callback, FSEventStreamContext *context,
		CFArrayRef pathsToWatch, FSEventStreamEventId sinceWhen,
		CFTimeInterval latency, FSEventStreamCreateFlags flags);
extern void FSEventStreamSetDispatchQueue(FSEventStreamRef streamRef, dispatch_queue_t q);
extern Boolean FSEventStreamStart(FSEventStreamRef streamRef);
extern void FSEventStreamStop(FSEventStreamRef streamRef);
extern void FSEventStreamInvalidate(FSEventStreamRef streamRef);
extern void FSEventStreamRelease(FSEventStreamRef streamRef);

CF_EXTERN_C_END

#endif /* __SPRT_OPEN_CORESERVICES_H_ */
