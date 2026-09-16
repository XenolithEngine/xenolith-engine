/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h" // IWYU pragma: keep

#include "agentchat/SavedPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* The saved column's rules. The row here carries the same warning as the chat one: no `width:100%`
on it, or the horizontal overflow that creates takes the vertical scrolling down with it. */
static constexpr auto s_css = StringView(R"css(
#saved-panel { display: flex; flex-direction: column;
               flex-grow: 1; flex-shrink: 1; flex-basis: 0px; }

#saved-bar { order: 0; flex: 0 0 var(--bar-h); -xl-z-order: 2;
             display: flex; flex-direction: row; align-items: center;
             column-gap: 8px; padding: 0px 12px; background-color: #14141a; }
#saved-count { flex-grow: 1; color: var(--muted); font-size: 12px; white-space: nowrap; }

button#saved-clear { flex: 0 0 68px; height: 26px;
                     display: flex; flex-direction: row; align-items: center;
                     justify-content: center; background-color: var(--control);
                     outline-width: 1px; outline-color: var(--outline); border-radius: 5px; }
button#saved-clear:hover    { background-color: #3a3a46; }
button#saved-clear:disabled { background-color: #24242c; }
button#saved-clear > label  { color: var(--text); font-size: 12px; white-space: nowrap; }

#saved-list { order: 1; flex-grow: 1; flex-shrink: 1; flex-basis: 0px; -xl-z-order: 1;
              display: flex; flex-direction: column; row-gap: 8px;
              padding: 12px; overflow-y: auto; }

.saved-row { flex: 0 0 auto; display: flex; flex-direction: row; align-items: flex-start; }

panel.bubble.saved { background-color: #23232c;
                     outline-width: 1px; outline-color: var(--outline); }
panel.bubble.saved > label.title { color: var(--text); font-size: 13px; font-weight: bold; }
panel.bubble.saved > label.body  { color: #c9c9d2; font-size: 13px; }
panel.bubble.saved > label.meta  { color: var(--muted); font-size: 11px; }

label.saved-empty { color: var(--muted); font-size: 12px; }
)css");

// The card is bounded by the column, less the list's padding and the card's own.
static constexpr float s_cardChrome = 24.0f + 24.0f;
static constexpr float s_minWrapWidth = 120.0f;

// What a card shows above its text when the agent gave it no title of its own.
static constexpr auto s_untitled = StringView("Note");

} // namespace

StringView getSavedPanelStylesheet() { return s_css; }

bool SavedPanel::init(Rc<SavedMessages> &&saved) {
	if (!Node::init()) {
		return false;
	}

	_saved = sp::move(saved);
	setName("saved-panel");

	buildBar();
	buildList();

	return true;
}

void SavedPanel::buildBar() {
	_bar = addChild(Rc<Node>::create(), ZOrder(1));
	_bar->setName("saved-bar");

	_countLabel = _bar->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_countLabel->setType("label");
	_countLabel->setName("saved-count");

	_clearButton = _bar->addChild(Rc<ui::Button>::create(), ZOrder(2));
	_clearButton->setName("saved-clear");
	_clearButton->setString("Clear");
	_clearButton->setCallback([this] { _saved->clear(); });
}

void SavedPanel::buildList() {
	_list = addChild(Rc<Node>::create(), ZOrder(2));
	_list->setName("saved-list");
}

void SavedPanel::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	scheduleUpdate();

	_commands.attach(scene);
	registerCommands();

	// One observer, set here and dropped in handleExit: a callback outliving this node would be a
	// call into freed storage the first time the agent saved anything.
	_saved->setChangeCallback([this] { rebuild(); });

	rebuild();
}

void SavedPanel::handleExit() {
	_saved->setChangeCallback(nullptr);
	_commands.detach();

	Node::handleExit();
}

void SavedPanel::handleContentSizeDirty() {
	Node::handleContentSizeDirty();
	applyWrapWidth();
}

void SavedPanel::update(const UpdateTime &time) {
	Node::update(time);

	// The same safety net as the chat log: the cards publish their own heights when they are built.
	for (auto &it : _cards) { it->refreshHeight(); }
}

void SavedPanel::rebuild() {
	_cards.clear();
	_list->removeAllChildren();
	_emptyLabel = nullptr;

	_countLabel->setString(toString(_saved->size(), " saved"));
	_clearButton->setEnabled(!_saved->empty());

	if (_saved->empty()) {
		// An empty column says nothing; this says what would put something in it.
		_emptyLabel = _list->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		_emptyLabel->setType("label");
		_emptyLabel->addStyleClass("saved-empty");
		_emptyLabel->setString("Ask the agent to remember something.");
		if (_wrapWidth > 0.0f) {
			_emptyLabel->setWidth(_wrapWidth);
		}
		return;
	}

	int16_t z = 1;
	for (auto &it : _saved->getEntries()) {
		auto row = _list->addChild(Rc<Node>::create(), ZOrder(z++));
		row->addStyleClass("saved-row");

		auto card = row->addChild(Rc<ChatBubble>::create(StringView("saved")), ZOrder(1));
		card->setWrapWidth(_wrapWidth);
		card->setTitle(it.title.empty() ? s_untitled : StringView(it.title));
		card->setBody(it.text);
		card->setMeta(it.source);

		_cards.emplace_back(card);
	}
}

void SavedPanel::applyWrapWidth() {
	auto width = getContentSize().width - s_cardChrome;
	if (width < s_minWrapWidth) {
		width = s_minWrapWidth;
	}

	if (_wrapWidth == width) {
		return;
	}

	_wrapWidth = width;
	for (auto &it : _cards) { it->setWrapWidth(_wrapWidth); }
	if (_emptyLabel) {
		_emptyLabel->setWidth(_wrapWidth);
	}
}

// ---- inspector ---------------------------------------------------------------------------

void SavedPanel::registerCommands() {
	_commands.add("saved.list", "Everything the agent has been asked to remember",
			[this](const Value &, Function<void(Value &&)> &&done) {
		Value result;
		result.setBool(true, "ok");
		result.setInteger(int64_t(_saved->size()), "count");
		result.setValue(_saved->encode(), "items");
		done(sp::move(result));
	});

	_commands.add("saved.add", "Add an entry by hand: {\"title\", \"text\"}. No model involved",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		auto text = args.getString("text");

		Value result;
		if (text.empty()) {
			result.setBool(false, "ok");
			result.setString("'text' is required", "error");
			done(sp::move(result));
			return;
		}

		_saved->add(args.getString("title"), text, "manual");

		result.setBool(true, "ok");
		result.setInteger(int64_t(_saved->size()), "count");
		done(sp::move(result));
	});

	_commands.add("saved.clear", "Drop every saved entry",
			[this](const Value &, Function<void(Value &&)> &&done) {
		_saved->clear();

		Value result;
		result.setBool(true, "ok");
		result.setInteger(int64_t(_saved->size()), "count");
		done(sp::move(result));
	});

	_commands.add("saved.state", "What the panel actually built",
			[this](const Value &, Function<void(Value &&)> &&done) {
		Value result;
		result.setBool(true, "ok");
		result.setInteger(int64_t(_saved->size()), "count");
		result.setInteger(int64_t(_cards.size()), "cards");
		result.setDouble(_wrapWidth, "wrapWidth");

		if (auto scroll = _list ? _list->getSystemByType<ui::ScrollSystem>() : nullptr) {
			result.setDouble(scroll->getScrollRange().height, "scrollRange");
			result.setDouble(scroll->getScrollPosition().y, "scrollPosition");
		}

		done(sp::move(result));
	});
}

} // namespace stappler::xenolith::examples
