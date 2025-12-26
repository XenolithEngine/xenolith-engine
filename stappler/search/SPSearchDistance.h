/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_SEARCH_SPSEARCHDISTANCE_H_
#define STAPPLER_SEARCH_SPSEARCHDISTANCE_H_

#include "SPStringView.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::search {

using namespace mem_pool;

// Edit (Levenshtein) Distance calculation and alignment, used by search index and transforms
// See: https://en.wikipedia.org/wiki/Levenshtein_distance

class SP_PUBLIC Distance : public memory::AllocPool {
public:
	enum class Value : uint8_t {
		Match,
		Insert,
		Delete,
		Replace
	};

	class Storage : public memory::AllocPool {
	public:
		struct Struct {
			uint8_t v1 : 2;
			uint8_t v2 : 2;
			uint8_t v3 : 2;
			uint8_t v4 : 2;

			void set(uint8_t idx, Value value);
			Value get(uint8_t idx) const;
		};

		struct Size {
			size_t size : sizeof(size_t) * 8 - 1;
			size_t vec : 1;
		};

		using Array = std::array<Struct, sizeof(Bytes) / sizeof(Struct)>;
		using Vec = Vector<Struct>;

		static Storage merge(const Storage &, const Storage &);

		Storage() noexcept;
		~Storage() noexcept;

		Storage(const Storage &) noexcept;
		Storage(Storage &&) noexcept;

		Storage &operator=(const Storage &) noexcept;
		Storage &operator=(Storage &&) noexcept;

		bool empty() const;
		size_t size() const;
		size_t capacity() const;

		void reserve(size_t);
		void emplace_back(Value);
		void reverse();

		Value at(size_t) const;
		void set(size_t, Value);

		void clear();

	protected:
		bool isVecStorage() const;
		bool isVecStorage(size_t) const;

		Size _size;
		union {
			Array _array;
			Vec _bytes;
		};
	};

	Distance() noexcept;
	Distance(const StringView &origin, const StringView &canonical, size_t maxDistance = maxOf<size_t>());

	Distance(const Distance &) noexcept;
	Distance(Distance &&) noexcept;

	Distance &operator=(const Distance &) noexcept;
	Distance &operator=(Distance &&) noexcept;

	bool empty() const;

	size_t size() const;

	// calculate position difference from canonical to original
	int32_t diff_original(size_t pos, bool forward = false) const;

	// calculate position difference from original to canonical
	int32_t diff_canonical(size_t pos, bool forward = false) const;

	size_t nmatch() const;

	memory::string info() const;

	Storage storage() const;

protected:
	uint32_t _distance = 0;
	Storage _storage;
};

}

#endif /* STAPPLER_SEARCH_SPSEARCHDISTANCE_H_ */
