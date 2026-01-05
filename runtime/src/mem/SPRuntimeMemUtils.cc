/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "private/SPRTMemStruct.h"
#include <sprt/runtime/math.h>
#include <sprt/runtime/mem/detail/alloc.h>
#include <sprt/runtime/mem/detail/rbtree.h>
#include <sprt/runtime/mem/userdata.h>

namespace sprt::memory::impl {

void AllocManager::reset(void *p) {
	__builtin_memset(this, 0, sizeof(AllocManager));
	pool = p;
}

void *AllocManager::alloc(size_t &sizeInBytes, uint32_t alignment, AllocFn allocFn) {
	if (buffered) {
		MemAddr *c, **lastp;

		c = buffered;
		lastp = &buffered;
		while (c) {
			if (c->size > sizeInBytes * 2) {
				break;
			} else if (c->size >= sizeInBytes) {
				if (alignment > config::DefaultAlignment) {
					// check if we can place aligned block into cached block
					auto newAddr = c->address;
					size_t space = c->size;
					if (auto a = math::align<size_t>(alignment, sizeInBytes, newAddr, space)) {
						*lastp = c->next;
						c->next = free_buffered;
						free_buffered = c;
						sizeInBytes = space;
						increment_return(sizeInBytes);
						return a;
					}
				} else {
					*lastp = c->next;
					c->next = free_buffered;
					free_buffered = c;
					sizeInBytes = c->size;
					increment_return(sizeInBytes);
					return c->address;
				}
			}

			lastp = &c->next;
			c = c->next;
		}
	}
	increment_alloc(sizeInBytes);
	return allocFn(pool, sizeInBytes, alignment);
}

void AllocManager::free(void *ptr, size_t sizeInBytes, AllocFn allocFn) {
	MemAddr *addr = nullptr;
	if (allocated == 0 || sizeInBytes > 2_MiB) {
		return;
	}

	if (free_buffered) {
		addr = free_buffered;
		free_buffered = addr->next;
	} else {
		addr = (MemAddr *)allocFn(pool, sizeof(MemAddr), config::DefaultAlignment);
		increment_alloc(sizeof(MemAddr));
	}

	if (addr) {
		addr->size = uint32_t(sizeInBytes);
		addr->address = ptr;
		addr->next = nullptr;

		if (buffered) {
			MemAddr *c, **lastp;

			c = buffered;
			lastp = &buffered;
			while (c) {
				if (c->size >= sizeInBytes) {
					addr->next = c;
					*lastp = addr;
					break;
				}

				lastp = &c->next;
				c = c->next;
			}

			if (!addr->next) {
				*lastp = addr;
			}
		} else {
			buffered = addr;
			addr->next = nullptr;
		}
	}
}

void MemNode::insert(MemNode *point) {
	this->ref = point->ref;
	*this->ref = this;
	this->next = point;
	point->ref = &this->next;
}

void MemNode::remove() {
	*this->ref = this->next;
	this->next->ref = this->ref;
}

size_t MemNode::free_space() const { return endp - first_avail; }

void Cleanup::run(Cleanup **cref) {
	Cleanup *c = *cref;
	while (c) {
		*cref = c->next;
		if (c->fn) {
			(*c->fn)((void *)c->data);
		}
		c = *cref;
	}
}

} // namespace sprt::memory::impl


namespace sprt::memory::pool {

struct Pool_StoreHandle : AllocPool {
	void *pointer;
	memory::function<void()> callback;
};

static Status sa_request_store_custom_cleanup(void *ptr) {
	if (ptr) {
		auto ref = (Pool_StoreHandle *)ptr;
		if (ref->callback) {
			memory::perform_conditional([&] { ref->callback(); }, ref->callback.get_allocator());
		}
	}
	return Status::Ok;
}

void store(pool_t *pool, void *ptr, const StringView &key, memory::function<void()> &&cb) {
	context<pool_t *> ctx(pool, context<pool_t *>::conditional);

	void *ret = nullptr;
	pool::userdata_get(&ret, key.data(), key.size(), pool);
	if (ret) {
		auto h = (Pool_StoreHandle *)ret;
		h->pointer = ptr;
		if (cb) {
			h->callback = sprt::move(cb);
		} else {
			h->callback = nullptr;
		}
	} else {
		auto h = new (pool) Pool_StoreHandle();
		h->pointer = ptr;
		if (cb) {
			h->callback = sprt::move(cb);
		}

		if (key.terminated()) {
			pool::userdata_set(h, key.data(), sa_request_store_custom_cleanup, pool);
		} else {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif
			char buf[key.size() + 1];
			__builtin_memcpy(buf, key.data(), key.size());
			buf[key.size()] = 0;
			pool::userdata_set(h, key.data(), sa_request_store_custom_cleanup, pool);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
		}
	}
}

} // namespace sprt::memory::pool

namespace sprt::memory::detail {

RbTreeNodeBase *RbTreeNodeBase::increment(RbTreeNodeBase *c) {
	if (c->right) {
		c = c->right;
		while (c->left) { c = c->left; }
	} else {
		if (c->parent) {
			if (c->parent->left == c) {
				c = c->parent;
			} else {
				while (c->parent && c->parent->right == c) { c = c->parent; }
				if (!c->parent) {
					return nullptr;
				} else {
					c = c->parent;
				}
			}
		} else {
			return nullptr;
		}
	}
	return c;
}

const RbTreeNodeBase *RbTreeNodeBase::increment(const RbTreeNodeBase *c) {
	if (c->right) {
		// move right one step, then left until end
		c = c->right;
		while (c->left) { c = c->left; }
	} else {
		if (c->parent) {
			if (c->parent->left == c) {
				c = c->parent;
			} else {
				while (c->parent && c->parent->right == c) { c = c->parent; }
				if (!c->parent) {
					return nullptr; // end of iteration
				} else {
					c = c->parent;
				}
			}
		} else {
			return nullptr;
		}
	}
	return c;
}

RbTreeNodeBase *RbTreeNodeBase::decrement(RbTreeNodeBase *c) {
	if (c->left) {
		// move left one step, then right until end
		c = c->left;
		while (c->right) { c = c->right; }
	} else {
		if (c->parent) {
			if (c->parent->right == c) {
				c = c->parent;
			} else {
				while (c->parent && c->parent->left == c) { c = c->parent; }
				if (!c->parent) {
					return nullptr; // end of iteration
				} else {
					c = c->parent;
				}
			}
		} else {
			return nullptr;
		}
	}
	return c;
}

const RbTreeNodeBase *RbTreeNodeBase::decrement(const RbTreeNodeBase *c) {
	if (c->left) {
		// move left one step, then right until end
		c = c->left;
		while (c->right) { c = c->right; }
	} else {
		if (c->parent) {
			if (c->parent->right == c) {
				c = c->parent;
			} else {
				while (c->parent && c->parent->left == c) { c = c->parent; }
				if (!c->parent) {
					return nullptr; // end of iteration
				} else {
					c = c->parent;
				}
			}
		} else {
			return nullptr;
		}
	}
	return c;
}

RbTreeNodeBase *RbTreeNodeBase::replace(RbTreeNodeBase *old, RbTreeNodeBase *n) {
	n->left = old->left;
	n->right = old->right;
	n->setColor(old->getColor());
	n->parent = old->parent;

	// link new with parent
	if (old->parent) {
		if (old->parent->left == old) {
			old->parent->left = n;
		} else {
			old->parent->right = n;
		}
	}

	// link new with childs
	if (old->left && old->left != n) {
		old->left->parent = n;
	} else if (old->left == n) {
		n->left = nullptr;
	}
	if (old->right && old->right != n) {
		old->right->parent = n;
	} else if (old->right == n) {
		n->right = nullptr;
	}

	return old;
}

void RotateLeft(RbTreeNodeBase *head, RbTreeNodeBase *n, RbTreeNodeBase *p) {
	RbTreeNodeBase *tmp = n->left;
	if (p == head->left) {
		head->left = n;
	} else {
		if (p->parent->right == p) {
			p->parent->right = n;
		} else {
			p->parent->left = n;
		}
	}
	n->parent = p->parent;
	p->parent = n;
	n->left = p;

	if (tmp) {
		tmp->parent = p;
	}
	p->right = tmp;
}

void RotateRight(RbTreeNodeBase *head, RbTreeNodeBase *n, RbTreeNodeBase *p) {
	RbTreeNodeBase *tmp = n->right;
	if (p == head->left) {
		head->left = n;
	} else {
		if (p->parent->right == p) {
			p->parent->right = n;
		} else {
			p->parent->left = n;
		}
	}
	n->parent = p->parent;
	p->parent = n;
	n->right = p;

	if (tmp) {
		tmp->parent = p;
	}
	p->left = tmp;
}

void RbTreeNodeBase::insert(RbTreeNodeBase *head, RbTreeNodeBase *n) {
	/* check Red-Black properties */
	while (n != head->left && n->parent->getColor() == RbTreeNodeColor::Red) {
		auto p = n->parent;
		auto g = n->parent->parent;
		if (p == g->left) {
			RbTreeNodeBase *u = g->right;
			if (u && u->getColor() == RbTreeNodeColor::Red) {
				p->setColor(RbTreeNodeColor::Black);
				u->setColor(RbTreeNodeColor::Black);
				g->setColor(RbTreeNodeColor::Red);
				n = g;
			} else {
				if (n == p->right) {
					RotateLeft(head, n, p);
					n = n->left;
					p = n->parent;
				}
				p->setColor(RbTreeNodeColor::Black);
				g->setColor(RbTreeNodeColor::Red);
				RotateRight(head, p, g);
			}
		} else {
			RbTreeNodeBase *u = g->left;
			if (u && u->getColor() == RbTreeNodeColor::Red) {
				p->setColor(RbTreeNodeColor::Black);
				u->setColor(RbTreeNodeColor::Black);
				g->setColor(RbTreeNodeColor::Red);
				n = g;
			} else {
				if (n == n->parent->left) {
					RotateRight(head, n, p);
					n = n->right;
					p = n->parent;
				}
				p->setColor(RbTreeNodeColor::Black);
				g->setColor(RbTreeNodeColor::Red);
				RotateLeft(head, p, g);
			}
		}
	}
	head->left->setColor(RbTreeNodeColor::Black); // root
}

void RbTreeNodeBase::remove(RbTreeNodeBase *head, RbTreeNodeBase *n) {
	while (n != head->left && n->getColor() == RbTreeNodeColor::Black) {
		if (n == n->parent->left) {
			RbTreeNodeBase *s = n->parent->right;
			if (s && s->getColor() == RbTreeNodeColor::Red) {
				n->parent->setColor(RbTreeNodeColor::Red);
				s->setColor(RbTreeNodeColor::Black);
				RotateLeft(head, s, n->parent);
				s = n->parent->right;
			}
			if (s) {
				if (s->getColor() == RbTreeNodeColor::Black
						&& (!s->left || s->left->getColor() == RbTreeNodeColor::Black)
						&& (!s->right || s->right->getColor() == RbTreeNodeColor::Black)) {
					s->setColor(RbTreeNodeColor::Red);
					if (s->parent->getColor() == RbTreeNodeColor::Red) {
						s->parent->setColor(RbTreeNodeColor::Black);
						break;
					} else {
						n = n->parent;
					}
				} else {
					if ((!s->right || s->right->getColor() == RbTreeNodeColor::Black)
							&& (s->left && s->left->getColor() == RbTreeNodeColor::Red)) {
						s->setColor(RbTreeNodeColor::Red);
						s->left->setColor(RbTreeNodeColor::Black);
						RotateRight(head, s->left, s);
						s = n->parent->right;
					}
					s->setColor(n->parent->getColor());
					n->parent->setColor(RbTreeNodeColor::Black);
					if (s->right) {
						s->right->setColor(RbTreeNodeColor::Black);
					}
					RotateLeft(head, s, n->parent);
					break;
				}
			} else {
				break;
			}
		} else {
			RbTreeNodeBase *s = n->parent->left;
			if (s && s->getColor() == RbTreeNodeColor::Red) {
				n->parent->setColor(RbTreeNodeColor::Red);
				s->setColor(RbTreeNodeColor::Black);
				RotateRight(head, s, n->parent);
				s = n->parent->left;
			}
			if (s) {
				if (s->getColor() == RbTreeNodeColor::Black
						&& (!s->left || s->left->getColor() == RbTreeNodeColor::Black)
						&& (!s->right || s->right->getColor() == RbTreeNodeColor::Black)) {
					s->setColor(RbTreeNodeColor::Red);
					if (s->parent->getColor() == RbTreeNodeColor::Red) {
						s->parent->setColor(RbTreeNodeColor::Black);
						break;
					} else {
						n = n->parent;
					}
				} else {
					if ((!s->left || s->left->getColor() == RbTreeNodeColor::Black)
							&& (s->right && s->right->getColor() == RbTreeNodeColor::Red)) {
						s->setColor(RbTreeNodeColor::Red);
						s->right->setColor(RbTreeNodeColor::Black);
						RotateLeft(head, s->right, s);
						s = n->parent->left;
					}
					s->setColor(n->parent->getColor());
					n->parent->setColor(RbTreeNodeColor::Black);
					if (s->left) {
						s->left->setColor(RbTreeNodeColor::Black);
					}
					RotateRight(head, s, n->parent);
					break;
				}
			} else {
				break;
			}
		}
	}
	n->setColor(RbTreeNodeColor::Black);
}

} // namespace sprt::memory::detail
