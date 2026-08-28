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

#ifndef STAPPLER_ZIP_SPZIPREADER_H_
#define STAPPLER_ZIP_SPZIPREADER_H_

#include "SPZipCatalog.h"

namespace STAPPLER_VERSIONIZED stappler {

/* Getting an entry's bytes out of the archive.
 *
 * Free functions rather than a reader object: the catalog already owns the entries and the source
 * is supplied by whoever owns it, so a class here would hold no state and only add a name.
 *
 * Refusals carry a DISTINCT status per reason. That is not decoration - "the read failed" is a test
 * that passes even when the wrong rule fires, and several of these rules exist specifically to stop
 * hostile input.
 *
 *   Directory          Declined              a directory has no content of its own; not an error
 *   NameRejected       ErrorNotPermitted     the sanitizer refused the name (stage 3)
 *   Encrypted          ErrorNotImplemented   there is no decryption here
 *   UnsupportedMethod  ErrorNotSupported     compression method outside {Store, Deflate}
 *   zip bomb           ErrorBufferOverflow   declared size far beyond what the payload can hold
 *   malformed record   ErrorInvalidArguemnt  the structure does not parse
 *   bad data           ErrorNotRecoverable   it parses, but the bytes are wrong (CRC, broken stream)
 */

/* Where an entry's data begins in the source, in absolute coordinates.
 *
 * `prefix` is ZipCatalog::prefix() - the amount the archive is shifted inside the source, which
 * every stored offset is short by. Reads the local header (its variable-part lengths are what the
 * data offset is computed from) and validates that the whole compressed range is inside the source.
 * Allocates nothing.
 */
SP_PUBLIC Status zipLocateEntryData(ZipSource &, const ZipEntry &, uint64_t prefix,
		uint64_t &dataOffset);

/* Reads one entry and decompresses it into `out`, which is cleared first.
 *
 * The whole entry is materialized because that is the contract the module's public readFile()
 * offers - one callback with the complete buffer, which is what the EPUB reader parses XML from.
 * The INPUT is streamed in chunks, so a compressed copy of the entry is never held alongside the
 * uncompressed one.
 *
 * An entry of length zero is a success with an empty result. libzip's wrapper refuses those on a
 * `stat.size == 0` early-out, which makes a legitimately empty file in an archive unreadable; that
 * behaviour is a defect and is deliberately not carried over.
 */
template <typename Interface>
SP_PUBLIC Status zipReadEntry(ZipSource &, const ZipEntry &, uint64_t prefix,
		typename Interface::BytesType &out);

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIPREADER_H_ */
