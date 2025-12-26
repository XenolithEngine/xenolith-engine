/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_DOCUMENT_SPDOCFORMAT_H_
#define CORE_DOCUMENT_SPDOCFORMAT_H_

#include "SPRef.h"
#include "SPFilesystem.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::document {

class Document;

using check_data_fn = bool (*)(memory::pool_t *, BytesView str, StringView ct);
using load_data_fn = Rc<Document> (*)(memory::pool_t *, BytesView, StringView ct);

using check_file_fn = bool (*)(memory::pool_t *, FileInfo path, StringView ct);
using load_file_fn = Rc<Document> (*)(memory::pool_t *, FileInfo path, StringView ct);

struct SP_PUBLIC Format {
	check_data_fn check_data;
	check_file_fn check_file;

	load_data_fn load_data;
	load_file_fn load_file;

	size_t priority = 0;

	static bool canOpenDocumnt(memory::pool_t *, FileInfo path, StringView ct = StringView());
	static bool canOpenDocumnt(memory::pool_t *, BytesView data, StringView ct = StringView());

	static Rc<Document> openDocument(memory::pool_t *, FileInfo path, StringView ct = StringView());
	static Rc<Document> openDocument(memory::pool_t *, BytesView data,
			StringView ct = StringView());

	Format(check_file_fn, load_file_fn, check_data_fn, load_data_fn, size_t = 0);
	~Format();

	Format(const Format &) = delete;
	Format(Format &&) = delete;
	Format &operator=(const Format &) = delete;
	Format &operator=(Format &&) = delete;
};

} // namespace stappler::document

#endif /* CORE_DOCUMENT_SPDOCFORMAT_H_ */
