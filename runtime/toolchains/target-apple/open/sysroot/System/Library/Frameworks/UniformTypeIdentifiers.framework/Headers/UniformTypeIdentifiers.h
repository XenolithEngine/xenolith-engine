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
	Minimal hand-written <UniformTypeIdentifiers/UniformTypeIdentifiers.h> for the
	Xcode-SDK-free macOS target (*-apple-macosx+open). Reconstructs ONLY the UTType
	surface the runtime uses (SPRuntimeFilesystem-macos.mm bundle-type probing):
	+typeWithIdentifier:, -conformsToType:, and the UTTypeApplicationBundle
	constant. Symbols resolve at run time from the real UniformTypeIdentifiers
	framework (.tbd stub + dynamic_lookup). See Foundation/Foundation.h for the
	rationale.
*/

#ifndef _SPRT_OPEN_UNIFORMTYPEIDENTIFIERS_H_
#define _SPRT_OPEN_UNIFORMTYPEIDENTIFIERS_H_

#import <Foundation/Foundation.h>

#if defined(__cplusplus)
#define SPRT_UT_EXTERN extern "C"
#else
#define SPRT_UT_EXTERN extern
#endif

@interface UTType : NSObject
+ (nullable UTType *)typeWithIdentifier:(NSString *)identifier;
+ (nullable UTType *)typeWithMIMEType:(NSString *)mimeType;
+ (nullable UTType *)typeWithFilenameExtension:(NSString *)filenameExtension;
- (BOOL)conformsToType:(UTType *)type;
@property (readonly) NSString *identifier;
@property (readonly, nullable) NSString *preferredMIMEType;
@property (readonly, nullable) NSString *preferredFilenameExtension;
@end

/* Bundle-of-an-application content type (UTType * const, C linkage). */
SPRT_UT_EXTERN UTType *const UTTypeApplicationBundle;

#endif /* _SPRT_OPEN_UNIFORMTYPEIDENTIFIERS_H_ */
