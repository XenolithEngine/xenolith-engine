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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SORT_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SORT_H_

#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/__functional/invoke.h>
#include <sprt/cxx/__iterator/iterator_ops.h>
#include <sprt/cxx/__iterator/iterator_tags.h>
#include <sprt/cxx/__type_traits/queries.h>

/*
	!!!WIBECODED - NEED CHECKS, TESTS and styling
*/

namespace sprt {

template <typename Iter>
concept is_random_access = sprt::is_base_of_v< sprt::random_access_iterator_tag,
		typename sprt::iterator_traits<Iter>::iterator_category >;

namespace detail {

template <typename RandomIt, typename Comparator>
void heapify(RandomIt begin, RandomIt end,
		typename sprt::iterator_traits<RandomIt>::difference_type index, Comparator comp) {

	using Diff = typename sprt::iterator_traits<RandomIt>::difference_type;
	const auto n = static_cast<Diff>(sprt::distance(begin, end));

	while (true) {
		// Calculate children indices relative to 'begin'
		// Left child: 2 * index + 1
		// Right child: 2 * index + 2
		Diff left_child = 2 * index + 1;
		Diff right_child = 2 * index + 2;

		Diff largest_idx = index;

		// Compare with left child if it exists and satisfies the heap property
		if (left_child < n && !comp(*(begin + left_child), *(begin + largest_idx))) {
			largest_idx = static_cast<Diff>(left_child);
		}

		// Compare with right child if it exists and satisfies the heap property
		if (right_child < n && !comp(*(begin + right_child), *(begin + largest_idx))) {
			largest_idx = static_cast<Diff>(right_child);
		}

		// If the largest element is still the current node, the heap property holds
		if (largest_idx == index) {
			break;
		}

		// Swap and continue sifting down
		sprt::iter_swap(begin + index, begin + largest_idx);
		index = static_cast<Diff>(largest_idx);
	}
}
template <typename RandomIt, typename Comparator>
void heapsort_impl(RandomIt begin, RandomIt end, Comparator comp) {
	using Diff = typename sprt::iterator_traits<RandomIt>::difference_type;

	const auto n = static_cast<Diff>(sprt::distance(begin, end));
	if (n <= 1) {
		return;
	}

	// Step 1: Build a max-heap (or min-heap depending on comparator logic).
	// We start from the last non-leaf node and sift down.
	// Last non-leaf index is (n / 2) - 1.
	for (Diff i = n / 2 - 1; i >= 0; --i) { heapify(begin, end, i, comp); }

	// Step 2: Extract elements one by one from the root.
	// The root is the largest element (in a max-heap).
	// We swap it to the end of the unsorted portion and reduce the heap size.
	for (Diff i = n - 1; i > 0; --i) {
		sprt::iter_swap(begin, begin + i);

		// Restore heap property on the reduced heap range [begin, begin + i)
		heapify(begin, begin + i, 0, comp);
	}
}

// Helper: Insertion Sort for small ranges (optimized for cache locality)
template <class RandomIt, class Compare>
void insertion_sort(RandomIt first, RandomIt last, Compare comp) {
	if (first == last) {
		return;
	}

	for (auto i = first + 1; i != last; ++i) {
		auto key = sprt::move_unsafe(*i); // Copy or move depending on type cost
		auto j = i - 1;

		// Move elements greater than key one position ahead
		while (j >= first && comp(key, *j)) {
			*(j + 1) = sprt::move_unsafe(*j);
			--j;
		}
		*(j + 1) = sprt::move_unsafe(key);
	}
}

// Helper: Median of Three pivot selection
template <typename RandomIt, typename Compare>
RandomIt __get_median_pivot(RandomIt first, RandomIt middle, RandomIt last, Compare comp) {
	auto &a = *first;
	auto &b = *middle;
	auto &c = *last;

	// Note: The logic below handles strict weak ordering.
	// We check if 'b' is between 'a' and 'c'.
	if ((comp(a, b) && !comp(b, a)) && (comp(b, c) || (!comp(b, c) && !comp(c, b)))) {
		return middle;
	}
	if ((comp(b, c) && !comp(c, b)) && (comp(c, a) || (!comp(c, a) && !comp(a, c)))) {
		return last; // 'c' is the median
	}

	return first; // Default fallback to 'a'
}

/**
 * Partition function.
 * Returns the final position of the pivot.
 */
template <typename RandomIt, typename Compare>
RandomIt __partition(RandomIt first, RandomIt last, Compare comp) {
	if (first >= last - 1) {
		return first;
	}

	auto mid = first + sprt::distance(first, last) / 2;

	// Select pivot deterministically using the custom comparator
	auto pivot_iter = __get_median_pivot(first, mid, last - 1, comp);

	// Move pivot to the end for easier partitioning
	sprt::swap(*pivot_iter, *(last - 1));

	const auto &pivot_value = *(last - 1);

	RandomIt store_index = first;

	for (RandomIt iter = first; iter < last - 1; ++iter) {
		// Use the custom comparator to decide if element belongs on the left
		if (comp(*iter, pivot_value)) {
			sprt::swap(*store_index, *iter);
			++store_index;
		}
	}

	// Place pivot in correct position
	sprt::swap(*(last - 1), *store_index);

	return store_index;
}

template <class RandomIt, class Compare>
void quicksort_impl(RandomIt first, RandomIt last, size_t depth_limit, Compare comp) {
	constexpr size_t insertion_sort_threshold = 5;

	// Base cases
	// Base case: if the range has fewer than 2 elements, it is already sorted
	if (first >= last - 1) {
		return;
	}

	// Small range optimization: Insertion Sort
	if ((last - first) <= insertion_sort_threshold) {
		insertion_sort(first, last, comp);
		return;
	}

	// Depth check for Introsort switch to Heapsort
	if (depth_limit == 0) {
		heapsort_impl(first, last, comp);
		return;
	}

	// Partition the array and get the pivot's final position
	auto pivot_pos = __partition(first, last, comp);

	// Recursively sort elements before and after the partitioning point
	quicksort_impl(first, pivot_pos, depth_limit - 1, comp);
	quicksort_impl(pivot_pos + 1, last, depth_limit - 1, comp);
}

} // namespace detail

/**
 * Main public interface: std::sort equivalent.
 * 
 * @tparam Iter Must be a Random Access Iterator.
 * @param first The beginning of the range (inclusive).
 * @param last  The end of the range (exclusive).
 * @param comp  A binary predicate returning true if 'a' should go before 'b'.
 */
template <class Iter, class Compare>
requires is_random_access<Iter>
		&& sprt::is_invocable_r_v<bool, Compare, typename sprt::iterator_traits<Iter>::value_type,
				typename sprt::iterator_traits<Iter>::value_type>
void sort(Iter first, Iter last, Compare comp) {
	if (first >= last) {
		return;
	}

	//detail::heapsort_impl(first, last, comp);
	//detail::insertion_sort(first, last, comp);

	// Calculate max recursion depth: log2(N) * 2
	size_t n = static_cast<size_t>(last - first);
	size_t limit = 0;
	while ((size_t(1) << limit) < n) { ++limit; }
	limit *= 2;

	detail::quicksort_impl(first, last, limit, comp);
}

// Overload for default comparison (operator<)
template <class Iter>
requires is_random_access<Iter>
		&& sprt::is_invocable_r_v<bool, decltype(sprt::less<void>{}),
				typename sprt::iterator_traits<Iter>::value_type,
				typename sprt::iterator_traits<Iter>::value_type>
void sort(Iter first, Iter last) {
	sort(first, last, sprt::less<void>{});
}

} // namespace sprt

#endif
