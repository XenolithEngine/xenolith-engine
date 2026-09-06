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

#ifndef XENOLITH_FONT_XLFONTREMOTEWIRE_H_
#define XENOLITH_FONT_XLFONTREMOTEWIRE_H_

#include "XLFontController.h" // FontSpecializationVector + Value
#include "XLRemoteProtocol.h" // remote::WireWriter for the packed encoding

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

/* Packed binary encoding of a GlyphRequest (remote::FontCode::GlyphRequest), shared by the client
 * controller (encode) and the server endpoint (decode).
 *
 * It was a CBOR dict, with a note saying the plan called for a packed layout and that swapping it
 * later would not touch the control flow on either side. This is that swap, and the reason it was
 * worth making is the shape of the traffic rather than elegance: a batch carries the WHOLE required
 * character set of every face on every flush -- not a delta -- and that set only grows over the life
 * of the process (FontController::update explains why: the atlas is rebuilt from scratch each time).
 * In CBOR every codepoint was a separate data::Value, allocated on both sides, to carry what is
 *4 bytes.
 *
 * Layout, network byte order throughout:
 *
 *   [u32 depId][u32 faceCount]
 *   per face: [u64 contentHash]
 *             [i16 style][u16 weight][u16 stretch][i16 grade][u16 size][u32 density 16.16]
 *             [u16 faceId][u32 charCount][u32 char x charCount]
 *
 * `density` stays 16.16 fixed point, as it was in the dict -- the one thing the old encoding got
 * right on purpose, so that nothing here depends on floating-point support in the value layer.
 */

// The specialization, 16 bytes. Free-standing because both sides of the request need it and neither
// should be writing the field order out by hand twice.
inline void encodeFontSpec(remote::WireWriter &w, const FontSpecializationVector &s) {
	w.writeU16(uint16_t(s.fontStyle.get()));
	w.writeU16(uint16_t(s.fontWeight.get()));
	w.writeU16(uint16_t(s.fontStretch.get()));
	w.writeU16(uint16_t(s.fontGrade.get()));
	w.writeU16(uint16_t(s.fontSize.value));
	w.writeU16(0); // padding, so the fixed-point density lands 4-byte aligned in the record
	w.writeU32(uint32_t(s.density * 65'536.0f + 0.5f));
}

inline FontSpecializationVector decodeFontSpec(BytesViewNetwork &in) {
	FontSpecializationVector s;
	s.fontStyle = FontStyle(int16_t(in.readUnsigned16()));
	s.fontWeight = FontWeight(uint16_t(in.readUnsigned16()));
	s.fontStretch = FontStretch(uint16_t(in.readUnsigned16()));
	s.fontGrade = FontGrade(int16_t(in.readUnsigned16()));
	s.fontSize.value = uint16_t(in.readUnsigned16());
	in.readUnsigned16(); // padding
	s.density = float(in.readUnsigned32()) / 65'536.0f;
	return s;
}

// One face's worth of a request, as it travels.
struct GlyphRequestFace {
	uint64_t contentHash = 0;
	FontSpecializationVector spec;
	uint16_t faceId = 0;
	Vector<char32_t> chars;
};

inline void encodeGlyphRequest(Bytes &out, uint32_t depId, SpanView<GlyphRequestFace> faces) {
	out.clear();
	remote::WireWriter w(out);
	w.writeU32(depId);
	w.writeU32(uint32_t(faces.size()));
	for (auto &f : faces) {
		w.writeU64(f.contentHash);
		encodeFontSpec(w, f.spec);
		w.writeU16(f.faceId);
		w.writeU32(uint32_t(f.chars.size()));
		for (auto c : f.chars) { w.writeU32(uint32_t(c)); }
	}
}

// Returns false on a payload that does not parse. The readers zero-fill past the end, so a truncated
// message yields a face count it cannot satisfy and is caught by the size check rather than by
// reading past the buffer.
inline bool decodeGlyphRequest(BytesView payload, uint32_t &outDepId,
		Vector<GlyphRequestFace> &out) {
	BytesViewNetwork in(payload.data(), payload.size());
	if (in.size() < sizeof(uint32_t) * 2) {
		return false;
	}
	outDepId = in.readUnsigned32();
	auto faceCount = in.readUnsigned32();
	for (uint32_t i = 0; i < faceCount; ++i) {
		// 8 (hash) + 16 (spec) + 2 (faceId) + 4 (charCount)
		if (in.size() < 30) {
			return false;
		}
		GlyphRequestFace f;
		f.contentHash = in.readUnsigned64();
		f.spec = decodeFontSpec(in);
		f.faceId = uint16_t(in.readUnsigned16());
		auto charCount = in.readUnsigned32();
		if (in.size() / sizeof(uint32_t) < charCount) {
			return false;
		}
		f.chars.reserve(charCount);
		for (uint32_t c = 0; c < charCount; ++c) {
			f.chars.emplace_back(char32_t(in.readUnsigned32()));
		}
		out.emplace_back(sp::move(f));
	}
	return true;
}

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTREMOTEWIRE_H_ */
