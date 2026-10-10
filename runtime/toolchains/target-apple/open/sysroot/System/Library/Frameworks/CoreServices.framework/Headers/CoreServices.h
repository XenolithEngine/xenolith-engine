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
#include <__sprt_abi/CoreServices/CoreServices.h>

CF_EXTERN_C_BEGIN

extern OSErr AECreateDesc(DescType typeCode, const void *dataPtr,
		Size dataSize, AEDesc *result);
extern OSErr AEDisposeDesc(AEDesc *theAEDesc);

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
