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
	if (_neighbour) {
		_neighbour->setAnchorPoint(Vec2(0.0f, 1.0f));
		_neighbour->setPosition(Vec2(48.0f, top - 50.0f));
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

	addCommand("begin", "Open an editor: {target=label|cell, row}", [this](Value &&args) {
		const Value &a = args;
		if (a.getString("target") == "cell") {
			return ackValue(beginCellEdit(size_t(a.getInteger("row"))));
		}
		return ackValue(beginLabelEdit());
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
		return ackValue(false);
	});

	addCommand("cancel", "Cancel the open editor", [this](Value &&) {
		if (_cellSession && _cellSession->isOpen()) {
			_cellSession->cancel();
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
		_lastCommit.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
