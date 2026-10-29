// libFuzzer on the client->server side of the remote protocol: the framing of the byte stream and
// every decoder a display server runs on what a client sends (TAU-P-Gpu-6e). One input stands for
// the bytes a hostile client could put on the wire; the requirement is a refusal, never a read out
// of bounds and never an allocation past the budgets the decoders enforce.

#include "XLRemoteProtocol.h"
#include "XLRemoteSerialize.h"
#include "XLRemotePeerInfo.h"

#include "SPData.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

static constexpr uint64_t UploadBudget = 64 * 1'024;

// The seeds: one framed message of every decoder, so the fuzzer starts from valid input.
static void writeSeeds(const char *dir) {
	auto framed = [](Domain d, uint8_t code, const Value &v) {
		WireWriter w;
		v.encode<CborFormat>(w);
		Bytes out;
		FrameInfo info;
		info.size = uint32_t(w.data().size());
		info.domain = toInt(d);
		info.code = code;
		info.type = toInt(MessageType::Request);
		info.flags = 0;
		info.serial = 7;
		writeFrame(out, info, w.data(), BytesView());
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
	MessageHeader h{};
	h.domain = toInt(Domain::Window);
	h.code = toInt(WindowCode::FrameInput);
	h.type = toInt(MessageType::Request);
	h.serial = 9;
	h.size = uint32_t(input.size());
	Bytes framedInput;
	writeFrame(framedInput, FrameInfo{h.size, h.domain, h.code, h.type, 0, h.serial}, input, BytesView());
	seeds.emplace_back("input", sp::move(framedInput));

	Value layers;
	layers.setInteger(1, "w");
	Bytes layerBytes;
	data::write<CborFormat>(layerBytes, layers, Value());
	seeds.emplace_back("layers", framed(Domain::Window, toInt(WindowCode::UpdateLayers), layers));
	seeds.emplace_back("peerinfo", framed(Domain::Global, toInt(GlobalCode::ServerInfo),
			serializePeerInfo(PeerInfo::makeLocal())));

	for (auto &it : seeds) {
		auto path = toString(dir, "/", it.first, ".seed");
		if (!filesystem::exists(path)) {
			filesystem::write(path, BytesView(it.second));
		}
	}
}

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
	for (int i = 1; i < *argc; ++i) {
		auto arg = StringView((*argv)[i]);
		if (!arg.empty() && arg.front() != '-' && filesystem::exists(arg) && filesystem::isDirectory(arg)) {
			writeSeeds((*argv)[i]);
		}
	}
	return 0;
}

// What the server does with one decoded message: parse the payload the way its handler would.
static void decodeMessage(const MessageHeader &h, BytesView payload) {
	switch (Domain(h.domain)) {
	case Domain::Global:
		if (auto v = data::read<mem_std::Interface>(payload); v) {
			deserializePeerInfo(sp::move(v));
		}
		break;
	case Domain::Window:
		switch (WindowCode(h.code)) {
		case WindowCode::CompileResource:
		case WindowCode::CompileMaterials:
		case WindowCode::CreateWindow:
		case WindowCode::WindowControl:
		case WindowCode::TextInputControl: {
			auto v = data::read<mem_std::Interface>(payload);
			if (!v) {
				break;
			}
			if (WindowCode(h.code) == WindowCode::CompileResource) {
				uint64_t used = 0;
				auto resource = QueueCodec::decodeResourceUpload(v, UploadBudget, used);
				if (resource) {
					// The mirror must read back without touching anything past its pixels.
					QueueCodec::encodeResource(*resource, ObjectRegistry());
				}
			} else if (WindowCode(h.code) == WindowCode::CreateWindow) {
				deserializeWindowRequest(v);
			} else if (WindowCode(h.code) == WindowCode::WindowControl) {
				deserializeFullscreenInfo(v.getValue("fs"));
			} else if (WindowCode(h.code) == WindowCode::TextInputControl) {
				deserializeTextInputRequest(v.getValue("req"));
				deserializeTextInputCommand(v.getValue("cmd"));
			} else {
				uint64_t imageId = 0;
				for (auto &it : v.getValue("mats").asArray()) {
					deserializeMaterialImage(it.getValue("imgs").getValue(0), imageId);
				}
			}
			break;
		}
		case WindowCode::FrameInput: {
			uint64_t windowId = 0;
			deserializeInputEvents(payload, windowId);
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
		data::read<mem_std::Interface>(payload);
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
	raw.domain = toInt(Domain::Window);
	raw.code = toInt(WindowCode::FrameInput);
	decodeMessage(raw, BytesView(data, size));
	raw.code = toInt(WindowCode::UpdateLayers);
	decodeMessage(raw, BytesView(data, size));
	return 0;
}

} // namespace STAPPLER_VERSIONIED_NAMESPACE::xenolith::remote
