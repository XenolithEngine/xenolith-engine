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

/* The typed wire format for input events and window layers.
 *
 * These two used to travel as a raw dump of their C++ layout, which is what forced the ABI tag to
 * gate every session: between builds that disagreed, one process read the other's padding as a
 * keycode. This file is what replaces that guarantee.
 *
 * The round-trip cases are the ordinary half. The load-bearing half is the GOLDEN VECTORS: a byte
 * string written out by hand from the format's documentation, which the codec must produce exactly
 * and parse exactly. A round trip only proves the codec agrees with itself -- and a codec that
 * silently picked up this build's field order would pass every round trip while being unreadable
 * anywhere else. The literal is the only assertion here that a second implementation, on a second
 * machine, would have to satisfy too.
 */

#include "SPCommon.h"

#include "XLRemoteSerialize.h"
#include "XLRemoteProtocol.h"
#include "XLFontRemoteWire.h"

#include "../tests.h"

#include <sprt/runtime/utils/base16.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

static Bytes fromHex(StringView hex) { return base16::decode<Interface>(hex); }

static String toHex(BytesView b) { return base16::encode<Interface>(b); }

} // namespace

void performWireTests() {
	sprt::cout << "--- remote wire format ---\n";

	{
		/* GOLDEN VECTOR, key event. Written by hand from the layout note in XLRemoteSerialize.h:
		
		   header : 0102030405060708  windowId
		            0028              recordSize = 40
		            0000              reserved
		            00000001          count
		   record : 11223344          id
		            00000007          event = KeyPressed (the 8th name; the VALUE is the contract)
		            00000000          input.button = None
		            80000000          input.modifiers = ValueTrue (bit 31)
		            3F800000          input.x =  1.0f
		            C0000000          input.y = -2.0f
		            0041              key.keycode  = 65
		            0001              key.compose  = Composed
		            0000FFE1          key.keysym
		            0000042F          key.keychar = U+042F CYRILLIC CAPITAL YA
		            00000000          padding to 16 bytes of variant */
		const StringView golden(
				// header
				"0102030405060708" "0028" "0000" "00000001"
				// id, event, button, modifiers, x, y
				"11223344" "00000007" "00000000" "80000000" "3f800000" "c0000000"
				// keycode, compose, keysym, keychar, padding
				"0041" "0001" "0000ffe1" "0000042f" "00000000");

		core::InputEventData e;
		e.id = 0x1122'3344;
		e.event = core::InputEventName::KeyPressed;
		e.input.button = core::InputMouseButton::None;
		e.input.modifiers = core::InputModifier::ValueTrue;
		e.input.x = 1.0f;
		e.input.y = -2.0f;
		e.key.keycode = core::InputKeyCode(65);
		e.key.compose = core::InputKeyComposeState::Composed;
		e.key.keysym = 0x0000'FFE1;
		e.key.keychar = char32_t(0x042F);

		Bytes encoded;
		serializeInputEvents(encoded, 0x0102'0304'0506'0708, makeSpanView(&e, 1));
		checkEq(StringView(toHex(BytesView(encoded.data(), encoded.size()))), golden,
				"wire: a key event encodes to the documented bytes");

		uint64_t windowId = 0;
		Vector<core::InputEventData> decoded;
		check(deserializeInputEvents(BytesView(fromHex(golden)), windowId, decoded)
						&& decoded.size() == 1,
				"wire: the documented bytes decode");
		if (decoded.size() == 1) {
			auto &d = decoded[0];
			checkEq(uint64_t(windowId), uint64_t(0x0102'0304'0506'0708), "wire: windowId survives");
			check(d.id == e.id && d.event == e.event, "wire: id and event survive");
			check(d.input.modifiers == core::InputModifier::ValueTrue,
					"wire: the modifier's high bit survives, unsign-extended");
			check(d.input.x == 1.0f && d.input.y == -2.0f, "wire: pointer coordinates survive");
			check(d.key.keycode == e.key.keycode && d.key.compose == e.key.compose
							&& d.key.keysym == e.key.keysym && d.key.keychar == e.key.keychar,
					"wire: the key variant survives");
		}
	}

	{
		/* GOLDEN VECTOR, window-state event. The variant here is two 64-bit masks, and it is the
		   only one that fills all 16 bytes -- worth pinning separately because a codec that wrote
		   the widest variant as 32-bit halves would still pass the key case above. */
		const StringView golden(
				// header
				"0000000000000009" "0028" "0000" "00000001"
				// id, event, button, modifiers, x, y
				"00000001" "0000000b" "00000000" "00000000" "00000000" "00000000"
				// state, changes
				"0000000000000001" "0000000000000003");

		core::InputEventData e;
		e.id = 1;
		e.event = core::InputEventName::WindowState;
		e.input.button = core::InputMouseButton::None;
		e.input.modifiers = core::InputModifier::None;
		e.input.x = 0.0f;
		e.input.y = 0.0f;
		e.window.state = sprt::window::WindowState(0x0000'0001);
		e.window.changes = sprt::window::WindowState(0x0000'0003);

		Bytes encoded;
		serializeInputEvents(encoded, 9, makeSpanView(&e, 1));
		checkEq(StringView(toHex(BytesView(encoded.data(), encoded.size()))), golden,
				"wire: a window-state event encodes to the documented bytes");
	}

	{
		// NaN is a VALUE here, not an absence: input.x defaults to NaN and hasLocation() is defined
		// by isnan(), so a codec that normalised it -- or rounded it through a double -- would turn
		// "no location" into "the origin".
		core::InputEventData e;
		e.event = core::InputEventName::MouseMove;
		check(!e.hasLocation(), "wire: a fresh event has no location (precondition)");

		Bytes encoded;
		serializeInputEvents(encoded, 1, makeSpanView(&e, 1));
		uint64_t windowId = 0;
		Vector<core::InputEventData> decoded;
		deserializeInputEvents(BytesView(encoded.data(), encoded.size()), windowId, decoded);
		check(decoded.size() == 1 && !decoded[0].hasLocation(),
				"wire: NaN coordinates survive as NaN, so hasLocation() still says no");
		check(decoded.size() == 1 && decoded[0].id == maxOf<uint32_t>(),
				"wire: the id sentinel survives rather than becoming a number");
	}

	{
		// An event this build has no name for. It must not index InputEventInfo -- the engine's own
		// accessors bounds-check for exactly this reason, because `event` comes from platform
		// back-ends. Off the wire it deserves at least as much suspicion.
		core::InputEventData e;
		e.id = 7;
		e.event = core::InputEventName(9'999);

		Bytes encoded;
		serializeInputEvents(encoded, 1, makeSpanView(&e, 1));
		uint64_t windowId = 0;
		Vector<core::InputEventData> decoded;
		check(deserializeInputEvents(BytesView(encoded.data(), encoded.size()), windowId, decoded),
				"wire: a batch with an unknown event still parses");
		check(decoded.size() == 1 && decoded[0].id == 7 && !decoded[0].hasInput(),
				"wire: an out-of-range event decodes without indexing the event table");
	}

	{
		// A batch from a NEWER peer: same fields first, then more. The whole point of putting the
		// record size on the wire is that this reads rather than desynchronises -- the reader takes
		// the prefix it knows and steps by the declared size to the next record.
		Bytes wide;
		WireWriter w(wide);
		w.writeU64(42);
		w.writeU16(kInputEventRecordSize + 8); // a peer that appended two fields
		w.writeU16(0);
		w.writeU32(2);
		for (uint32_t i = 0; i < 2; ++i) {
			w.writeU32(100 + i); // id
			w.writeU32(toInt(core::InputEventName::Move));
			w.writeU32(0);
			w.writeU32(0);
			w.writeFloatBits(3.0f);
			w.writeFloatBits(4.0f);
			w.writeFloatBits(0.0f);
			w.writeFloatBits(0.0f);
			w.writeFloatBits(2.0f); // density
			w.writeZero(4);
			w.writeU64(0xDEAD'BEEF'0000'0000); // the field we do not know about
		}

		uint64_t windowId = 0;
		Vector<core::InputEventData> decoded;
		check(deserializeInputEvents(BytesView(wide.data(), wide.size()), windowId, decoded),
				"wire: a longer record from a newer peer is accepted");
		check(decoded.size() == 2 && decoded[0].id == 100 && decoded[1].id == 101,
				"wire: and BOTH records are found, so the reader stepped by the declared size");
		check(decoded.size() == 2 && decoded[1].input.x == 3.0f && decoded[1].point.density == 2.0f,
				"wire: the known prefix of the second record is intact");
	}

	{
		// The reverse: a peer whose record is shorter than what we read. There is no prefix to take
		// -- the prefix is the whole of it -- so this is refused rather than half-decoded.
		Bytes narrow;
		WireWriter w(narrow);
		w.writeU64(1);
		w.writeU16(kInputEventRecordSize - 4);
		w.writeU16(0);
		w.writeU32(1);
		w.writeZero(kInputEventRecordSize - 4);

		uint64_t windowId = 0;
		Vector<core::InputEventData> decoded;
		check(!deserializeInputEvents(BytesView(narrow.data(), narrow.size()), windowId, decoded),
				"wire: a record too short to hold the fields is refused");
		check(decoded.empty(), "wire: and nothing half-decoded is handed back");
	}

	{
		// Truncation and garbage.
		uint64_t windowId = 0;
		Vector<core::InputEventData> decoded;
		check(!deserializeInputEvents(BytesView(), windowId, decoded),
				"wire: an empty payload is not a batch");

		Bytes claimsMore;
		WireWriter w(claimsMore);
		w.writeU64(1);
		w.writeU16(kInputEventRecordSize);
		w.writeU16(0);
		w.writeU32(4); // says four
		w.writeZero(kInputEventRecordSize); // carries one
		check(!deserializeInputEvents(BytesView(claimsMore.data(), claimsMore.size()), windowId,
					  decoded),
				"wire: a header claiming more records than it carries is refused");
	}

	{
		/* GOLDEN VECTOR, WindowLayer.
		
		   header : 00000000000000AB  windowId
		            0018              recordSize = 24
		            0000              reserved
		            00000001          count
		   record : 41200000          rect.origin.x =  10.0f
		            41A00000          rect.origin.y =  20.0f
		            42C80000          rect.size.width  = 100.0f
		            42480000          rect.size.height =  50.0f
		            03                cursor
		            000000            padding -- explicit, where the dump shipped whatever was there
		            00000011          flags */
		const StringView golden(
				// header
				"00000000000000ab" "0018" "0000" "00000001"
				// x, y, width, height
				"41200000" "41a00000" "42c80000" "42480000"
				// cursor, padding, flags
				"03" "000000" "00000011");

		sprt::window::WindowLayer l;
		l.rect.origin.x = 10.0f;
		l.rect.origin.y = 20.0f;
		l.rect.size.width = 100.0f;
		l.rect.size.height = 50.0f;
		l.cursor = sprt::window::WindowCursor(3);
		l.flags = sprt::window::WindowLayerFlags(0x11);

		Bytes encoded;
		serializeWindowLayers(encoded, 0xAB, makeSpanView(&l, 1));
		checkEq(StringView(toHex(BytesView(encoded.data(), encoded.size()))), golden,
				"wire: a window layer encodes to the documented bytes");

		uint64_t windowId = 0;
		sprt::window::Vector<sprt::window::WindowLayer> decoded;
		check(deserializeWindowLayers(BytesView(encoded.data(), encoded.size()), windowId, decoded)
						&& decoded.size() == 1,
				"wire: the layer decodes");
		if (decoded.size() == 1) {
			check(decoded[0] == l, "wire: the layer round-trips field for field");
			checkEq(uint64_t(windowId), uint64_t(0xAB), "wire: the layer batch keeps its window id");
		}
	}

	{
		/* GOLDEN VECTOR, GlyphRequest (M6.4). Was a CBOR dict in which every codepoint was its own
		   data::Value -- allocated on both sides to carry four bytes -- while a batch carries the
		   whole required character set of every face, every flush.
		
		   header : 0000002a          depId = 42
		            00000001          faceCount = 1
		   face   : 1122334455667788  contentHash
		            0002              style
		            0190              weight  = 400
		            0064              stretch = 100
		            0000              grade
		            0018              size = 24
		            0000              padding
		            00020000          density = 2.0 in 16.16
		            0007              faceId
		            00000002          charCount
		            00000041          'A'
		            0000042f          U+042F */
		const StringView golden("0000002a" "00000001"
								"1122334455667788"
								"0002" "0190" "0064" "0000" "0018" "0000" "00020000"
								"0007" "00000002" "00000041" "0000042f");

		font::GlyphRequestFace f;
		f.contentHash = 0x1122'3344'5566'7788;
		f.spec.fontStyle = font::FontStyle(int16_t(2));
		f.spec.fontWeight = font::FontWeight(uint16_t(400));
		f.spec.fontStretch = font::FontStretch(uint16_t(100));
		f.spec.fontGrade = font::FontGrade(int16_t(0));
		f.spec.fontSize.value = 24;
		f.spec.density = 2.0f;
		f.faceId = 7;
		f.chars.emplace_back(U'A');
		f.chars.emplace_back(char32_t(0x042F));

		Bytes encoded;
		font::encodeGlyphRequest(encoded, 42, makeSpanView(&f, 1));
		checkEq(StringView(toHex(BytesView(encoded.data(), encoded.size()))), golden,
				"wire: a glyph request encodes to the documented bytes");

		uint32_t depId = 0;
		Vector<font::GlyphRequestFace> decoded;
		check(font::decodeGlyphRequest(BytesView(fromHex(golden)), depId, decoded)
						&& decoded.size() == 1,
				"wire: the glyph request decodes");
		if (decoded.size() == 1) {
			checkEq(uint64_t(depId), uint64_t(42), "wire: the gating dependency id survives");
			check(decoded[0].contentHash == f.contentHash && decoded[0].faceId == f.faceId,
					"wire: the face is identified the same way");
			check(decoded[0].spec.density == 2.0f && decoded[0].spec.fontSize.value == 24,
					"wire: the specialization survives, density included");
			check(decoded[0].chars.size() == 2 && decoded[0].chars[1] == char32_t(0x042F),
					"wire: a non-ASCII codepoint survives");
		}

		// Truncation must be refused rather than read past the end.
		auto shortened = fromHex(golden);
		shortened.resize(shortened.size() - 4);
		uint32_t d2 = 0;
		Vector<font::GlyphRequestFace> partial;
		check(!font::decodeGlyphRequest(BytesView(shortened), d2, partial),
				"wire: a truncated glyph request is refused");
	}

	{
		/* The enum VALUES are part of the wire contract, and this is where that is stated.
		
		   Typing the format does not make the numbers mean the same thing on both sides: `event`
		   rides as an integer, so inserting a name into the MIDDLE of InputEventName changes what
		   the same bytes mean without changing a single field. That was the one thing the M3 ABI tag
		   caught for a real reason, and with the tag no longer gating a session this is what
		   replaces it -- readable, and it names the value that moved instead of reporting that some
		   opaque hash differs. */
		checkEq(uint64_t(toInt(core::InputEventName::None)), uint64_t(0), "wire: None == 0");
		checkEq(uint64_t(toInt(core::InputEventName::Begin)), uint64_t(1), "wire: Begin == 1");
		checkEq(uint64_t(toInt(core::InputEventName::Move)), uint64_t(2), "wire: Move == 2");
		checkEq(uint64_t(toInt(core::InputEventName::End)), uint64_t(3), "wire: End == 3");
		checkEq(uint64_t(toInt(core::InputEventName::Cancel)), uint64_t(4), "wire: Cancel == 4");
		checkEq(uint64_t(toInt(core::InputEventName::MouseMove)), uint64_t(5),
				"wire: MouseMove == 5");
		checkEq(uint64_t(toInt(core::InputEventName::Scroll)), uint64_t(6), "wire: Scroll == 6");
		checkEq(uint64_t(toInt(core::InputEventName::KeyPressed)), uint64_t(7),
				"wire: KeyPressed == 7");
		checkEq(uint64_t(toInt(core::InputEventName::KeyRepeated)), uint64_t(8),
				"wire: KeyRepeated == 8");
		checkEq(uint64_t(toInt(core::InputEventName::KeyReleased)), uint64_t(9),
				"wire: KeyReleased == 9");
		checkEq(uint64_t(toInt(core::InputEventName::KeyCanceled)), uint64_t(10),
				"wire: KeyCanceled == 10");
		checkEq(uint64_t(toInt(core::InputEventName::WindowState)), uint64_t(11),
				"wire: WindowState == 11");
		checkEq(uint64_t(toInt(core::InputEventName::Max)), uint64_t(12),
				"wire: InputEventName has exactly the names above");

		// The ranges the old tag hashed. A value appended past these is additive; one inserted below
		// them is not, and the assertions above are what says so.
		checkEq(uint64_t(toInt(core::InputMouseButton::Max)), uint64_t(20),
				"wire: InputMouseButton range is pinned");
		checkEq(uint64_t(toInt(core::InputKeyCode::Max)), uint64_t(141),
				"wire: InputKeyCode range is pinned");
		checkEq(uint64_t(toInt(sprt::window::WindowCursor::Max)), uint64_t(40),
				"wire: WindowCursor range is pinned");
	}
}

} // namespace stappler::xenolith::remote
