#include <sprt/runtime/stream.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/platform.h>
namespace sprt {
// A rough measure of the Latin fast path, which is the only reason it exists.
// Comparing these same words with the fast path switched off takes about five
// times as long.
void performCollationBench() {
	static const char *words[] = {"apple","Banana","cherry","dátil","elderberry","fig","grape",
		"honeydew","imbe","jackfruit","kiwi","lemon","mango","nectarine","orange","papaya",
		"quince","raspberry","strawberry","tangerine","ugli","vanilla","watermelon","ximenia"};
	constexpr int n = 24;
	constexpr int rounds = 2000;
	auto t0 = platform::clock(platform::ClockType::Monotonic);
	int64_t acc = 0;
	for (int r = 0; r < rounds; ++r) {
		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < n; ++j) {
				acc += unicode::collate(StringView(words[i]), StringView(words[j]), StringView());
			}
		}
	}
	auto dt = platform::clock(platform::ClockType::Monotonic) - t0;
	sprt::cout << "collate: " << (rounds * n * n) << " comparisons in " << dt
			   << " us (" << (double(dt) * 1000.0 / (rounds * n * n)) << " ns each), acc="
			   << acc << "\n";
}
}
