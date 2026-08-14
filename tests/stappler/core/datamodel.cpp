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

// data::Model — the tree a virtualized view reads. What is asserted here is the set of things
// data::Source cannot do, because those are the reasons the class exists: interleaved order,
// identity that survives a structural change, moving an element, a category that carries its own
// record and its own external object, and a mutation that is routed to that object and can be
// refused by it.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPDataModel.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using TestModel = data::Model;
using TestNode = data::Model::Node;
using TestValue = data::Model::Value;

// Something a node can stand for. The destructor flag is how the lifetime section proves the tree
// holds no cycle: a parent back-pointer held by Rc would keep every one of these alive forever.
struct ModelTestObject : Ref {
	bool *freed = nullptr;

	virtual ~ModelTestObject() {
		if (freed) {
			*freed = true;
		}
	}
};

static TestValue makeValue(StringView name) {
	TestValue ret;
	ret.setString(name, "name");
	return ret;
}

// The names of one category's children, in display order — the one string that says whether the
// order is what the caller asked for.
static mem_std::String childNames(TestNode *cat) {
	mem_std::String out;
	for (auto &it : cat->getChildren()) {
		if (!out.empty()) {
			out.append(",");
		}
		out.append(it->getData().getString("name"));
	}
	return out;
}

static constexpr size_t Append = maxOf<size_t>();

} // namespace

void performDataModelTests() {
	sprt::cout << "\n== stappler data::Model tests ==\n";

	// ---- structure: arbitrary, interleaved order ------------------------------------------------
	//
	// The headline difference from data::Source, which always shows every subcategory before any of
	// its own items and offers no way to say otherwise.
	{
		auto model = Rc<TestModel>::create();
		check(model && model->getRoot() != nullptr, "model: a fresh model has a root");
		check(model->getRoot()->isCategory(), "model: the root is a category");

		auto root = model->getRoot();
		auto dirA = model->emplaceCategory(root, Append, makeValue("dirA"));
		model->emplaceItem(root, Append, makeValue("file1"));
		auto dirB = model->emplaceCategory(root, Append, makeValue("dirB"));
		model->emplaceItem(root, Append, makeValue("file2"));

		checkEq(childNames(root), "dirA,file1,dirB,file2",
				"model: categories and items interleave in the order they were added");

		model->emplaceItem(root, 2, makeValue("mid"));
		checkEq(childNames(root), "dirA,file1,mid,dirB,file2",
				"model: an item can be inserted at an arbitrary index");

		check(dirA->getChildIndex() == 0 && dirB->getChildIndex() == 3,
				"model: a node knows where it sits in its parent");
		check(dirA->getParent() == root, "model: a child points back at its parent");
	}

	// ---- identity survives structural change ----------------------------------------------------
	//
	// In data::Source an item IS its index, so removing one silently renames every item after it —
	// which is the whole reason TreeView has to throw away every loaded payload on any change.
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();

		auto first = model->emplaceItem(root, Append, makeValue("first"));
		auto second = model->emplaceItem(root, Append, makeValue("second"));
		auto third = model->emplaceItem(root, Append, makeValue("third"));

		const auto idFirst = first->getId();
		const auto idThird = third->getId();

		check(idFirst != idThird, "model: every node gets a distinct id");
		check(model->getNode(idThird) == third, "model: an id resolves to its node");

		model->emplaceItem(root, 0, makeValue("zero"));
		check(model->getNode(idThird) == third, "model: an id survives an insertion before it");

		check(model->removeNode(second), "model: an item can be removed");
		check(model->getNode(idThird) == third, "model: an id survives a removal before it");
		checkEq(third->getData().getString("name"), "third",
				"model: the node behind a surviving id still holds its own payload");

		// Ids are never reused, so a stale one is always simply "not found" and can never come back
		// meaning some other element.
		const auto idGone = idFirst;
		check(model->removeNode(first), "model: the first item can be removed");
		check(model->getNode(idGone) == nullptr, "model: a removed id stops resolving");

		model->emplaceItem(root, Append, makeValue("fresh"));
		check(model->getNode(idGone) == nullptr, "model: a later node does not inherit a freed id");
	}

	// ---- moving ---------------------------------------------------------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();

		auto src = model->emplaceCategory(root, Append, makeValue("src"));
		auto dst = model->emplaceCategory(root, Append, makeValue("dst"));
		auto leaf = model->emplaceItem(src, Append, makeValue("leaf"));
		const auto idLeaf = leaf->getId();

		check(model->moveNode(leaf, dst, 0), "model: an item moves between categories");
		check(leaf->getParent() == dst, "model: the moved node reports its new parent");
		check(src->getChildCount() == 0 && dst->getChildCount() == 1,
				"model: the move left one child list and joined the other");
		check(model->getNode(idLeaf) == leaf, "model: a move does not change identity");

		// Reordering within one parent. `index` reads against the list as it will be AFTER the node
		// is taken out, which is the only reading that makes "move one slot down" expressible.
		auto a = model->emplaceItem(dst, Append, makeValue("a"));
		model->emplaceItem(dst, Append, makeValue("b"));
		checkEq(childNames(dst), "leaf,a,b", "model: the starting order");
		check(model->moveNode(a, dst, 2), "model: a node moves within its own parent");
		checkEq(childNames(dst), "leaf,b,a", "model: reordering within one parent");

		// A node may not become a child of its own descendant: the cycle would be detached from the
		// root and kept alive by its own refcounts forever.
		auto deep = model->emplaceCategory(src, Append, makeValue("deep"));
		auto deeper = model->emplaceCategory(deep, Append, makeValue("deeper"));
		check(!model->moveNode(src, deeper, 0),
				"model: a node cannot move under its own descendant");
		check(src->getParent() == root, "model: the refused move changed nothing");

		check(!model->moveNode(model->getRoot(), dst, 0), "model: the root cannot be moved");
		check(!model->removeNode(model->getRoot()), "model: the root cannot be removed");

		// Removing a category takes its subtree with it.
		const auto idDeeper = deeper->getId();
		check(model->removeNode(deep), "model: a category can be removed");
		check(model->getNode(idDeeper) == nullptr,
				"model: removing a category unregisters its descendants too");
	}

	// ---- a category carries its own record and its own object -----------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto object = Rc<ModelTestObject>::alloc();

		auto cat = model->emplaceCategory(model->getRoot(), Append, makeValue("branch"), object);
		auto item = model->emplaceItem(cat, Append, makeValue("leaf"), object);

		checkEq(cat->getData().getString("name"), "branch", "model: a category has its own Value");
		check(cat->getObject() == object.get(), "model: a category points at an external object");
		check(item->getObject() == object.get(), "model: an item points at an external object");
		check(cat->getKind() == TestModel::Kind::Category
						&& item->getKind() == TestModel::Kind::Item,
				"model: kinds are reported");
	}

	// ---- payload edits are not structural changes -----------------------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto node = model->emplaceItem(model->getRoot(), Append, makeValue("before"));

		const auto subscriber = TestModel::getNextId();
		model->subscribe(subscriber);
		model->check(subscriber); // drop the Initial flag every new subscriber starts with

		check(node->getRevision() == 0, "model: a fresh node has revision 0");

		model->setNodeData(node, makeValue("after"));
		auto flags = model->check(subscriber);

		checkEq(node->getData().getString("name"), "after", "model: the payload was replaced");
		check(node->getRevision() == 1, "model: a payload edit bumps the revision");
		check(flags.hasFlag(TestModel::Update::Data), "model: a payload edit posts Data");
		check(!flags.hasFlag(TestModel::Update::Structure),
				"model: a payload edit does NOT post Structure");
	}

	// ---- a change deep in the tree reaches the one subscriber -----------------------------------
	//
	// data::Source cannot do this: every category is a Subscription of its own, so a branch's
	// setDirty() reaches nobody and the owner has to re-publish and re-dirty the root by hand.
	{
		auto model = Rc<TestModel>::create();
		auto branch = model->emplaceCategory(model->getRoot(), Append, makeValue("branch"));
		auto deep = model->emplaceCategory(branch, Append, makeValue("deep"));

		const auto subscriber = TestModel::getNextId();
		model->subscribe(subscriber);
		model->check(subscriber);

		model->emplaceItem(deep, Append, makeValue("buried"));
		check(model->check(subscriber).hasFlag(TestModel::Update::Structure),
				"model: a change three levels down reaches the subscriber");
	}

	// ---- spans: rows nobody stores --------------------------------------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();

		model->emplaceItem(root, Append, makeValue("header"));
		auto span = model->emplaceSpan(root, Append, 5'000,
				[](const TestModel::BatchCallback &cb, uint64_t first, size_t size) {
			TestModel::Map<uint64_t, TestValue> out;
			for (size_t i = 0; i < size; ++i) {
				out.emplace(first + i, makeValue(mem_std::toString("row", first + i)));
			}
			cb(out);
		});
		model->emplaceItem(root, Append, makeValue("footer"));

		check(span->getKind() == TestModel::Kind::Span, "model: a span reports its kind");
		check(span->getRowCount() == 5'000, "model: a span contributes its length in rows");
		check(root->getChildCount() == 3, "model: a span is ONE child among the explicit ones");

		TestModel::Map<uint64_t, TestValue> got;
		auto asked = span->getSpanData(
				[&](TestModel::Map<uint64_t, TestValue> &data) { got = sp::move(data); }, 10, 4);

		check(asked == 4 && got.size() == 4, "model: a slice answers exactly what was asked for");
		auto it = got.find(uint64_t(10));
		check(it != got.end() && it->second.getString("name") == "row10",
				"model: slice keys are offsets within the span");

		check(span->getSpanData([](TestModel::Map<uint64_t, TestValue> &) { }, 4'998, 10) == 2,
				"model: a slice is clamped to the span's length");
		check(span->getSpanData([](TestModel::Map<uint64_t, TestValue> &) { }, 6'000, 4) == 0,
				"model: a slice starting past the end asks for nothing");

		// Resizing keeps the span's identity, which is the reason a row is addressed by
		// (span id, offset) rather than by a reserved block of consecutive ids.
		const auto idSpan = span->getId();
		model->setSpanCount(span, 10);
		check(span->getRowCount() == 10, "model: a span can be resized");
		check(model->getNode(idSpan) == span, "model: resizing a span does not change its id");

		// A span sits in the child list like anything else, so it moves like anything else.
		auto other = model->emplaceCategory(root, Append, makeValue("other"));
		check(model->moveNode(span, other, 0), "model: a span node can be moved");
		check(span->getParent() == other, "model: the span landed in its new parent");
	}

	// ---- lazy children --------------------------------------------------------------------------
	{
		auto model = Rc<TestModel>::create();

		// Inline: a directory walk answers before the call returns.
		auto inlineCat = model->emplaceCategory(model->getRoot(), Append, makeValue("inline"));
		// `self` is the parameter, and the model is captured RAW: the callback is stored in the node,
		// which the model owns, so an Rc either way round would be a cycle.
		inlineCat->setChildsCallback(
				[m = model.get()](TestNode *self, const mem_std::Function<void()> &complete) {
			m->emplaceItem(self, Append, makeValue("walked"));
			complete();
		});

		check(inlineCat->getChildsState() == TestModel::ChildsState::Pending,
				"model: installing a childs callback makes the category Pending");

		bool inlineDone = false;
		inlineCat->requestChilds([&] { inlineDone = true; });
		check(inlineDone, "model: an inline lazy load completes before the call returns");
		check(inlineCat->getChildCount() == 1, "model: the inline load produced its child");
		check(inlineCat->getChildsState() == TestModel::ChildsState::Loaded,
				"model: the category is Loaded afterwards");

		// A second request must not run the callback again.
		inlineCat->requestChilds(nullptr);
		check(inlineCat->getChildCount() == 1, "model: a loaded category is not loaded twice");

		// Deferred: a fetch answers later.
		mem_std::Function<void()> saved;
		auto lazyCat = model->emplaceCategory(model->getRoot(), Append, makeValue("lazy"));
		lazyCat->setChildsCallback([&saved](TestNode *, const mem_std::Function<void()> &complete) {
			saved = complete;
		});

		bool lazyDone = false;
		lazyCat->requestChilds([&] { lazyDone = true; });
		check(!lazyDone && lazyCat->getChildsState() == TestModel::ChildsState::Loading,
				"model: a deferred lazy load reports Loading and has not completed");

		model->emplaceItem(lazyCat, Append, makeValue("fetched"));
		saved();
		check(lazyDone && lazyCat->getChildsState() == TestModel::ChildsState::Loaded,
				"model: the completion fires the waiting callbacks");

		// resetChilds drops the children and goes back to Pending, so the next request reloads.
		const auto idFetched = lazyCat->getChildren().at(0)->getId();
		lazyCat->resetChilds();
		check(lazyCat->getChildCount() == 0
						&& lazyCat->getChildsState() == TestModel::ChildsState::Pending,
				"model: resetChilds empties the category and re-arms it");
		check(model->getNode(idFetched) == nullptr, "model: the dropped children stop resolving");
	}

	// ---- a retired deferred load ------------------------------------------------------------------
	//
	// The hazard a loader that answers on another thread runs into: the branch is refreshed while it
	// is still fetching, so its answer describes children that have already been thrown away. Left
	// unguarded, the stale completion marks the category Loaded over the load that replaced it, and
	// fires that load's waiters over children that have not arrived.
	{
		auto model = Rc<TestModel>::create();

		mem_std::Vector<mem_std::Function<void()>> saved;
		auto cat = model->emplaceCategory(model->getRoot(), Append, makeValue("branch"));
		cat->setChildsCallback([&saved](TestNode *, const mem_std::Function<void()> &complete) {
			saved.emplace_back(complete);
		});

		cat->requestChilds(nullptr);
		check(saved.size() == 1 && cat->getChildsState() == TestModel::ChildsState::Loading,
				"model: the first load is in flight");

		// The refresh that retires it, and the load that replaces it.
		cat->resetChilds();
		bool secondDone = false;
		cat->requestChilds([&] { secondDone = true; });
		check(saved.size() == 2 && cat->getChildsState() == TestModel::ChildsState::Loading,
				"model: a reset while Loading re-arms the category for a new load");

		// The FIRST load answers now, late.
		saved.at(0)();
		check(!secondDone, "model: a retired completion does not fire the current load's waiters");
		check(cat->getChildsState() == TestModel::ChildsState::Loading,
				"model: a retired completion leaves the category Loading for the load that "
				"replaced it");

		model->emplaceItem(cat, Append, makeValue("late"));
		saved.at(1)();
		check(secondDone && cat->getChildsState() == TestModel::ChildsState::Loaded,
				"model: the current load still completes normally");
		check(cat->getChildCount() == 1, "model: and its children are the ones on show");
	}

	// ---- sorting keeps identity ------------------------------------------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();

		model->emplaceItem(root, Append, makeValue("c"));
		auto a = model->emplaceItem(root, Append, makeValue("a"));
		model->emplaceCategory(root, Append, makeValue("b"));
		const auto idA = a->getId();

		model->sortChildren(root, [](const TestNode *l, const TestNode *r) {
			return l->getData().getString("name") < r->getData().getString("name");
		});

		checkEq(childNames(root), "a,b,c", "model: children sort in place, kinds mixed");
		check(model->getNode(idA) == a, "model: sorting does not change any id");
	}

	// ---- the slots: veto ------------------------------------------------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto keep = model->emplaceItem(model->getRoot(), Append, makeValue("keep"));
		auto dst = model->emplaceCategory(model->getRoot(), Append, makeValue("dst"));

		model->setSlots(TestModel::Slots{
			.canMove = [](const TestNode *, const TestNode *, size_t) { return false; },
			.canRemove = [](const TestNode *) { return false; },
		});

		check(!model->removeNode(keep), "model: canRemove vetoes a removal");
		check(keep->getParent() == model->getRoot(), "model: the vetoed node is untouched");
		check(!model->moveNode(keep, dst, 0), "model: canMove vetoes a move");
		check(keep->getParent() == model->getRoot(), "model: the vetoed move changed nothing");
	}

	// ---- the slots: optimistic apply, and revert when the outside world refuses ------------------
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();
		auto doomed = model->emplaceItem(root, Append, makeValue("doomed"));
		auto other = model->emplaceItem(root, Append, makeValue("other"));
		const auto idDoomed = doomed->getId();

		TestModel::CompletionCallback pending;
		size_t removeCalls = 0;

		model->setSlots(TestModel::Slots{
			.performRemove =
					[&](TestNode *, TestModel::CompletionCallback &&done) {
			++removeCalls;
			pending = sp::move(done);
		},
		});

		check(model->removeNode(doomed), "model: an optimistic removal is accepted");
		check(removeCalls == 1, "model: the removal was handed to the slot");
		check(root->getChildCount() == 1, "model: the row is gone from the tree at once");
		check(model->hasPendingMutations(), "model: the mutation is recorded as pending");

		// Declined, so the model has to put it back — and the node must still exist to be put back,
		// which is what the pending record is for.
		pending(Status::Declined);
		check(!model->hasPendingMutations(), "model: the pending record is retired");
		check(root->getChildCount() == 2, "model: a refused removal is undone");
		checkEq(childNames(root), "doomed,other", "model: the node came back where it was");
		check(model->getNode(idDoomed) != nullptr, "model: the restored node resolves again");

		// And now let one succeed.
		check(model->removeNode(model->getNode(idDoomed)), "model: a second removal is accepted");
		pending(Status::Ok);
		check(root->getChildCount() == 1, "model: a confirmed removal stays applied");
		check(model->getNode(idDoomed) == nullptr, "model: the confirmed removal freed the id");
		(void)other;
	}

	// ---- the slots: a refused MOVE goes back where it came from ----------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();
		auto src = model->emplaceCategory(root, Append, makeValue("src"));
		auto dst = model->emplaceCategory(root, Append, makeValue("dst"));
		model->emplaceItem(src, Append, makeValue("stay"));
		auto moving = model->emplaceItem(src, Append, makeValue("moving"));

		TestModel::CompletionCallback pending;
		model->setSlots(TestModel::Slots{
			.performMove =
					[&](TestNode *, TestNode *, size_t, TestModel::CompletionCallback &&done) {
			pending = sp::move(done);
		},
		});

		check(model->moveNode(moving, dst, 0), "model: an optimistic move is accepted");
		check(moving->getParent() == dst, "model: the move is visible at once");

		pending(Status::Declined);
		check(moving->getParent() == src, "model: a refused move is undone");
		checkEq(childNames(src), "stay,moving", "model: the node came back at its old index");
	}

	// ---- the slots: Confirmed waits ---------------------------------------------------------------
	{
		auto model = Rc<TestModel>::create();
		auto root = model->getRoot();
		auto src = model->emplaceCategory(root, Append, makeValue("src"));
		auto dst = model->emplaceCategory(root, Append, makeValue("dst"));
		auto moving = model->emplaceItem(src, Append, makeValue("moving"));

		TestModel::CompletionCallback pending;
		model->setMutationPolicy(TestModel::MutationPolicy::Confirmed);
		model->setSlots(TestModel::Slots{
			.performMove =
					[&](TestNode *, TestNode *, size_t, TestModel::CompletionCallback &&done) {
			pending = sp::move(done);
		},
		});

		check(model->moveNode(moving, dst, 0), "model: a confirmed move is accepted");
		check(moving->getParent() == src, "model: nothing moves until the action confirms");

		pending(Status::Ok);
		check(moving->getParent() == dst, "model: the confirmation applies the move");
	}

	// ---- lifetime: the tree owns downwards only ----------------------------------------------------
	//
	// A parent back-pointer held by Rc would make every branch a cycle, and nothing below would ever
	// be freed. The flag on the shared object is what proves it is not.
	{
		bool freed = false;
		{
			auto model = Rc<TestModel>::create();
			auto object = Rc<ModelTestObject>::alloc();
			object->freed = &freed;

			auto node = model->getRoot();
			for (uint32_t i = 0; i < 6; ++i) {
				node = model->emplaceCategory(node, Append, makeValue("level"), object);
			}
			model->emplaceItem(node, Append, makeValue("bottom"), object);

			object = nullptr; // the tree is now the only owner
			check(!freed, "model: the tree keeps the object it points at alive");
			check(model->getNodeCount() == 8, "model: root plus six levels plus the leaf");
		}
		check(freed, "model: releasing the model frees the whole tree");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
