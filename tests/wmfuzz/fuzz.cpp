// libFuzzer on the client->server side of the remote protocol: the framing of the byte stream and
// every decoder a display server runs on what a client sends (TAU-P-Gpu-6e). One input stands for
// the bytes a hostile client could put on the wire; the requirement is a refusal, never a read out
// of bounds and never an allocation past the budgets the decoders enforce.

#include "XLRemoteProtocol.h"
#include "XLRemoteSerialize.h"
#include "XLRemotePeerInfo.h"

#include "SPData.h"
#include "SPLog.h"

#include <cstdio>
#include <cstdlib>
#include <wchar.h>

// The system's fuzzer archive is built against a newer glibc than the toolchain's sysroot; the C23
// spellings it calls for are plain wrappers there.
extern "C" {
unsigned long __isoc23_strtoul(const char *n, char **e, int b) { return ::strtoul(n, e, b); }
unsigned long long __isoc23_strtoull(const char *n, char **e, int b) { return ::strtoull(n, e, b); }
long __isoc23_wcstol(const wchar_t *n, wchar_t **e, int b) { return ::wcstol(n, e, b); }
unsigned long __isoc23_wcstoul(const wchar_t *n, wchar_t **e, int b) { return ::wcstoul(n, e, b); }
long long __isoc23_wcstoll(const wchar_t *n, wchar_t **e, int b) { return ::wcstoll(n, e, b); }
unsigned long long __isoc23_wcstoull(const wchar_t *n, wchar_t **e, int b) {
	return ::wcstoull(n, e, b);
}
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

static constexpr uint64_t UploadBudget = 64 * 1'024;

// The seeds: one framed message of every decoder, so the fuzzer starts from valid input.
static void writeSeeds(const char *dir) {
	auto framed = [](Domain d, uint8_t code, const Value &v) {
		Bytes payload = data::write<mem_std::Interface>(v, data::EncodeFormat::Cbor);
		Bytes out;
		encodeFrame(out, BytesView(), MessageType::Client, d, code, 7,
				BytesView(payload.data(), payload.size()));
		return out;
	};

	Vector<Pair<StringView, Bytes>> seeds;
	Value upload;
	upload.setInteger(1, "window");
	upload.setString("seed", "name");
	Value image;
	image.setString("k", "key");
	image.setInteger(2, "width");
	image.setInteger(2, "height");
	image.setBytes(Bytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}, "pixels");
	Value images(Value::Type::ARRAY);
	images.addValue(sp::move(image));
	upload.setValue(sp::move(images), "images");
	seeds.emplace_back("upload", framed(Domain::Window, toInt(WindowCode::CompileResource), upload));

	Vector<core::InputEventData> events;
	core::InputEventData ev;
	ev.event = core::InputEventName::KeyPressed;
	ev.key.keycode = core::InputKeyCode::A;
	events.emplace_back(ev);
	Bytes input;
	serializeInputEvents(input, 1, events);
	Bytes framedInput;
	encodeFrame(framedInput, BytesView(), MessageType::Client, Domain::Window,
			toInt(WindowCode::FrameInput), 9, BytesView(input.data(), input.size()));
	seeds.emplace_back("input", sp::move(framedInput));

	Value layers;
	layers.setInteger(1, "w");
	seeds.emplace_back("layers", framed(Domain::Window, toInt(WindowCode::UpdateLayers), layers));
	seeds.emplace_back("peerinfo", framed(Domain::Global, toInt(GlobalCode::ServerInfo),
			serializePeerInfo(PeerInfo::makeLocal())));

	for (auto &it : seeds) {
		String path = toString(dir, "/", it.first, ".seed");
		if (auto f = ::fopen(path.c_str(), "wb")) {
			::fwrite(it.second.data(), 1, it.second.size(), f);
			::fclose(f);
		}
	}
}

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
	// The engine's allocations go through its pool subsystem; an app's main sets it up, here the
	// harness does. The parse errors a hostile input produces would log per iteration.
	sprt::memory::pool::initialize();
	stappler::log::setLogFilterMask(stappler::log::Full);
	for (int i = 1; i < *argc; ++i) {
		auto arg = StringView((*argv)[i]);
		if (!arg.empty() && arg.front() != '-') {
			writeSeeds((*argv)[i]);
		}
	}
	return 0;
}

// What the server does with one decoded message: parse the payload the way its handler would.
// Every read goes through a const reference: the non-const getters assert on a missing key, and
// hostile input must not take the server down that way.
static void decodeMessage(const MessageHeader &h, BytesView payload) {
	switch (Domain(h.domain)) {
	case Domain::Global:
		if (auto v = remote::readMessagePayload<mem_std::Interface>(payload); !v.isNull()) {
			deserializePeerInfo(sp::move(v));
		}
		break;
	case Domain::Window:
		switch (WindowCode(h.code)) {
		case WindowCode::CompileResource: {
			const auto v = remote::readMessagePayload<mem_std::Interface>(payload);
			if (v.isNull()) {
				break;
			}
			uint64_t used = 0;
			QueueCodec::decodeResourceUpload(v, UploadBudget, used);
			break;
		}
		case WindowCode::CompileMaterials: {
			const auto v = remote::readMessagePayload<mem_std::Interface>(payload);
			uint64_t imageId = 0;
			for (const auto &it : v.getValue("mats").asArray()) {
				deserializeMaterialImage(it.getValue("imgs").getValue(0), imageId);
			}
			break;
		}
		case WindowCode::CreateWindow:
			deserializeWindowRequest(remote::readMessagePayload<mem_std::Interface>(payload));
			break;
		case WindowCode::WindowControl:
			deserializeFullscreenInfo(
					remote::readMessagePayload<mem_std::Interface>(payload).getValue("fs"));
			break;
		case WindowCode::TextInputControl: {
			const auto v = remote::readMessagePayload<mem_std::Interface>(payload);
			deserializeTextInputRequest(v.getValue("req"));
			deserializeTextInputCommand(v.getValue("cmd"));
			break;
		}
		case WindowCode::FrameInput: {
			uint64_t windowId = 0;
			Vector<core::InputEventData> events;
			deserializeInputEvents(payload, windowId, events);
			break;
		}
		case WindowCode::UpdateLayers: {
			uint64_t windowId = 0;
			sprt::window::Vector<sprt::window::WindowLayer> layers;
			deserializeWindowLayers(payload, windowId, layers);
			break;
		}
		default:
			break;
		}
		break;
	case Domain::Data:
	case Domain::Font:
		// The announce and the font requests are CBOR payloads too; the reader is the parser here.
		remote::readMessagePayload<mem_std::Interface>(payload);
		break;
	default:
		break;
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// The stream framing first: a violation must answer false, not trip.
	MessageReader reader;
	reader.append(BytesView(data, size), BytesView());
	reader.dispatch([](const MessageHeader &h, BytesView payload) {
		decodeMessage(h, payload);
		return true;
	});

	// The same bytes as each payload shape the server decodes raw.
	MessageHeader raw{};
	raw.msgtype = toInt(MessageType::Client);
	raw.domain = toInt(Domain::Window);
	raw.code = toInt(WindowCode::FrameInput);
	decodeMessage(raw, BytesView(data, size));
	raw.code = toInt(WindowCode::UpdateLayers);
	decodeMessage(raw, BytesView(data, size));
	return 0;
}

} // namespace STAPPLER_VERSIONIZED_NAMESPACE::xenolith::remote
