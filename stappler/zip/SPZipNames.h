/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_ZIP_SPZIPNAMES_H_
#define STAPPLER_ZIP_SPZIPNAMES_H_

#include "SPZipFormat.h"

namespace STAPPLER_VERSIONIZED stappler {

/* Entry names: what encoding they are in, and whether they are safe to hand to anything that
 * treats them as a path.
 *
 * A name in a ZIP is a bag of bytes with a hint, not a string. The hint is general purpose bit 11
 * ("these are UTF-8"), and it is missing far more often than the specification suggests - the stock
 * Linux `zip` writes UTF-8 names without setting it. Reading bit 11 literally therefore turns a
 * perfectly ordinary archive into mojibake, so the encoding is GUESSED, the same way libzip guesses
 * it, and the flag only raises the prior.
 */
enum class ZipNameEncoding {
	// every byte below 0x80: nothing to decode
	Ascii,

	// valid UTF-8 with bytes above 0x7F: already in the target encoding
	Utf8,

	// anything else: transcoded through the CP437 table
	Cp437,
};

/* Why a name was refused. `None` means it was accepted.
 *
 * These are reasons, not just a boolean, because the reason is the whole content of the test that
 * covers them - "rejected" alone would pass even if the wrong rule fired.
 */
enum class ZipNameRejection {
	None,

	// nothing to name
	Empty,

	// leading '/': an absolute POSIX path
	Absolute,

	// "C:" and friends: an absolute path in Windows spelling
	DriveLetter,

	// a backslash anywhere. On POSIX it is an ordinary filename character, so rewriting it to '/'
	// would silently make this a DIFFERENT file than the archive names; refusing says so out loud.
	Backslash,

	// '.', '..' or an empty segment - whatever sprt::filepath::validatePath refuses
	PathSegments,

	// a NUL byte inside the name, which truncates it in every C consumer downstream
	EmbeddedNul,
};

/* Strict UTF-8 validation.
 *
 * Deliberately stricter than libzip's, which checks only the shape of each sequence: this also
 * rejects overlong encodings and surrogates. An overlong '/' (0xC0 0xAF) is the reason - it would
 * sail past a sanitizer that scans raw bytes, and then be folded back into a separator by whoever
 * normalizes the name later. Bytes rejected here fall through to CP437, which is visibly wrong
 * rather than dangerous.
 */
SP_PUBLIC bool zipIsValidUtf8(BytesView);

// Decides how the raw bytes of a name should be read. `utf8Flag` is general purpose bit 11.
SP_PUBLIC ZipNameEncoding zipGuessEncoding(BytesView name, bool utf8Flag);

/* The name from the Info-ZIP Unicode Path extra field (0x7075), or an empty view when the entry has
 * none that can be trusted.
 *
 * The field wins over the header name when it is present - but only after its CRC32 is checked
 * against the header name it claims to replace. Without that check the field would be a way to make
 * an entry answer to one name while being another.
 */
SP_PUBLIC BytesView zipUnicodePathName(const ZipRawEntry &);

/* Emits the decoded, UTF-8 name of an entry. Call it once to measure and once to fill; it is
 * deterministic, and doing it this way keeps the decision about what the name IS in exactly one
 * place regardless of how the caller wants to store it.
 */
SP_PUBLIC void zipDecodeName(const ZipRawEntry &, const Callback<void(BytesView)> &);

/* Checks a decoded name for anything that makes it unsafe to use as a path.
 *
 * Takes bytes rather than a StringView on purpose: StringView's (pointer, length) constructor stops
 * at the first NUL, so a name carrying one would arrive here already truncated and its NUL would be
 * unfindable.
 *
 * A trailing '/' is NOT a rejection - that is the conventional spelling of a directory entry.
 */
SP_PUBLIC ZipNameRejection zipCheckName(BytesView name);

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIPNAMES_H_ */
