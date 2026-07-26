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

#ifndef STAPPLER_GIT_SPGITOBJECT_H_
#define STAPPLER_GIT_SPGITOBJECT_H_

#include "SPGit.h"

// Git object model: the four canonical object types, the loose-object identity
// hash (git-SHA-1 over "<type> <size>\0" + content) and parsers for the tree /
// commit / tag object bodies used by checkout. All pure (no network / no fs).

namespace STAPPLER_VERSIONIZED stappler::git {

enum class ObjectType {
	Invalid,
	Commit, // 1
	Tree, // 2
	Blob, // 3
	Tag, // 4
};

// Ascii name used in the object header ("commit"/"tree"/"blob"/"tag").
SP_PUBLIC StringView getObjectTypeName(ObjectType);

struct SP_PUBLIC Object {
	ObjectType type = ObjectType::Invalid;
	Bytes data;
};

// A single entry of a tree object.
struct SP_PUBLIC TreeEntry {
	uint32_t mode = 0; // octal st_mode, e.g. 0100644, 0100755, 040000, 0120000, 0160000
	String name;
	Oid oid;

	bool isDir() const { return (mode & 0170000u) == 0040000u; }
	bool isSymlink() const { return (mode & 0170000u) == 0120000u; }
	bool isGitlink() const { return (mode & 0170000u) == 0160000u; }
	bool isExecutable() const { return !isDir() && (mode & 0000111u) != 0; }
};

// Compute the git object id of `content` as an object of `type`.
SP_PUBLIC Oid hashObject(ObjectType, BytesView content, ObjectFormat = ObjectFormat::Sha1);

// Parse a tree object body into its entries.
SP_PUBLIC Vector<TreeEntry> parseTree(BytesView, ObjectFormat = ObjectFormat::Sha1);

// Extract the root tree oid from a commit object body ("tree <hex>" line).
SP_PUBLIC Oid commitTree(BytesView, ObjectFormat = ObjectFormat::Sha1);

// Extract the referenced object oid from a tag object body ("object <hex>" line).
SP_PUBLIC Oid tagObject(BytesView, ObjectFormat = ObjectFormat::Sha1);

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGITOBJECT_H_ */
