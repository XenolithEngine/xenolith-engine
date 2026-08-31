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


#include "XLCommon.h"

#include "widgets/InlineEditorLayout.h"
#include "XLUiCheckbox.h"
#include "XLUiStyleResolver.h"
#include "XL2dSceneContent.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float s_rowHeight = 28.0f;
static constexpr size_t s_rowCount = 40;

static constexpr auto s_inlineCss = StringView(R"css(
label {
	color: #e8e8e8;
	font-size: 14px;
}
#edited-label {
	width: 220px;
	height: 30px;
	background-color: #292929;
	color: #e8e8e8;
	font-size: 14px;
}
table-view {
	width: 320px;
	height: 280px;
	background-color: #202026;
	outline-width: 1px;
	outline-color: #3d3d3d;
}
table-row.selected {
	background-color: #3a3a5c;
}
text-input {
	width: 220px;
	height: 30px;
	background-color: #292929;
	outline-width: 1px;
	outline-color: rgba(255,255,255,.15);
	border-radius: 4px;
	padding: 0 8px;
	color: #e8e8e8;
	font-size: 14px;
}
inline-editor {
	background-color: #1b1b1b;
	outline-width: 1px;
	outline-color: #fcb400;
	color: #ffffff;
	font-size: 14px;
	padding: 0 4px;
}
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool InlineEditorLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_inlineCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_labelText = String("Transform");

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setName("edited-label");
	_label->setType("label");
	_label->setString(_labelText);
	_label->setAlignment(font::TextAlign::Left);

	_labelTarget = _label->addSystem(
			Rc<ui::InlineEditTarget>::create(ui::InlineEditTrigger::DoubleTap, _labelText));
	_labelTarget->setCommitCallback([this](const Value &value) {
		if (_refuse) {
			++_refusals;
			return false;
		}
		++_commits;
		_lastCommit = value.getString();
		_labelText = _lastCommit;
		_label->setString(_labelText);
		_labelTarget->setText(_labelText);
		return true;
	});
	_labelTarget->setCancelCallback([this] { ++_cancels; });
	_labelTarget->setCloseCallback([this] { ++_closes; });

	/* THE FACTORY PATH, which had never been exercised and did not work.

	The editor is a ui::Checkbox: a widget with no text, so a value arriving here could not have come
	from the stock one-line editor by accident. What makes it work is setCollectCallback - without
	it InlineEditTarget builds the editor, opens the session, and hands `onCommit` a Nil, because
	`collect` is optional and nobody was filling it in on the factory's behalf. */
	_custom = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_custom->setName("edited-custom");
	_custom->setType("label");
	_custom->setString(_customValue ? "custom: true" : "custom: false");
	_custom->setAlignment(font::TextAlign::Left);

	_customTarget = _custom->addSystem(Rc<ui::InlineEditTarget>::create(
			ui::InlineEditTrigger::DoubleTap, StringView("custom")));
	_customTarget->setFactory([this](const ui::InlineEditRequest &) -> Rc<Node> {
		auto box = Rc<ui::Checkbox>::create();
		box->setChecked(_customValue, true);
		return box;
	});
	_customTarget->setCollectCallback([this]() -> Value {
		if (auto session = _customTarget->getSession()) {
			if (auto box = dynamic_cast<ui::Checkbox *>(session->getEditor())) {
				return Value(box->isChecked());
			}
		}
		return Value();
	});
	_customTarget->setCommitCallback([this](const Value &value) {
		if (_refuse) {
			++_refusals;
			return false;
		}
		++_commits;
		_lastCommitValue = value;
		_customValue = value.asBool();
		_custom->setString(_customValue ? "custom: true" : "custom: false");
		return true;
	});
	_customTarget->setCancelCallback([this] { ++_cancels; });
	_customTarget->setCloseCallback([this] { ++_closes; });

	_table = addChild(Rc<ui::TableView>::create(), ZOrder(1));
	_table->setName("fields");
	_table->setHeaderVisible(false);
	_table->setRowHeight(s_rowHeight);
	_table->setSelectionEnabled(true);
	_table->setColumns(Vector<ui::TableView::Column>{
		{String("name"), String(), String("cell-name"), ui::GridTrack()},
	});

	rebuildModel();

	_neighbour = addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setText("abcdef");
	_neighbour->setCaretBlink(false);

	return true;
}

void InlineEditorLayout::rebuildModel() {
	_model = Rc<data::Model>::create();
	_values.clear();

	auto root = _model->getRoot();
	for (uint32_t i = 0; i < s_rowCount; ++i) {
		auto name = toString("field", i);
		Value value;
		value.setString(name, "name");
		_model->emplaceItem(root, maxOf<size_t>(), sp::move(value));
		_values.emplace_back(sp::move(name));
	}

	if (_table) {
		_table->setSource(_model);
	}
}

void InlineEditorLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 20.0f;

	if (_label) {
		_label->setAnchorPoint(Vec2(0.0f, 1.0f));
		_label->setPosition(Vec2(48.0f, top));
		_label->setWidth(220.0f);
	}
	if (_custom) {
		_custom->setAnchorPoint(Vec2(0.0f, 1.0f));
		_custom->setPosition(Vec2(48.0f, top - 30.0f));
		_custom->setWidth(220.0f);
	}
	if (_neighbour) {
		_neighbour->setAnchorPoint(Vec2(0.0f, 1.0f));
		_neighbour->setPosition(Vec2(48.0f, top - 70.0f));
		_neighbour->setContentSize(Size2(220.0f, 30.0f));
	}
	if (_table) {
		_table->setAnchorPoint(Vec2(0.0f, 1.0f));
		_table->setPosition(Vec2(320.0f, top));
		_table->setContentSize(Size2(320.0f, 280.0f));
	}
}

bool InlineEditorLayout::getCellRect(size_t row, Rect &out) const {
	if (!_table) {
		return false;
	}

	/* Derived from the row's NODE, so it only answers for a row that is currently built.

	That is the whole argument for E4's TableView::getCellRect: a row that has no node still has a
	rectangle - its height is resolved in rebuildRows() before any node exists - and reproducing
	that from outside means copying the list's own arithmetic. A stand can get away with the node
	because it only ever edits a row it can see. */
	auto controller = _table->getController();
	if (!controller) {
		return false;
	}

	for (auto &item : controller->getItems()) {
		auto node = dynamic_cast<ui::TableView::RowNode *>(item.node);
		if (!node || node->getRowIndex() != row) {
			continue;
		}

		const auto size = node->getContentSize();
		const Vec2 corners[4] = {
			_table->convertToNodeSpace(node->convertToWorldSpace(Vec2::ZERO)),
			_table->convertToNodeSpace(node->convertToWorldSpace(Vec2(size.width, 0.0f))),
			_table->convertToNodeSpace(node->convertToWorldSpace(Vec2(0.0f, size.height))),
			_table->convertToNodeSpace(node->convertToWorldSpace(Vec2(size.width, size.height))),
		};

		Vec2 low = corners[0];
		Vec2 high = corners[0];
		for (uint32_t i = 1; i < 4; ++i) {
			low.x = sprt::min(low.x, corners[i].x);
			low.y = sprt::min(low.y, corners[i].y);
			high.x = sprt::max(high.x, corners[i].x);
			high.y = sprt::max(high.y, corners[i].y);
		}

		out = Rect(low.x, low.y, high.x - low.x, high.y - low.y);
		return out.size.width > 0.0f && out.size.height > 0.0f;
	}
	return false;
}

bool InlineEditorLayout::beginLabelEdit() { return _labelTarget && _labelTarget->begin(); }

bool InlineEditorLayout::beginCustomEdit() { return _customTarget && _customTarget->begin(); }

bool InlineEditorLayout::beginCellEdit(size_t row) {
	if (!_table || row >= _values.size() || (_cellSession && _cellSession->isOpen())) {
		return false;
	}

	Rect rect;
	if (!getCellRect(row, rect)) {
		return false;
	}

	_editedRow = row;

	ui::InlineTextEditConfig config;
	config.closeOnScroll = _closeOnScroll;
	config.text = _values[row];
	config.onCommit = [this, row](StringView text) {
		if (_refuse) {
			++_refusals;
			return false;
		}
		++_commits;
		_lastCommit = text.str<Interface>();
		_values[row] = _lastCommit;

		// One row's payload, not the whole model: this is also what proves the editor is not a
		// child of the row it edits, because the row it edits is exactly the one being rebuilt.
		auto children = _model->getRoot()->getChildren();
		if (row < children.size()) {
			auto node = children.at(row).get();
			Value value;
			value.setString(_values[row], "name");
			_model->setNodeData(node, sp::move(value));
		}
		return true;
	};
	config.onCancel = [this] { ++_cancels; };
	config.onClose = [this] {
		++_closes;
		_cellSession = nullptr;
		_editedRow = maxOf<size_t>();
	};

	// The TABLE is the anchor: its space is where the rect keeps its meaning while rows underneath
	// are destroyed and rebuilt.
	_cellSession = ui::beginInlineTextEdit(_table, rect, sp::move(config));
	return _cellSession != nullptr;
}

Value InlineEditorLayout::encodeState() const {
	Value ret;

	ret.setString(_labelText, "labelText");
	ret.setBool(_labelTarget && _labelTarget->isEditing(), "labelEditing");

	// The factory path. `lastCommitNull` is the one that mattered: before setCollectCallback every
	// commit through a factory-built editor arrived as Nil, and a check that only looked at the
	// boolean would have read that as `false` and passed.
	ret.setBool(_customValue, "customValue");
	ret.setBool(_customTarget && _customTarget->isEditing(), "customEditing");
	ret.setBool(_lastCommitValue.isNull(), "lastCommitNull");
	ret.setBool(_lastCommitValue.asBool(), "lastCommitBool");
	if (_customTarget && _customTarget->isEditing()) {
		if (auto box = dynamic_cast<ui::Checkbox *>(_customTarget->getSession()->getEditor())) {
			ret.setBool(box->isChecked(), "editorChecked");
		}
	}

	const bool cellEditing = _cellSession && _cellSession->isOpen();
	ret.setBool(cellEditing, "cellEditing");
	ret.setInteger(_editedRow == maxOf<size_t>() ? -1 : int64_t(_editedRow), "editedRow");

	// What the open editor currently holds. The claim "a rebuild does not kill the input" is a
	// claim about this string.
	ui::InlineEditSession *session = nullptr;
	if (cellEditing) {
		session = _cellSession.get();
	} else if (_labelTarget && _labelTarget->isEditing()) {
		session = _labelTarget->getSession();
	}
	if (session) {
		if (auto input = dynamic_cast<ui::TextInput *>(session->getEditor())) {
			ret.setString(input->getText(), "editorText");

			/* The editor is seeded and selected BEFORE its first visit, so its label has no size
			yet when the highlight is computed. Reporting where the highlight ended up - not merely
			that a selection exists - is the only way to see the difference: a selection built
			against a height of zero sits below the text, where the field's scissor removes it, and
			every other field here still reads as if it were on screen. */
			auto label = input->getContainer()->getLabel();
			auto rect = label->getSelectionRect();
			Value sel;
			sel.setDouble(double(rect.origin.y), "y");
			sel.setDouble(double(rect.size.height), "height");
			sel.setDouble(double(rect.size.width), "width");
			sel.setDouble(double(label->getContentSize().height), "labelHeight");
			sel.setDouble(double(label->getPosition().y), "labelY");
			sel.setDouble(double(input->getContainer()->getContentSize().height), "viewportHeight");
			ret.setValue(sp::move(sel), "editorSelection");
		}
		ret.setBool(session->getLayout() != nullptr, "overlay");
	}

	ret.setInteger(int64_t(_commits), "commits");
	ret.setInteger(int64_t(_cancels), "cancels");
	ret.setInteger(int64_t(_closes), "closes");
	ret.setInteger(int64_t(_refusals), "refusals");
	ret.setString(_lastCommit, "lastCommit");
	ret.setBool(_refuse, "refusing");
	ret.setBool(_closeOnScroll, "closeOnScroll");

	// What the rebuild callback saw, beside what the same row measures NOW: equal means the answer
	// was complete when it arrived
	ret.setInteger(int64_t(_rebuildAnswers), "rebuildAnswers");
	ret.setInteger(std::lround(_rebuildRowWidth), "rebuildRowWidth");
	Rect watched;
	ret.setInteger(getCellRect(_rebuildWatchRow, watched) ? std::lround(watched.size.width) : 0,
			"watchRowWidth");

	if (_table) {
		ret.setInteger(int64_t(_table->getRowCount()), "rowCount");
		if (auto scroll = _table->getScroll()) {
			ret.setInteger(int64_t(std::lround(scroll->getScrollPosition() * 100.0f)), "scroll");
		}
	}

	Value values;
	for (auto &it : _values) { values.addString(it); }
	ret.setValue(sp::move(values), "values");

	if (_neighbour) {
		ret.setString(_neighbour->getText(), "neighbourText");
		ret.setInteger(int64_t(_neighbour->getCursor().start), "neighbourCursor");
		ret.setBool(_neighbour->isFocused(), "neighbourFocused");
	}
	return ret;
}

void InlineEditorLayout::registerCommands() {
	addCommand("state", "Report both editors, the values and the ending counters",
			[this](Value &&) { return encodeState(); });

	addCommand("begin", "Open an editor: {target=label|cell|custom, row}", [this](Value &&args) {
		const Value &a = args;
		const auto target = a.getString("target");
		if (target == "cell") {
			return ackValue(beginCellEdit(size_t(a.getInteger("row"))));
		}
		if (target == "custom") {
			return ackValue(beginCustomEdit());
		}
		return ackValue(beginLabelEdit());
	});

	// The factory's editor has no text to type into, which is the point of choosing it.
	addCommand("check", "Set the custom editor's checkbox: {value}", [this](Value &&args) {
		if (!_customTarget || !_customTarget->isEditing()) {
			return ackValue(false);
		}
		auto box = dynamic_cast<ui::Checkbox *>(_customTarget->getSession()->getEditor());
		if (!box) {
			return ackValue(false);
		}
		box->setChecked(static_cast<const Value &>(args).getBool("value"), true);
		return ackValue(true);
	});

	addCommand("type", "Put text into the open editor: {value}", [this](Value &&args) {
		ui::InlineEditSession *session = nullptr;
		if (_cellSession && _cellSession->isOpen()) {
			session = _cellSession.get();
		} else if (_labelTarget && _labelTarget->isEditing()) {
			session = _labelTarget->getSession();
		}
		if (!session) {
			return ackValue(false);
		}
		auto input = dynamic_cast<ui::TextInput *>(session->getEditor());
		if (!input) {
			return ackValue(false);
		}
		input->setText(static_cast<const Value &>(args).getString("value"));
		return ackValue(true);
	});

	addCommand("commit", "Commit the open editor", [this](Value &&) {
		if (_cellSession && _cellSession->isOpen()) {
			return ackValue(_cellSession->commit());
		}
		if (_labelTarget && _labelTarget->isEditing()) {
			return ackValue(_labelTarget->getSession()->commit());
		}
		if (_customTarget && _customTarget->isEditing()) {
			return ackValue(_customTarget->getSession()->commit());
		}
		return ackValue(false);
	});

	addCommand("cancel", "Cancel the open editor", [this](Value &&) {
		if (_cellSession && _cellSession->isOpen()) {
			_cellSession->cancel();
			return ackValue(true);
		}
		if (_customTarget && _customTarget->isEditing()) {
			_customTarget->getSession()->cancel();
			return ackValue(true);
		}
		if (_labelTarget && _labelTarget->isEditing()) {
			_labelTarget->getSession()->cancel();
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("rebuild", "Rebuild every row of the table (TableView::invalidateSource)",
			[this](Value &&) {
		if (!_table) {
			return ackValue(false);
		}
		_table->invalidateSource();
		return ackValue(true);
	});

	addCommand("scroll", "Scroll the table by {delta}", [this](Value &&args) {
		if (!_table || !_table->getScroll()) {
			return ackValue(false);
		}
		auto scroll = _table->getScroll();
		scroll->setScrollPosition(scroll->getScrollPosition()
				+ float(static_cast<const Value &>(args).getInteger("delta")));
		return ackValue(true);
	});

	addCommand("refuse", "Make every commit be refused, or stop: {value}", [this](Value &&args) {
		_refuse = static_cast<const Value &>(args).getBool("value");
		return ackValue(true);
	});

	addCommand("close-on-scroll", "What the next cell editor is opened with: {value}",
			[this](Value &&args) {
		_closeOnScroll = static_cast<const Value &>(args).getBool("value");
		return ackValue(true);
	});

	addCommand("detach-table", "Take the table out of the scene under an open cell editor",
			[this](Value &&) {
		if (!_table) {
			return ackValue(false);
		}
		// The anchor leaving the scene: what happens when a whole panel is closed while one of its
		// cells is being edited.
		_table->removeFromParent(true);
		_table = nullptr;
		return ackValue(true);
	});

	/* The engine seam an inline editor over a virtualized row depends on: ask the view to report
	when its row nodes are current, instead of watching for it every frame. */
	addCommand("rebuild-callback", "Ask the table to report when its rows are rebuilt: {row}",
			[this](Value &&args) {
		if (!_table) {
			return ackValue(false);
		}
		_rebuildWatchRow =
				size_t(sprt::max(static_cast<const Value &>(args).getInteger("row"), int64_t(0)));
		_rebuildRowWidth = 0.0f;
		_table->requestRebuildNodes([this] {
			++_rebuildAnswers;
			Rect rect;
			if (getCellRect(_rebuildWatchRow, rect)) {
				_rebuildRowWidth = rect.size.width;
			}
		});
		return ackValue(true);
	});

	addCommand("overlays", "How many overlays the scene content is holding", [this](Value &&) {
		Value ret;
		auto content = dynamic_cast<basic2d::SceneContent2d *>(
				getScene() ? getScene()->getContent() : nullptr);
		ret.setInteger(content ? int64_t(content->getOverlays().size()) : -1, "count");
		return ret;
	});

	addCommand("focus-neighbour", "Put the caret in the field beside the editors",
			[this](Value &&) {
		if (!_neighbour) {
			return ackValue(false);
		}
		_neighbour->focus();
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero the ending counters", [this](Value &&) {
		_commits = _cancels = _closes = _refusals = 0;
		_rebuildAnswers = 0;
		_lastCommit.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
