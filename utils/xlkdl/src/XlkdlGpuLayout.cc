/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XlkdlGpu.h"

namespace STAPPLER_VERSIONIZED stappler::xlkdl::gpu {

namespace {

uint32_t alignUp(uint32_t value, uint32_t align) { return (value + align - 1) / align * align; }

// A field starts where the one before it ends: a gap std430 leaves is one C++ might not, so it is
// an error rather than a guess. The record ends at its std430 size - a multiple of its largest
// alignment - with uint fields, which C++ then lays out the same.
bool layoutRecord(const Schema &schema, Record &record, Diagnostics &diag) {
	uint32_t offset = 0;
	uint32_t align = 4;
	const Field *prev = nullptr;
	for (auto &field : record.fields) {
		auto start = alignUp(offset, field.type->align);
		if (start != offset) {
			diag.error(schema.file, field.location,
					mem_std::toString("'", field.name, "' of '", record.name, "' starts at ", start,
							" in std430, leaving ", start - offset, " bytes after '", prev->name,
							"' that C++ would not: reorder the fields or fill the gap"));
			return false;
		}
		field.offset = start;
		offset = start + field.type->size * (field.count ? field.count : 1);
		align = sprt::max(align, field.type->align);
		prev = &field;
	}
	record.align = align;
	record.size = alignUp(offset, align);

	auto padding = (record.size - offset) / 4;
	for (uint32_t i = 0; i < padding; ++i) {
		Field f;
		f.name = padding == 1 ? String("padding") : mem_std::toString("padding", i);
		f.type = findType("uint");
		f.offset = offset + i * 4;
		f.padding = true;
		for (auto &it : record.fields) {
			if (it.name == f.name) {
				diag.error(schema.file, it.location,
						mem_std::toString("'", record.name, "' ends with ", padding,
								" uint of padding, and '", f.name, "' is its name"));
				return false;
			}
		}
		record.fields.emplace_back(sprt::move(f));
	}
	return true;
}

} // namespace

bool layoutSchema(Schema &schema, Diagnostics &diag) {
	bool ok = true;
	for (auto &record : schema.records) { ok = layoutRecord(schema, record, diag) && ok; }
	return ok;
}

} // namespace stappler::xlkdl::gpu
