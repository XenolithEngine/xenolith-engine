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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_BACK_INSERT_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_BACK_INSERT_H_

#include <sprt/cxx/detail/iterator_tags.h>

namespace sprt {

template <typename _Container>
class back_insert_iterator : public iterator<output_iterator_tag, void, void, void, void> {
protected:
	_Container *container;

public:
	typedef output_iterator_tag iterator_category;
	typedef void value_type;
	typedef void difference_type;
	typedef void pointer;
	typedef void reference;
	typedef _Container container_type;

	explicit back_insert_iterator(_Container &__x) : container(sprt::addressof(__x)) { }
	back_insert_iterator &operator=(const typename _Container::value_type &__value) {
		container->push_back(__value);
		return *this;
	}
	back_insert_iterator &operator*() { return *this; }
	back_insert_iterator &operator++() { return *this; }
	back_insert_iterator operator++(int) { return *this; }

	_Container *__get_container() const { return container; }
};

template <typename _Container>
inline back_insert_iterator<_Container> back_inserter(_Container &__x) {
	return back_insert_iterator<_Container>(__x);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_BACK_INSERT_H_
