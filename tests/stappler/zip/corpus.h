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

#ifndef TESTS_STAPPLER_ZIP_CORPUS_H_
#define TESTS_STAPPLER_ZIP_CORPUS_H_

#include "SPCommon.h"
#include "SPMemInterface.h"

namespace STAPPLER_VERSIONIZED stappler::test::zip {

using Bytes = memory::StandardInterface::BytesType;
using String = memory::StandardInterface::StringType;

template <typename T>
using Vector = memory::StandardInterface::VectorType<T>;

// One entry the archive is expected to expose, spelled the way ZipArchive reports it (not the way
// the ZIP file spells it - name transcoding is precisely one of the things under test).
struct EntryExpect {
	String name;
	Bytes content;

	// readFile() must succeed and hand back exactly `content`. False means the entry is expected to
	// be listed but not readable (a directory entry, or one the zip-bomb guard rejects).
	bool readable = true;
};

// A single archive plus what reading it must produce.
//
// `entries` empty means "not yet characterized": the test prints the observed listing and asserts
// nothing about it. That is the deliberate starting state for the cases whose libzip behaviour
// cannot be predicted from the specification (CP437 names, 0x7075 precedence, path traversal,
// encrypted entries) - the first run reveals the truth and it gets written down here, rather than
// the reader later being bent to match a guess.
struct Case {
	String name;
	Bytes archive;
	Vector<EntryExpect> entries;

	// the ZipArchive constructor must produce a usable handle
	bool openable = true;

	// entries are known and must match exactly; false = characterization-only (see above)
	bool characterized = true;
};

// Builds every archive in the corpus from literals, byte by byte. Nothing is read from disk: a
// binary fixture in git could not be reviewed, and a generated one documents its own structure.
Vector<Case> buildCorpus();

// Raw-deflate a buffer (zlib with -MAX_WBITS), the wire form a ZIP method-8 entry carries.
Bytes deflateRaw(BytesView);

} // namespace stappler::test::zip

#endif /* TESTS_STAPPLER_ZIP_CORPUS_H_ */
