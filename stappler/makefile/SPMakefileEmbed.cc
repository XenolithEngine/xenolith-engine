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

#include "SPMakefileEmbed.h"

#include "SPDataEncode.h"
#include "SPFilesystem.h"
#include "SPFilesystemEmbedded.h"
#include "SPMemory.h"

#include <sprt/cxx/algorithm>

namespace STAPPLER_VERSIONIZED stappler::makefile {

using filesystem::embedded::comparePath;

struct EmbedEntry {
	mem_std::String path; // bundle-relative
	bool isDir = false;
	bool compressed = false;
	uint64_t offset = 0;
	uint64_t size = 0;
	uint64_t originalSize = 0;
};

// Writes a path as the contents of a C string literal. Everything outside printable ASCII —
// and the two characters that would end or escape the literal — becomes a 3-digit octal escape.
// Octal is used rather than \x because a \x escape is greedy and would swallow a following hex
// digit of the path itself.
static void writeEscapedPath(const Callback<void(StringView)> &out, StringView path) {
	char buf[8];
	for (auto c : path) {
		auto u = uint8_t(c);
		if (u == '"') {
			out << "\\\"";
		} else if (u == '\\') {
			out << "\\\\";
		} else if (u >= 0x20 && u < 0x7F) {
			buf[0] = char(u);
			out << StringView(buf, 1);
		} else {
			buf[0] = '\\';
			buf[1] = char('0' + ((u >> 6) & 0x7));
			buf[2] = char('0' + ((u >> 3) & 0x7));
			buf[3] = char('0' + (u & 0x7));
			out << StringView(buf, 4);
		}
	}
}

// C identifier suffix for the bundle's symbols
static mem_std::String makeIdentifier(StringView name) {
	mem_std::String ret;
	ret.reserve(name.size());
	for (auto c : name) {
		auto u = uint8_t(c);
		auto ok = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9')
				|| u == '_';
		ret.push_back(ok ? char(u) : '_');
	}
	return ret;
}

Status generateEmbeddedSource(const EmbedConfig &cfg, const Callback<void(StringView)> &err) {
	if (cfg.output.empty() || cfg.bundleName.empty() || cfg.sourceDir.empty()) {
		err << "embed: usage: $(EMBED) <out.cpp> <bundle-name> <src-dir> [compress]";
		return Status::ErrorInvalidArguemnt;
	}

	FileInfo dirInfo(cfg.sourceDir, FileCategory::Custom);

	filesystem::Stat dirStat;
	if (!filesystem::stat(dirInfo, dirStat) || dirStat.type != FileType::Dir) {
		err << "embed: not a directory: " << cfg.sourceDir;
		return Status::ErrorNotFound;
	}

	// ftw reports paths relative to the walked root already merged onto it; strip the root back
	// off to get the bundle-relative path. The root itself arrives as the bare source dir.
	mem_std::Vector<EmbedEntry> entries;
	auto walked = filesystem::ftw(dirInfo, [&](const FileInfo &info, FileType type) {
		auto rel = info.path;
		if (rel.size() <= cfg.sourceDir.size()) {
			return true; // the root of the walk itself
		}
		rel += cfg.sourceDir.size();
		rel.skipChars<StringView::Chars<'/'>>();
		if (rel.empty()) {
			return true;
		}

		if (type == FileType::Dir) {
			entries.emplace_back(EmbedEntry{rel.str<mem_std::Interface>(), true});
		} else if (type == FileType::File) {
			entries.emplace_back(EmbedEntry{rel.str<mem_std::Interface>(), false});
		}
		return true;
	}, -1, true);

	if (!walked) {
		err << "embed: failed to enumerate: " << cfg.sourceDir;
		return Status::ErrorNotFound;
	}

	// Entry order is part of the format — see comparePath in SPFilesystemEmbedded.h
	sprt::sort(entries.begin(), entries.end(), [](const EmbedEntry &l, const EmbedEntry &r) {
		return comparePath(l.path, r.path) < 0;
	});

	mem_std::Bytes blob;
	uint64_t newestMtime = 0;

	for (auto &it : entries) {
		// keep the merged path alive: FileInfo only stores a view of it
		auto fullPath = filepath::merge<mem_std::Interface>(cfg.sourceDir, it.path);
		FileInfo fileInfo(StringView(fullPath), FileCategory::Custom);

		filesystem::Stat st;
		if (filesystem::stat(fileInfo, st)) {
			// mtime is content-derived, not "now", so that regenerating an unchanged bundle
			// produces an identical file (and matches what embedfs.sh computes)
			auto secs = st.mtime.toMicros() / 1'000'000;
			if (secs > newestMtime) {
				newestMtime = secs;
			}
		}

		if (it.isDir) {
			continue;
		}

		auto content = filesystem::readIntoMemory<mem_std::Interface>(fileInfo);
		if (content.empty() && st.size != 0) {
			err << "embed: failed to read: " << fileInfo.path;
			return Status::ErrorNotFound;
		}

		it.originalSize = content.size();
		it.offset = blob.size();

		if (cfg.compress && !content.empty()) {
			// `conditional` leaves the data alone when compressing would not actually shrink it,
			// which is exactly the per-entry Compressed flag the format carries
			auto packed = data::compress<mem_std::Interface>(content.data(), content.size(),
					data::EncodeFormat::LZ4HCCompression, true);
			if (!packed.empty()) {
				it.compressed = true;
				it.size = packed.size();
				blob.insert(blob.end(), packed.begin(), packed.end());
				continue;
			}
		}

		it.size = content.size();
		blob.insert(blob.end(), content.begin(), content.end());
	}

	auto ident = makeIdentifier(cfg.bundleName);

	mem_std::StringStream out;

	out << "/**\n Autogenerated by BundleFS. Do not edit.\n\n Bundle: " << cfg.bundleName
		<< "\n **/\n///@ SP_EXCLUDE\n\n";
	out << "#include \"SPFilesystemEmbedded.h\"\n#include \"SPSharedModule.h\"\n\n";
	out << "namespace STAPPLER_VERSIONIZED stappler::filesystem::embedded {\n\n";

	out << "alignas(16) static const uint8_t s_data_" << ident << "[] = {\n";
	if (blob.empty()) {
		out << "\t0x00,\n";
	} else {
		char hex[8] = {'0', 'x', 0, 0, ',', 0, 0, 0};
		auto digits = "0123456789abcdef";
		for (size_t i = 0; i < blob.size(); ++i) {
			if (i % 16 == 0) {
				if (i > 0) {
					out << "\n";
				}
				out << "\t";
			} else {
				out << " ";
			}
			hex[2] = digits[(blob[i] >> 4) & 0xF];
			hex[3] = digits[blob[i] & 0xF];
			out << StringView(hex, 5);
		}
		out << "\n";
	}
	out << "};\n\n";

	out << "static const Entry s_entries_" << ident << "[] = {\n";
	if (entries.empty()) {
		out << "\t{ \"\", 0, EntryFlags::Dir, 0, 0, 0 },\n";
	} else {
		for (auto &it : entries) {
			out << "\t{ \"";
			writeEscapedPath([&](StringView str) { out << str; }, it.path);
			out << "\", " << it.path.size() << ", ";
			if (it.isDir) {
				out << "EntryFlags::Dir, 0, 0, 0 },\n";
			} else {
				out << (it.compressed ? "EntryFlags::Compressed" : "EntryFlags::None") << ", "
					<< it.offset << ", " << it.size << ", " << it.originalSize << " },\n";
			}
		}
	}
	out << "};\n\n";

	out << "static const Bundle s_bundle_" << ident << " = {\n";
	out << "\tBundleVersion,\n";
	out << "\t\"" << cfg.bundleName << "\",\n";
	out << "\ts_entries_" << ident << ",\n";
	out << "\t" << entries.size() << ",\n";
	out << "\ts_data_" << ident << ",\n";
	out << "\t" << blob.size() << ",\n";
	out << "\t" << (newestMtime * 1'000'000) << ",\n";
	out << "};\n\n";

	out << "SP_USED static SharedExtension s_extension_" << ident << "(BundleModuleName, \""
		<< cfg.bundleName << "\",\n\t\t&s_bundle_" << ident << ");\n\n";
	out << "} // namespace stappler::filesystem::embedded\n";

	auto str = out.str();
	if (!filesystem::write(FileInfo(cfg.output, FileCategory::Custom),
				reinterpret_cast<const uint8_t *>(str.data()), str.size())) {
		err << "embed: failed to write: " << cfg.output;
		return Status::ErrorInvalidArguemnt;
	}

	return Status::Ok;
}

} // namespace stappler::makefile
