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

#include "SPMemRbtree.h"
#include "SPMemStringStream.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::memory::detail {

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

#if SP_MEM_RBTREE_DEBUG

bool TreeDebug::make_hint_test(std::ostream &stream, int size) {
	RbTree<int, int> tree_test1;
	auto time_test1_s = std::chrono::system_clock::now();
	for (int i = 0; i < size; ++i) { tree_test1.emplace(i); }
	auto time_test1 = std::chrono::system_clock::now() - time_test1_s;


	auto time_test2_s = std::chrono::system_clock::now();
	RbTree<int, int> tree_test2;
	auto it2 = tree_test2.begin();
	for (int i = 0; i < size; ++i) {
		tree_test2.emplace_hint(it2, i);
		it2 = tree_test2.end();
	}
	auto time_test2 = std::chrono::system_clock::now() - time_test2_s;


	auto time_test3_s = std::chrono::system_clock::now();
	RbTree<int, int> tree_test3;
	auto it3 = tree_test3.begin();
	for (int i = size; i > 0; --i) {
		tree_test3.emplace_hint(it3, i);
		it3 = tree_test3.end();
	}
	auto time_test3 = std::chrono::system_clock::now() - time_test3_s;


	auto time_test4_s = std::chrono::system_clock::now();
	RbTree<int, int> tree_test4;
	auto it4 = tree_test4.begin();
	for (int i = size; i > 0; --i) {
		tree_test4.emplace_hint(it4, i);
		it4 = tree_test4.begin();
	}
	auto time_test4 = std::chrono::system_clock::now() - time_test4_s;


	auto time_test5_s = std::chrono::system_clock::now();
	RbTree<int, int> tree_test5;
	auto it5 = tree_test5.begin();
	for (int i = 0; i < size; ++i) { it5 = tree_test5.emplace_hint(it5, i); }
	auto time_test5 = std::chrono::system_clock::now() - time_test5_s;


	stream << std::fixed << std::setprecision(2) << " time: " << time_test1.count()
		   << " Emplace test: " << validate(tree_test1) << "\n"
		   << " time: " << time_test2.count() << " Emplace Hint test: " << validate(tree_test2)
		   << "\n"
		   << " time: " << time_test3.count()
		   << " Emplace Wrong Hint test: " << validate(tree_test3) << "\n"
		   << " time: " << time_test4.count()
		   << " Emplace Corrected Hint test: " << validate(tree_test4) << "\n"
		   << " time: " << time_test5.count()
		   << " Emplace Closest Hint test: " << validate(tree_test5) << "\n";

	return true;
}

#endif

} // namespace stappler::memory::detail
