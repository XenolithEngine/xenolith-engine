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

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

// CBOR (data::Value) encoding of a FontSpecializationVector for remote::Domain::Font messages, shared by
// the client controller (encode) and the server endpoint (decode). `density` is stored as 16.16 fixed
// point so we never depend on Value floating-point support.
//
// The encoding is CBOR rather than a packed binary layout; it can be swapped for one later without
// touching the control flow on either side.
inline Value encodeFontSpec(const FontSpecializationVector &s) {
	Value v;
	v.setInteger(int64_t(s.fontStyle.get()), "st");
	v.setInteger(int64_t(s.fontWeight.get()), "wt");
	v.setInteger(int64_t(s.fontStretch.get()), "sr");
	v.setInteger(int64_t(s.fontGrade.get()), "gr");
	v.setInteger(int64_t(s.fontSize.value), "sz");
	v.setInteger(int64_t(s.density * 65536.0f + 0.5f), "dn");
	return v;
}

inline FontSpecializationVector decodeFontSpec(const Value &v) {
	FontSpecializationVector s;
	s.fontStyle = FontStyle(int16_t(v.getInteger("st")));
	s.fontWeight = FontWeight(uint16_t(v.getInteger("wt")));
	s.fontStretch = FontStretch(uint16_t(v.getInteger("sr")));
	s.fontGrade = FontGrade(int16_t(v.getInteger("gr")));
	s.fontSize.value = uint16_t(v.getInteger("sz"));
	s.density = float(v.getInteger("dn")) / 65536.0f;
	return s;
}

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTREMOTEWIRE_H_ */
