/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_CORE_MEMORY_DETAIL_SPMEMRBTREE_H_
#define STAPPLER_CORE_MEMORY_DETAIL_SPMEMRBTREE_H_

#include "SPMemAlloc.h"
#include "SPMemStorageNode.h"

#define SP_MEM_RBTREE_DEBUG 0

namespace STAPPLER_VERSIONIZED stappler::memory::detail {

enum RbTreeNodeColor : uintptr_t {
	Red = 0,
	Black = 1
};

template <typename Value>
using RbTreeNodeStorage = Storage<Value>;

struct SP_PUBLIC RbTreeNodeBase : public AllocPool {
	using Flag = RbTreeNodeFlag<sizeof(uintptr_t)>;

	RbTreeNodeBase *parent = nullptr;
	RbTreeNodeBase *left = nullptr;
	RbTreeNodeBase *right = nullptr;
	Flag flag;

	RbTreeNodeBase() noexcept : flag(Flag{0, 0, 0, 0}) { }
	RbTreeNodeBase(RbTreeNodeColor c) noexcept : flag(Flag{uintptr_t(c ? 1 : 0), 0, 0, 0}) { }

	inline void setColor(RbTreeNodeColor c) { flag.color = c ? 1 : 0; }
	inline RbTreeNodeColor getColor() const { return RbTreeNodeColor(flag.color); }

	inline void setPrealloc(bool v) { flag.prealloc = v ? 1 : 0; }
	inline bool isPrealloc() const { return flag.prealloc != 0; }

	inline void setSize(uintptr_t s) { flag.size = s; }
	inline uintptr_t getSize() const { return flag.size; }

	inline void setIndex(uintptr_t s) { flag.index = s; }
	inline uintptr_t getIndex() const { return flag.index; }

	static inline RbTreeNodeBase *min(RbTreeNodeBase *x) {
		while (x->left != 0) { x = x->left; }
		return x;
	}

	static inline const RbTreeNodeBase *min(const RbTreeNodeBase *x) {
		while (x->left != 0) { x = x->left; }
		return x;
	}

	static inline RbTreeNodeBase *max(RbTreeNodeBase *x) {
		while (x->right != 0) { x = x->right; }
		return x;
	}

	static inline const RbTreeNodeBase *max(const RbTreeNodeBase *x) {
		while (x->right != 0) { x = x->right; }
		return x;
	}

	static RbTreeNodeBase *increment(RbTreeNodeBase *c);
	static const RbTreeNodeBase *increment(const RbTreeNodeBase *c);

	static RbTreeNodeBase *decrement(RbTreeNodeBase *c);
	static const RbTreeNodeBase *decrement(const RbTreeNodeBase *c);

	// replace node in it's place in tree with new one
	static RbTreeNodeBase *replace(RbTreeNodeBase *old, RbTreeNodeBase *n);

	static void insert(RbTreeNodeBase *head, RbTreeNodeBase *n);
	static void remove(RbTreeNodeBase *head, RbTreeNodeBase *n);
};

template <typename Value>
struct RbTreeNode : public RbTreeNodeBase {
	RbTreeNodeStorage<Value> value;

	static inline Value *cast(RbTreeNodeBase *n) {
		return static_cast<RbTreeNode<Value> *>(n)->value.ptr();
	}
	static inline const Value *cast(const RbTreeNodeBase *n) {
		return static_cast<const RbTreeNode<Value> *>(n)->value.ptr();
	}

	// Where to store next node when this node is in preserved list
	auto getNextStorage() const { return static_cast<RbTreeNode *>(parent); }
	auto getNextStoragePtr() { return reinterpret_cast<RbTreeNode **>(&parent); }
};

template <typename Value>
struct RbTreeIterator {
	using iterator_category = std::bidirectional_iterator_tag;

	using node_type = RbTreeNode<Value>;
	using value_type = Value;
	using reference = Value &;
	using pointer = Value *;

	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using self = RbTreeIterator<Value>;
	using node_ptr = RbTreeNodeBase *;
	using link_ptr = RbTreeNode<Value> *;

	RbTreeIterator() noexcept : _node() { }

	explicit RbTreeIterator(node_ptr x) noexcept : _node(x) { }

	reference operator*() const noexcept { return *node_type::cast(_node); }
	pointer operator->() const noexcept { return node_type::cast(_node); }

	self &operator++() noexcept {
		_node = node_type::increment(_node);
		return *this;
	}
	self operator++(int) noexcept {
		self ret = *this;
		_node = node_type::increment(_node);
		return ret;
	}

	self &operator--() noexcept {
		_node = node_type::decrement(_node);
		return *this;
	}
	self operator--(int) noexcept {
		self ret = *this;
		_node = node_type::decrement(_node);
		return ret;
	}

	bool operator==(const self &other) const noexcept { return _node == other._node; }
	bool operator!=(const self &other) const noexcept { return _node != other._node; }

	node_ptr _node;
};

template <typename Value>
struct RbTreeConstIterator {
	using iterator_category = std::bidirectional_iterator_tag;

	using node_type = RbTreeNode<Value>;
	using value_type = Value;
	using reference = const Value &;
	using pointer = const Value *;

	using iterator = RbTreeIterator<Value>;

	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using self = RbTreeConstIterator<Value>;
	using node_ptr = const RbTreeNodeBase *;
	using link_ptr = const RbTreeNode<Value> *;

	RbTreeConstIterator() noexcept : _node() { }

	explicit RbTreeConstIterator(node_ptr x) noexcept : _node(x) { }

	RbTreeConstIterator(const iterator &it) noexcept : _node(it._node) { }

	iterator constcast() const noexcept {
		return iterator(const_cast<typename iterator::node_ptr>(_node));
	}

	reference operator*() const noexcept { return *node_type::cast(_node); }
	pointer operator->() const noexcept { return node_type::cast(_node); }

	self &operator++() noexcept {
		_node = node_type::increment(_node);
		return *this;
	}
	self operator++(int) noexcept {
		self tmp = *this;
		_node = node_type::increment(_node);
		return tmp;
	}

	self &operator--() noexcept {
		_node = node_type::decrement(_node);
		return *this;
	}
	self operator--(int) noexcept {
		self tmp = *this;
		_node = node_type::decrement(_node);
		return tmp;
	}

	bool operator==(const self &x) const noexcept { return _node == x._node; }
	bool operator!=(const self &x) const noexcept { return _node != x._node; }

	node_ptr _node;
};


template <typename Value>
inline bool operator==(const RbTreeIterator<Value> &l,
		const RbTreeConstIterator<Value> &r) noexcept {
	return l._node == r._node;
}

template <typename Value>
inline bool operator!=(const RbTreeIterator<Value> &l,
		const RbTreeConstIterator<Value> &r) noexcept {
	return l._node != r._node;
}


template <typename Key, typename Value>
struct RbTreeKeyExtractor;

template <typename Key>
struct RbTreeKeyExtractor<Key, Key> {
	static inline const Key &extract(const Key &k) noexcept { return k; }

	template <typename A, typename... Args>
	static inline void construct(A &alloc, RbTreeNode<Key> *node, const Key &key,
			Args &&...args) noexcept {
		alloc.construct(node->value.ptr(), key);
	}

	template <typename A, typename... Args>
	static inline void construct(A &alloc, RbTreeNode<Key> *node, Key &&key,
			Args &&...args) noexcept {
		alloc.construct(node->value.ptr(), sp::move_unsafe(key));
	}
};

template <typename Key, typename Value>
struct RbTreeKeyExtractor<Key, Pair<Key, Value>> {
	static inline const Key &extract(const Pair<Key, Value> &k) noexcept { return k.first; }

	template <typename A, typename... Args>
	static inline void construct(A &alloc, RbTreeNode<Pair<Key, Value>> *node, const Key &k,
			Args &&...args) noexcept {
		alloc.construct(node->value.ptr(), std::piecewise_construct, std::forward_as_tuple(k),
				std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename A, typename... Args>
	static inline void construct(A &alloc, RbTreeNode<Pair<Key, Value>> *node, Key &&k,
			Args &&...args) noexcept {
		alloc.construct(node->value.ptr(), std::piecewise_construct,
				std::forward_as_tuple(sp::move_unsafe(k)),
				std::forward_as_tuple(std::forward<Args>(args)...));
	}
};

template <typename Key, typename Value>
struct RbTreeKeyExtractor<Key, Pair<const Key, Value>> {
	static inline const Key &extract(const Pair<const Key, Value> &k) noexcept { return k.first; }

	template <typename A, typename... Args>
	static inline void construct(A &alloc, RbTreeNode<Pair<const Key, Value>> *node, const Key &k,
			Args &&...args) noexcept {
		alloc.construct(node->value.ptr(), std::piecewise_construct, std::forward_as_tuple(k),
				std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename A, typename... Args>
	static inline void construct(A &alloc, RbTreeNode<Pair<const Key, Value>> *node, Key &&k,
			Args &&...args) noexcept {
		alloc.construct(node->value.ptr(), std::piecewise_construct,
				std::forward_as_tuple(sp::move_unsafe(k)),
				std::forward_as_tuple(std::forward<Args>(args)...));
	}
};

namespace impl {

template <typename T, typename... P>
struct dependent_type {
	using type = T;
};

template <typename A, typename... B>
using void_type = typename dependent_type<void, A, B...>::type;

template <typename DummyVoid, template <typename...> typename A, typename... B>
struct is_detected : std::false_type { };

template <template <typename...> typename A, typename... B>
struct is_detected<void_type<A<B...>>, A, B...> : std::true_type { };

template <template <typename...> typename A, typename... B>
inline constexpr bool is_detected_v = impl::is_detected<void, A, B...>::value;

} // namespace impl

template <typename T>
using RbTreeDetectTransparent = typename T::is_transparent;

template <typename Key, typename Value, typename Comp = std::less<>>
class RbTree : public AllocPool {
public:
	using value_type = Value;
	using node_type = RbTreeNode<Value>;
	using node_ptr = RbTreeNode<Value> *;
	using base_type = RbTreeNodeBase *;
	using const_node_ptr = const node_type *;

	using node_allocator_type = Allocator<node_type>;
	using value_allocator_type = Allocator<value_type>;
	using comparator_type = Comp;

	using iterator = RbTreeIterator<Value>;
	using const_iterator = RbTreeConstIterator<Value>;

	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	using allocator_helper = NodeBlockAllocatorHelper<node_type>;

public:
	RbTree(const Comp &comp = Comp(),
			const value_allocator_type &alloc = value_allocator_type()) noexcept
	: _header(RbTreeNodeColor::Black), _comp(comp), _allocator(alloc), _size(0) { }

	RbTree(const RbTree &other, const value_allocator_type &alloc = value_allocator_type()) noexcept
	: _header(RbTreeNodeColor::Black), _comp(other._comp), _allocator(alloc), _size(0) {
		clone(other);
	}

	RbTree(RbTree &&other, const value_allocator_type &alloc = value_allocator_type()) noexcept
	: _header(RbTreeNodeColor::Black), _comp(other._comp), _allocator(alloc), _size(0) {
		if (other.get_allocator() == _allocator) {
			_header = other._header;
			_size = other._size;
			_comp = sp::move_unsafe(other._comp);
			if (_header.left != nullptr) {
				_header.left->parent = &_header;
			}
			other._header = RbTreeNodeBase(RbTreeNodeColor::Black);
			other._size = 0;
		} else {
			clone(other);
		}
	}

	RbTree &operator=(const RbTree &other) noexcept {
		clone(other);
		return *this;
	}

	RbTree &operator=(RbTree &&other) noexcept {
		if (other.get_allocator() == _allocator) {
			clear();
			_header = other._header;
			_size = other._size;
			_comp = sp::move_unsafe(other._comp);
			if (_header.left != nullptr) {
				_header.left->parent = &_header;
			}
			other._header = RbTreeNodeBase(RbTreeNodeColor::Black);
			other._size = 0;
		} else {
			clone(other);
		}
		return *this;
	}

	~RbTree() noexcept {
		if (_size > 0) {
			clear();
		}
		if (_header.flag.size > 0 && _free) {
			allocator_helper::template release_blocks<false>(_allocator, &_free,
					_header.flag.index);
		}
	}

	const value_allocator_type &get_allocator() const noexcept { return _allocator; }

	template <typename... Args>
	Pair<iterator, bool> emplace(Args &&...args) {
		auto ret = insertNodeUnique(std::forward<Args>(args)...);
		return pair(iterator(ret.first), ret.second);
	}

	template <typename... Args>
	iterator emplace_hint(const_iterator hint, Args &&...args) {
		return iterator(insertNodeUniqueHint(hint, std::forward<Args>(args)...));
	}

	template <typename K, typename... Args>
	Pair<iterator, bool> try_emplace(K &&k, Args &&...args) {
		auto ret = tryInsertNodeUnique(std::forward<K>(k), std::forward<Args>(args)...);
		return pair(iterator(ret.first), ret.second);
	}

	template <typename K, typename... Args>
	iterator try_emplace(const_iterator hint, K &&k, Args &&...args) {
		return iterator(
				tryInsertNodeUniqueHint(hint, std::forward<K>(k), std::forward<Args>(args)...));
	}

	template <typename K, class M>
	Pair<iterator, bool> insert_or_assign(K &&k, M &&m) {
		auto ret = tryAssignNodeUnique(std::forward<K>(k), std::forward<M>(m));
		return pair(iterator(ret.first), ret.second);
	}

	template <typename K, class M>
	iterator insert_or_assign(const_iterator hint, K &&k, M &&m) {
		return iterator(tryAssignNodeUniqueHint(hint, std::forward<K>(k), std::forward<M>(m)));
	}

	iterator erase(const_iterator pos) {
		if (pos._node != &_header) {
			auto next = RbTreeNodeBase::increment(pos.constcast()._node);
			deleteNode(const_cast<RbTreeNodeBase *>(pos._node));
			return iterator(next);
		}
		return pos.constcast();
	}

	iterator erase(const_iterator first, const_iterator last) {
		for (auto it = first; it != last; it++) {
			deleteNode(const_cast<RbTreeNodeBase *>(it._node));
		}
		return last;
	}

	size_t erase_unique(const Key &key) {
		auto node = find_impl(key);
		if (node) {
			deleteNode(node);
			return 1;
		}
		return 0;
	}

	iterator begin() noexcept { return iterator(_header.left ? left() : &_header); }
	iterator end() noexcept { return iterator(&_header); }

	const_iterator begin() const noexcept {
		return const_iterator(_header.left ? left() : &_header);
	}
	const_iterator end() const noexcept { return const_iterator(&_header); }

	reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

	const_iterator cbegin() const noexcept {
		return const_iterator(_header.left ? left() : &_header);
	}
	const_iterator cend() const noexcept { return const_iterator(&_header); }

	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

	void clear() noexcept {
		if (_header.left) {
			clear_visit(static_cast<RbTreeNode<Value> *>(_header.left));
		}
		_header.left = nullptr;
		_header.right = nullptr;
		_header.parent = nullptr;
		_size = 0;
	}

	void shrink_to_fit() noexcept {
		auto nFreed = allocator_helper::template release_blocks<true>(get_allocator(), &_free,
				_header.flag.index);
		_header.flag.size -= nFreed;
	}

	size_t capacity() const noexcept { return _size + _header.flag.size; }

	size_t size() const noexcept { return _size; }

	bool empty() const noexcept { return _header.left == nullptr; }

	void set_memory_persistent(bool value) noexcept { _header.flag.prealloc = value ? 1 : 0; }

	bool memory_persistent() const noexcept { return _header.flag.prealloc; }

	void swap(RbTree &other) noexcept {
		std::swap(_header, other._header);
		std::swap(_allocator, other._allocator);
		std::swap(_size, other._size);
		std::swap(_comp, other._comp);
		std::swap(_free, other._free);
	}

	template < class K >
	iterator find(const K &x) {
		auto ptr = find_impl(x);
		return (ptr) ? iterator(ptr) : end();
	}

	template < class K >
	const_iterator find(const K &x) const {
		auto ptr = find_impl(x);
		return (ptr) ? const_iterator(ptr) : end();
	}

	template < class K >
	iterator lower_bound(const K &x) {
		auto ptr = lower_bound_ptr(x);
		return (ptr) ? iterator(ptr) : end();
	}

	template < class K >
	const_iterator lower_bound(const K &x) const {
		auto ptr = lower_bound_ptr(x);
		return (ptr) ? const_iterator(ptr) : end();
	}

	template < class K >
	iterator upper_bound(const K &x) {
		auto ptr = upper_bound_ptr(x);
		return (ptr) ? iterator(ptr) : end();
	}

	template < class K >
	const_iterator upper_bound(const K &x) const {
		auto ptr = upper_bound_ptr(x);
		return (ptr) ? const_iterator(ptr) : end();
	}
	template < class K >
	Pair<iterator, iterator> equal_range(const K &x) {
		return pair(lower_bound(x), upper_bound(x));
	}

	template < class K >
	Pair<const_iterator, const_iterator> equal_range(const K &x) const {
		return pair(lower_bound(x), upper_bound(x));
	}

	template < class K >
	size_t count(const K &x) const {
		return count_impl(x);
	}

	template < class K >
	size_t count_unique(const K &x) const {
		return findNode(x) ? 1 : 0;
	}

	void reserve(size_t c) {
		// if requested count is greater then size + pending preallocated nodes
		if (c > _size + _header.flag.size) {
			allocate_block(c - (_size + _header.flag.size));
		}
	}

protected:
	friend class TreeDebug;

	// header values has a special meanings:
	// _header.parent - left node from root (first node of iteration)
	// _header.right - right node from root
	// _header.left - root node (actual root of the tree), nullptr if tree is not defined
	// &_header (pointer to the header) - last node in iteration
	// _header.size - extra capacity, available via _free
	// _header.index - count of preallocated blocks in use
	// _header.prealloc - flag of persistent mode (enabled if 1, disabled by default)

	RbTreeNodeBase _header; // root is _header.left
	comparator_type _comp;
	value_allocator_type _allocator;
	size_t _size = 0;
	RbTreeNode<Value> *_free = nullptr;

	inline node_ptr root() { return static_cast<node_ptr>(_header.left); }
	inline const_node_ptr root() const { return static_cast<const_node_ptr>(_header.left); }
	inline void setroot(base_type n) {
		_header.left = n;
		n->parent = &_header;
	}

	inline node_ptr left() { return static_cast<node_ptr>(_header.parent); }
	inline const_node_ptr left() const { return static_cast<const_node_ptr>(_header.parent); }
	inline void setleft(base_type n) { _header.parent = (n == &_header) ? nullptr : n; }

	inline node_ptr right() { return static_cast<node_ptr>(_header.right); }
	inline const_node_ptr right() const { return static_cast<const_node_ptr>(_header.right); }
	inline void setright(base_type n) { _header.right = (n == &_header) ? nullptr : n; }


	inline const Key &extract(const Value &val) const {
		return RbTreeKeyExtractor<Key, Value>::extract(val);
	}
	inline const Key &extract(const RbTreeNodeStorage<Value> &s) const { return extract(s.ref()); }
	inline const Key &extract(const RbTreeNodeBase *s) const {
		return extract(static_cast<const RbTreeNode<Value> *>(s)->value.ref());
	}

	inline bool compareLtKey(const Key &l, const Key &r) const { return _comp(l, r); }
	inline bool compareEqKey(const Key &l, const Key &r) const {
		return !compareLtKey(l, r) && !compareLtKey(r, l);
	}

	template <typename A, typename B>
	inline bool compareLtTransparent(const A &l, const B &r) const {
		if constexpr (impl::is_detected_v<RbTreeDetectTransparent, Comp>) {
			return _comp(l, r);
		} else if constexpr (std::is_same_v<A, B>) {
			return compareLtKey(l, r);
		} else {
			static_assert(
					"Comparator should be transparent or search key and stored key types must be "
					"the same");
			return false;
		}
	}

	inline bool compareEqValue(const Value &l, const Value &r) const {
		return compareEqKey(extract(l), extract(r));
	}
	inline bool compareLtValue(const Value &l, const Value &r) const {
		return compareLtKey(extract(l), extract(r));
	}

	struct InsertData {
		const Key *key;
		RbTreeNode<Value> *val;
		RbTreeNodeBase *current;
		RbTreeNodeBase *parent;
		bool isLeft;
	};

	template <typename... Args>
	InsertData constructNode(Args &&...args) {
		RbTreeNode<Value> *ret = allocateNode();
		ret->parent = nullptr;
		ret->left = nullptr;
		ret->right = nullptr;
		ret->setColor(RbTreeNodeColor::Red);
		_allocator.construct(ret->value.ptr(), std::forward<Args>(args)...);

		return InsertData{&extract(ret->value), ret, nullptr, nullptr, false};
	}

	InsertData constructKey(const Key &k) {
		return InsertData{&k, nullptr, nullptr, nullptr, false};
	}

	InsertData constructKey(Key &&k) { return InsertData{&k, nullptr, nullptr, nullptr, false}; }

	template <typename K, typename... Args>
	RbTreeNode<Value> *constructEmplace(K &&k, Args &&...args) {
		RbTreeNode<Value> *ret = allocateNode();
		ret->parent = nullptr;
		ret->left = nullptr;
		ret->right = nullptr;
		ret->setColor(RbTreeNodeColor::Red);

		RbTreeKeyExtractor<Key, Value>::construct(_allocator, ret, std::forward<K>(k),
				std::forward<Args>(args)...);
		return ret;
	}

	template <typename M>
	void constructAssign(RbTreeNode<Value> *n, const M &m) {
		n->value.ref() = m;
	}

	template <typename M>
	void constructAssign(RbTreeNode<Value> *n, M &&m) {
		n->value.ref() = sp::move_unsafe(m);
	}

	bool getInsertPositionUnique_search(InsertData &d) {
		while (d.current != nullptr) {
			d.parent = d.current;
			if (compareLtKey(*(d.key), extract(d.current))) {
				d.isLeft = true;
				d.current = static_cast<RbTreeNode<Value> *>(d.current->left);
			} else {
				if (!compareLtKey(extract(d.current), *(d.key))) { // equality check
					return false;
				}
				d.isLeft = false;
				d.current = static_cast<RbTreeNode<Value> *>(d.current->right);
			}
		}
		return true;
	}

	bool getInsertPosition_tryRoot(InsertData &d) {
		if (_size == 0) {
			d.parent = nullptr;
			d.isLeft = true;
			d.current = nullptr;
			return true;
		}
		return false;
	}

	bool getInsertPositionUnique_tryHint(InsertData &d) {
		if (d.current == nullptr) {
			return false; // no hint provided
		}

		// check if hint is special value (begin or end)
		if (d.current == left() || static_cast<RbTreeNodeBase *>(d.current) == &_header) {
			d.current = nullptr;
			return false; // this cases served by next two functions
		}

		// check if we can insert just before hint
		auto h = static_cast<RbTreeNode<Value> *>(d.current);
		if (compareLtKey(*(d.key), extract(h->value))) {
			// check for bound
			auto p = static_cast<node_ptr>(RbTreeNodeBase::decrement(d.current));
			if (compareLtKey(extract(p->value), *(d.key))) {
				d.parent = d.current;
				d.current = d.current->left;
				d.isLeft = true;
				if (!getInsertPositionUnique_search(d)) {
					return true; // stop searching, non-unique position, current is not null
				}
				return true;
			}
		} else if (compareLtKey(extract(h->value), *(d.key))) {
			auto p = static_cast<node_ptr>(RbTreeNodeBase::increment(d.current));
			if (static_cast<RbTreeNodeBase *>(p) == &_header) { // insert back case
				d.parent = d.current;
				d.current = d.current->right;
				d.isLeft = false;
				return true;
			} else if (compareLtKey(*(d.key), extract(p->value))) {
				d.parent = d.current;
				d.current = d.current->right;
				d.isLeft = false;
				if (!getInsertPositionUnique_search(d)) {
					return true; // stop searching, non-unique position, current is not null
				}
				return true;
			}
		} else { // hint and new one is equal
			return true; // current is hint, non-unique cancel
		}

		d.current = nullptr;
		return false;
	}

	bool getInsertPositionUnique_tryLeft(InsertData &d) {
		if (auto l = left()) {
			if (compareLtKey(*(d.key), extract(l->value))) {
				d.current = nullptr;
				d.parent = l;
				d.isLeft = true;
				return true;
			} else if (!compareLtKey(extract(l->value), *(d.key))) {
				d.current = l;
				return true;
			}
		}
		return false;
	}

	bool getInsertPositionUnique_tryRight(InsertData &d) {
		if (auto r = right()) {
			if (compareLtKey(extract(r->value), *(d.key))) {
				d.current = nullptr;
				d.parent = r;
				d.isLeft = false;
				return true;
			} else if (!compareLtKey(*(d.key), extract(r->value))) {
				d.current = r;
				return true;
			}
		}
		return false;
	}

	bool getInsertPositionUnique(InsertData &d) {
		// *_try* functions should return true with non-nullptr d.current
		// if new node is not unique to stop search process
		if (getInsertPosition_tryRoot(d) || getInsertPositionUnique_tryHint(d)
				|| getInsertPositionUnique_tryLeft(d) || getInsertPositionUnique_tryRight(d)) {
			return d.current == nullptr;
		}

		// full-scan
		if (!d.current) {
			d.current = root();
		}
		/* find where node belongs */
		return getInsertPositionUnique_search(d);
	}

	template <typename... Args>
	Pair<RbTreeNode<Value> *, bool> insertNodeUnique(Args &&...args) {
		InsertData d = constructNode(std::forward<Args>(args)...);
		if (!getInsertPositionUnique(d)) {
			destroyNode(d.val);
			return pair(static_cast<RbTreeNode<Value> *>(d.current), false);
		}

		return pair(makeInsert(d.val, d.parent, d.isLeft), true);
	}

	template <typename... Args>
	RbTreeNode<Value> *insertNodeUniqueHint(const_iterator hint, Args &&...args) {
		InsertData d = constructNode(std::forward<Args>(args)...);
		d.current = hint.constcast()._node;
		if (!getInsertPositionUnique(d)) {
			destroyNode(d.val);
			return static_cast<RbTreeNode<Value> *>(d.current);
		}

		return makeInsert(d.val, d.parent, d.isLeft);
	}

	template <typename K, typename... Args>
	Pair<RbTreeNode<Value> *, bool> tryInsertNodeUnique(K &&k, Args &&...args) {
		InsertData d = constructKey(std::forward<K>(k));
		if (!getInsertPositionUnique(d)) {
			return pair(static_cast<RbTreeNode<Value> *>(d.current), false);
		}

		return pair(makeInsert(constructEmplace(std::forward<K>(k), std::forward<Args>(args)...),
							d.parent, d.isLeft),
				true);
	}

	template <typename K, typename... Args>
	RbTreeNode<Value> *tryInsertNodeUniqueHint(const_iterator hint, K &&k, Args &&...args) {
		InsertData d = constructKey(std::forward<K>(k));
		d.current = hint.constcast()._node;
		if (!getInsertPositionUnique(d)) {
			return static_cast<RbTreeNode<Value> *>(d.current);
		}

		return makeInsert(constructEmplace(std::forward<K>(k), std::forward<Args>(args)...),
				d.parent, d.isLeft);
	}

	template <typename K, typename M>
	Pair<RbTreeNode<Value> *, bool> tryAssignNodeUnique(K &&k, M &&m) {
		InsertData d = constructKey(std::forward<K>(k));
		if (!getInsertPositionUnique(d)) {
			constructAssign(d.current, std::forward<M>(m));
			return pair(static_cast<RbTreeNode<Value> *>(d.current), false);
		}

		return pair(makeInsert(constructEmplace(std::forward<K>(k), std::forward<M>(m)), d.parent,
							d.isLeft),
				true);
	}

	template <typename K, typename M>
	RbTreeNode<Value> *tryAssignNodeUniqueHint(const_iterator hint, K &&k, M &&m) {
		InsertData d = constructKey(std::forward<K>(k));
		d.current = hint.constcast()._node;
		if (!getInsertPositionUnique(d)) {
			constructAssign(d.current, std::forward<M>(m));
			return static_cast<RbTreeNode<Value> *>(d.current);
		}

		return makeInsert(constructEmplace(std::forward<K>(k), std::forward<M>(m)), d.parent,
				d.isLeft);
	}

	RbTreeNode<Value> *makeInsert(RbTreeNode<Value> *n, RbTreeNodeBase *parent, bool isLeft) {
		n->parent = parent;
		if (parent) {
			if (isLeft) {
				if (parent == left()) {
					setleft(n);
				}
				parent->left = n;
			} else {
				if (parent == right()) {
					setright(n);
				}
				parent->right = n;
			}
		} else {
			setleft(n);
			setright(n);
			setroot(n);
		}

		RbTreeNodeBase::insert(&_header, n);
		++_size;
		return n;
	}

	void deleteNode(RbTreeNodeBase *z) {
		RbTreeNodeBase *x = nullptr;
		RbTreeNodeBase *y = nullptr;

		if (!z) {
			return;
		}

		if (!z->left || !z->right) {
			/* y has a leaf node as a child */
			y = z;

			if (z == right()) {
				setright((z == left()) ? nullptr : RbTreeNodeBase::decrement(z));
			}
			if (z == left()) {
				setleft(RbTreeNodeBase::increment(z));
			}
		} else {
			y = z->left;
			while (y->right) { y = y->right; }
		}

		/* x is y's only child */
		if (y->left) {
			x = y->left;
		} else {
			x = y->right;
		}

		if (!x) {
			// if there is no replacement (we use empty leaf node as new Z),
			// we run rebalance with phantom Y node, then swap data and remove links
			// to phantom
			if (y->getColor() == RbTreeNodeColor::Black) {
				RbTreeNodeBase::remove(&_header, y);
			}

			if (y == y->parent->left) {
				y->parent->left = nullptr;
			} else {
				y->parent->right = nullptr;
			}


			if (y != z) {
				// copy node's data may be expensive, so, we keep data and replace node
				RbTreeNodeBase::replace(z, y);
			}

		} else {
			// if we have replacement, we insert it at proper place then call rebalance
			x->parent = y->parent;

			if (y == y->parent->left) {
				y->parent->left = x;
			} else {
				y->parent->right = x;
			}

			if (y != z) {
				RbTreeNodeBase::replace(z, y);
			} else {
				y->setColor(RbTreeNodeColor::Red);
			}

			if (y->getColor() == RbTreeNodeColor::Black) {
				RbTreeNodeBase::remove(&_header, x);
			} else {
				x->setColor(RbTreeNodeColor::Black);
			}
		}

		destroyNode(static_cast<RbTreeNode<Value> *>(z));
		--_size;
	}

	void clear_visit(RbTreeNode<Value> *target) {
		if (target->left) {
			clear_visit(static_cast<RbTreeNode<Value> *>(target->left));
		}
		if (target->right) {
			clear_visit(static_cast<RbTreeNode<Value> *>(target->right));
		}
		destroyNode(target);
	}

	void clone_visit(const RbTreeNode<Value> *source, RbTreeNode<Value> *target) {
		_allocator.construct(target->value.ptr(), source->value.ref());
		target->setColor(source->getColor());
		if (source->left) {
			target->left = allocateNode();
			target->left->parent = target;
			clone_visit(static_cast<RbTreeNode<Value> *>(source->left),
					static_cast<RbTreeNode<Value> *>(target->left));
			if (_header.parent == source->left) { // check for leftmost node
				_header.parent = target->left;
			}
		} else {
			target->left = nullptr;
		}

		if (source->right) {
			target->right = allocateNode();
			target->right->parent = target;
			clone_visit(static_cast<RbTreeNode<Value> *>(source->right),
					static_cast<RbTreeNode<Value> *>(target->right));
			if (_header.right == source->right) { // check for rightmost node
				_header.right = target->right;
			}
		} else {
			target->right = nullptr;
		}
	}

	void clone(const RbTree &other) {
		// prevent 'clear' from dealloc anything
		auto preallocTmp = memory_persistent();
		set_memory_persistent(true);
		clear();
		set_memory_persistent(preallocTmp);

		reserve(other._size);

		auto flag = _header.flag;

		_size = other._size;
		_comp = other._comp;
		_header = other._header;
		_header.flag = flag;
		if (other._header.left) {
			_header.left = allocateNode();
			_header.left->parent = &_header;
			if (other._header.left == other._header.parent) {
				_header.parent = _header.left;
			}
			if (other._header.left == other._header.right) {
				_header.right = _header.left;
			}
			clone_visit(static_cast<RbTreeNode<Value> *>(other._header.left),
					static_cast<RbTreeNode<Value> *>(_header.left));
		}
	}

	template < class K >
	node_ptr find_impl(const K &x) const {
		const_node_ptr current = root();
		while (current) {
			auto &key = extract(current);
			if (compareLtTransparent(x, key)) {
				current = static_cast<const_node_ptr>(current->left);
			} else {
				if (!compareLtTransparent(key, x)) { // equality check
					return const_cast<node_ptr>(current);
				}
				current = static_cast<const_node_ptr>(current->right);
			}
		}
		return nullptr;
	}

	template < class K >
	node_ptr lower_bound_ptr(const K &x) const {
		const_node_ptr current = root();
		const_node_ptr saved = nullptr;
		while (current) {
			if (!compareLtTransparent(extract(current), x)) {
				saved = current;
				current = static_cast<const_node_ptr>(current->left);
			} else {
				current = static_cast<const_node_ptr>(current->right);
			}
		}
		return const_cast<node_ptr>(saved);
	}

	template < class K >
	node_ptr upper_bound_ptr(const K &x) const {
		const_node_ptr current = root();
		const_node_ptr saved = current;
		while (current) {
			if (compareLtTransparent(x, extract(current))) {
				saved = current;
				current = static_cast<const_node_ptr>(current->left);
			} else {
				current = static_cast<const_node_ptr>(current->right);
			}
		}
		return const_cast<node_ptr>(saved);
	}

	template < class K >
	size_t count_impl(const K &x) const {
		auto c = find_impl(x);
		if (!c) {
			return 0;
		} else {
			size_t ret = 1;
			const_node_ptr next, current;

			current = c;
			next = static_cast<const_node_ptr>(RbTreeNodeBase::decrement(current));
			while (next && !compareLtTransparent(extract(next), extract(current))) {
				current = next;
				next = static_cast<const_node_ptr>(RbTreeNodeBase::decrement(current));
				ret++;
			}

			current = c;
			next = static_cast<const_node_ptr>(RbTreeNodeBase::increment(current));
			while (next && next != &_header
					&& !compareLtTransparent(extract(current), extract(next))) {
				current = next;
				next = static_cast<const_node_ptr>(RbTreeNodeBase::increment(current));
				ret++;
			}
			return ret;
		}
	}

	void destroyNode(RbTreeNode<Value> *n) {
		_allocator.destroy(n->value.ptr());
		if (!_free) {
			// no saved node - always hold one
			n->parent = nullptr;
			_free = n;
			++_header.flag.size; // increment capacity counter
		} else if (n->isPrealloc() || _header.flag.prealloc) {
			// node was preallocated - hold it in chain
			n->parent = _free;
			_free = n;
			++_header.flag.size; // increment capacity counter
		} else {
			// deallocate node
			node_allocator_type(_allocator).__deallocate(n, 1, n->getSize());
		}
	}

	RbTreeNode<Value> *allocateNode() {
		if (_free) {
			auto ret = _free;
			_free = (RbTreeNode<Value> *)ret->parent;
			--_header.flag.size; // decrement capacity counter
			return ret;
		} else {
			size_t s;
			auto ret = node_allocator_type(_allocator).__allocate(1, s);
			node_allocator_type(_allocator).construct(ret);
			ret->setSize(s);
			ret->setPrealloc(false);
			return ret;
		}
	}

	void allocate_block(size_t count) {
		RbTreeNode<Value> *block = nullptr, *tail = nullptr;
		if (_header.flag.index < node_type::Flag::MaxIndex) {
			uintptr_t preallocIdx = ++_header.flag.index;
			block = allocator_helper::allocate_block([](auto node, size_t idx) SPINLINE {
				return false;
			}, get_allocator(), preallocIdx, count, &tail);
		} else {
			block = allocator_helper::allocate_batch([](auto node, size_t idx) SPINLINE {
				return false;
			}, get_allocator(), count, &tail);
		}

		_header.flag.size += count; // increment capacity counter
		*tail->getNextStoragePtr() = _free;
		_free = block;
	}
};

} // namespace stappler::memory::detail

#if SP_MEM_RBTREE_DEBUG

namespace STAPPLER_VERSIONIZED stappler::memory::detail {

class TreeDebug {
public:
	enum class Validation {
		Valid,
		RootIsNotBlack,
		RedChildIntoRedNode,
		DifferentBlackNodeCount,
	};

	template <class T>
	static void visit(const T &tree, std::ostream &stream) {
		typename T::const_node_ptr r = tree.root();
		stream << "visit " << (void *)r << "  header: " << tree._header.left << " | "
			   << tree._header.right << " | " << tree._header.parent;
		stream << "\n";
		if (r) {
			visit(tree, stream, static_cast<typename T::const_node_ptr>(r), 0);
		}
	}

	template <class T>
	static Validation validate(const T &tree) {
		if (tree._header.left && tree._header.left->flag.color == RbTreeNodeColor::Red) {
			return Validation::RootIsNotBlack;
		} else {
			auto counter = 0;
			auto root = tree._header.left;
			while (root) {
				if (root->flag.color == RbTreeNodeColor::Black) {
					++counter;
				}
				root = root->left;
			}
			return validate(counter, tree._header.left, 0);
		}
	}

	//static bool make_test(std::ostream &stream, int size = 128);
	//static bool make_test(std::ostream &stream, const apr::array<int> &insert, const apr::array<int> &erase);

	static bool make_hint_test(std::ostream &stream, int size = 128);

protected:
	template <class T>
	static void visit(const T &tree, std::ostream &stream, typename T::const_node_ptr node,
			int depth) {
		if (node->left) {
			visit(tree, stream, static_cast<typename T::const_node_ptr>(node->left), depth + 1);
		}
		for (int i = 0; i < depth; i++) { stream << "--"; }
		stream << (void *)node << " l:" << (void *)node->left << " r:" << (void *)node->right
			   << " p:" << (void *)node->parent << " v:" << *(node->value.ptr())
			   << (node->flag.color ? " black" : " red") << "\n";
		if (node->right) {
			visit(tree, stream, static_cast<typename T::const_node_ptr>(node->right), depth + 1);
		}
	}

	static Validation validate(int counter, RbTreeNodeBase *node, int path) {
		if (node == nullptr) {
			if (counter != path) {
				return Validation::DifferentBlackNodeCount;
			}
			return Validation::Valid;
		} else {
			if (node->flag.color == RbTreeNodeColor::Black) {
				auto res = validate(counter, node->left, path + 1);
				if (res == Validation::Valid) {
					res = validate(counter, node->right, path + 1);
				}
				return res;
			} else {
				if ((node->left && node->left->flag.color == RbTreeNodeColor::Red)
						|| (node->right && node->right->flag.color == RbTreeNodeColor::Red)) {
					return Validation::RedChildIntoRedNode;
				} else {
					auto res = validate(counter, node->left, path);
					if (res == Validation::Valid) {
						res = validate(counter, node->right, path);
					}
					return res;
				}
			}
		}
	}
};

template <typename CharType>
inline std::basic_ostream<CharType> &operator<<(std::basic_ostream<CharType> &os,
		const TreeDebug::Validation &v) {
	switch (v) {
	case TreeDebug::Validation::Valid: os << "Valid"; break;
	case TreeDebug::Validation::DifferentBlackNodeCount: os << "DifferentBlackNodeCount"; break;
	case TreeDebug::Validation::RedChildIntoRedNode: os << "RedChildIntoRedNode"; break;
	case TreeDebug::Validation::RootIsNotBlack: os << "RootIsNotBlack"; break;
	}
	return os;
}

} // namespace stappler::memory::detail

#endif // SP_MEM_RBTREE_DEBUG

#endif /* STAPPLER_CORE_MEMORY_DETAIL_SPMEMRBTREE_H_ */
