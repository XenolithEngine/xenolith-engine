#include "SPCommon.h"
#include "SPData.h"
#include "XLRemoteSerialize.h"
#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

void performResourceUploadTests() {
	using stappler::test::check;
	Value request;
	request.setString("test-resource", "name");
	Value image;
	image.setString("image", "key");
	image.setInteger(2, "width");
	image.setInteger(1, "height");
	const uint8_t pixels[] = {1, 2, 3, 255, 4, 5, 6, 128};
	image.setBytes(BytesView(pixels, sizeof(pixels)), "pixels");
	request.emplace("images").addValue(image);
	uint64_t used = 100;
	auto resource = QueueCodec::decodeResourceUpload(request, 8, used);
	uint8_t actual[8] = {};
	check(resource && used == 8 && resource->getImage("image")
					&& resource->getImage("image")->writeData(actual, sizeof(actual)) == 8
					&& ::memcmp(actual, pixels, sizeof(actual)) == 0,
			"resource upload: exact RGBA data and a completely consumed budget");
	auto refused = [&](Value value, uint64_t budget, const char *name) {
		used = 100;
		check(!QueueCodec::decodeResourceUpload(value, budget, used) && used == 0, name);
	};
	refused(request, 7, "resource upload: rejects a byte over the remaining budget");
	for (auto width : {int64_t(0), int64_t(-1), int64_t(4'097), int64_t(1) << 32}) {
		auto bad = request;
		bad.getValue("images").getValue(size_t(0)).setInteger(width, "width");
		refused(sp::move(bad), 16 * 1'024 * 1'024, "resource upload: invalid or wrapping width");
	}
	auto bad = request;
	bad.getValue("images").getValue(size_t(0)).setInteger(4'097, "height");
	refused(bad, 16 * 1'024 * 1'024, "resource upload: excessive height");
	bad = request;
	bad.getValue("images").getValue(size_t(0)).setBytes(BytesView(pixels, 7), "pixels");
	refused(bad, 8, "resource upload: truncated pixel data");
	bad = request;
	bad.getValue("images").addValue(image);
	refused(bad, 16, "resource upload: duplicate image keys");
	bad = request;
	bad.setString("wrong type", "images");
	refused(bad, 8, "resource upload: array required");
	bad = request;
	bad.setString(String(129, 'x'), "name");
	refused(bad, 8, "resource upload: bounded resource names");
	bad = request;
	bad.getValue("images").getValue(size_t(0)).setString(String(129, 'x'), "key");
	refused(bad, 8, "resource upload: bounded image keys");
	bad = request;
	for (unsigned i = 0; i < 16; ++i) { bad.getValue("images").addValue(image); }
	refused(bad, 16 * 1'024 * 1'024, "resource upload: bounded image count");
	bad = request;
	auto &large = bad.getValue("images").getValue(size_t(0));
	large.setInteger(4'096, "width");
	large.setInteger(4'096, "height");
	refused(bad, 64 * 1'024 * 1'024, "resource upload: request ceiling precedes pixel allocation");
}

} // namespace stappler::xenolith::remote
