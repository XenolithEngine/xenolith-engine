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

#include "XLUiTextInput.h"
#include "XLDropTarget.h"
#include "XLAction.h"
#include "XLDirector.h"
#include "XLInheritedStyle.h" // the colour a Label actually paints with
#include "XLUiTextDocument.h" // TextDocument::diff - what the platform echo changed
#include "XLSelectionSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr uint32_t TextInputAdjustTag = "XLUiTextInputAdjust"_tag;
static constexpr uint32_t TextInputBlinkTag = "XLUiTextInputBlink"_tag;

// Half a blink period.
static constexpr float TextInputBlinkHalfPeriod = 0.53f;

static constexpr float TextInputCaretWidth = 1.5f;

// how far past the viewport edge the caret may travel before the label is slid back
static constexpr float TextInputScrollMargin = 60.0f;

// pixels per second while a drag-selection pulls the text past an edge
static constexpr float TextInputAutoScrollSpeed = 300.0f;

// Hold time before selecting the word, and again before widening to the whole text.
static constexpr TimeInterval TextInputLongPressInterval = TimeInterval::milliseconds(500);

// The press recognizer counts intervals in update(), which needs frames even while nothing
// on screen changes.
static constexpr uint32_t TextInputPressTag = "XLUiTextInputPress"_tag;

void TextInput::registerStyleAppliers(StringView type) {
	using document::ParameterName;

	// One registration per type. App-thread only, so a plain set is enough.
	static Set<String> s_registered;
	if (!s_registered.emplace(type.str<mem_std::Interface>()).second) {
		return;
	}

	StyleResolver::registerTypeApplier(type,
			[](StyleResolver &res, Node *node, const ResolvedStyle &s, document::ParameterName name,
					const document::StyleValue &val) {
		if (auto input = dynamic_cast<TextInput *>(node)) {
			return input->setStyleValue(s, name, val);
		}
		return false;
	},
			StyleResolver::makeParameterMask({
				ParameterName::CssBackgroundColor,
				ParameterName::CssOutlineColor,
				ParameterName::CssOutlineWidth,
				ParameterName::CssOutlineStyle,
				ParameterName::CssBorderTopLeftRadius,
				ParameterName::CssBorderTopRightRadius,
				ParameterName::CssBorderBottomRightRadius,
				ParameterName::CssBorderBottomLeftRadius,
				ParameterName::CssPaddingTop,
				ParameterName::CssPaddingRight,
				ParameterName::CssPaddingBottom,
				ParameterName::CssPaddingLeft,
				ParameterName::CmdReset,
			}));
}

ComponentId TextInputStyleComponent::Id;

// The frame-stack tag the style watch publishes itself under; see TextInput::init
static constexpr uint64_t TextInputStyleWatchTag = "XLUiTextInputStyleWatch"_tag;

TextInputContainer::~TextInputContainer() { }

bool TextInputContainer::init() {
	if (!Node::init()) {
		return false;
	}

	// The text a person typed. The placeholder below is a caption and keeps its tags.
	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(0));
	_label->setLocaleEnabled(false);
	_label->setAnchorPoint(Anchor::BottomLeft);
	_label->setType("label");
	_label->addStyleClass("xl-ui-text-input-label");

	_placeholder = addChild(Rc<basic2d::Label>::create(), ZOrder(0));
	_placeholder->setAnchorPoint(Anchor::BottomLeft);
	_placeholder->setType("label");
	_placeholder->addStyleClass("xl-ui-text-input-placeholder");
	_placeholder->setVisible(false);

	// a child of the label, so it rides the horizontal-overflow offset for free and
	// Label::getCursorPosition() is already in its parent space
	_caret = _label->addChild(Rc<basic2d::Layer>::create(Color::White), ZOrder(1));
	_caret->setAnchorPoint(Anchor::BottomLeft);
	_caret->setVisible(false);

	/* A label's height settles in its own content-size phase, after this node's hooks, so centring
	is driven from there. That runs before the model matrix is rebuilt, within the same frame. */
	_label->setContentSizeDirtyCallback([this] { updateLabelPosition(); });
	_placeholder->setContentSizeDirtyCallback([this] { updateLabelPosition(); });

	// ApplyForAll, not ApplyForNodesBelow: "below" means negative z-order (Node::wrapVisit), and
	// the labels sit at ZOrder(0).
	// 2px of horizontal bleed so a caret at position 0 or at the very end is not shaved off.
	_scissor = addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
	_scissor->enableScissor(Padding(0.0f, 2.0f));

	return true;
}

void TextInputContainer::update(const UpdateTime &time) {
	Node::update(time);

	// Vec2::INVALID is a pair of NaNs: test with isValid(), never == Vec2::INVALID.
	if (!_autoScrollTarget.isValid() || !hasHorizontalOverflow()) {
		return;
	}

	// A drag parked outside the box keeps pulling the text; with no gesture events, the clock
	// drives it.
	const auto width = _contentSize.width;
	const auto edge = sprt::min(48.0f, width / 3.0f);
	const auto xPos = convertToNodeSpace(_autoScrollTarget).x;
	const auto labelWidth = _label->getContentSize().width;
	const auto labelPos = _label->getPosition().x;

	if (xPos < edge) {
		const float rel = 1.0f - math::clamp(xPos / edge, 0.0f, 1.0f);
		_label->setPositionX(sprt::min(0.0f, labelPos + rel * TextInputAutoScrollSpeed * time.dt));
	} else if (xPos > width - edge) {
		const float rel = 1.0f - math::clamp((width - xPos) / edge, 0.0f, 1.0f);
		_label->setPositionX(
				sprt::max(width - labelWidth, labelPos - rel * TextInputAutoScrollSpeed * time.dt));
	}
}

void TextInputContainer::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_placeholder->setPositionX(0.0f);
	updateLabelPosition();

	// The caret spans the whole inner box of the field; the font height is only a fallback until
	// the box has a size.
	_caret->setContentSize(Size2(TextInputCaretWidth,
			_contentSize.height > 0.0f ? _contentSize.height : float(_label->getFontHeight())));
	_caretDirty = true;
}

bool TextInputContainer::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	// Deferred: setCursor/handleLabelChanged only mark, so several mutations in a frame cost one
	// recomputation.
	if (_caretDirty) {
		updateCaretPosition();
		_caretDirty = false;
	}

	return Node::visitDraw(frame, parentFlags);
}

void TextInputContainer::setEnabled(bool value) {
	if (value == _enabled) {
		return;
	}
	_enabled = value;
	updateCaretBlink();
}

void TextInputContainer::setCursor(TextCursor cursor, uint32_t activePosition) {
	// without a selection there is only one end, and an unknown moving end follows the start of
	// the range
	if (cursor.length == 0 || activePosition == maxOf<uint32_t>()) {
		activePosition = cursor.start;
	}

	if (_cursor == cursor && _cursorActive == activePosition) {
		return;
	}

	if (_cursor == cursor) {
		// same range, other end moving: only what the viewport follows has changed
		_cursorActive = activePosition;
		_caretDirty = true;
		return;
	}

	_cursor = cursor;
	_cursorActive = activePosition;
	_caretDirty = true;

	// the selection highlight is drawn by the Label itself (Label::Selection), so this is the whole
	// implementation of "show a selection"
	_label->setSelectionCursor(_cursor.length > 0 ? _cursor : TextCursor::InvalidCursor);
	updateCaretBlink();
}

void TextInputContainer::setMarked(TextCursor marked) {
	if (_marked == marked) {
		return;
	}
	_marked = marked;
	_label->setMarkedCursor(_marked.length > 0 ? _marked : TextCursor::InvalidCursor);
}

void TextInputContainer::setCaretColor(const Color4F &color) { _caret->setColor(color); }

void TextInputContainer::setSelectionColor(const Color4F &color) {
	_label->setSelectionColor(color);
}

void TextInputContainer::setMarkedColor(const Color4F &color) { _label->setMarkedColor(color); }

void TextInputContainer::setCaretBlink(bool value) {
	if (value == _caretBlink) {
		return;
	}
	_caretBlink = value;
	updateCaretBlink();
}

void TextInputContainer::setReadOnly(bool value) {
	if (value == _readOnly) {
		return;
	}
	_readOnly = value;
	updateCaretBlink();
}

void TextInputContainer::setPlaceholderVisible(bool value) { _placeholder->setVisible(value); }

void TextInputContainer::handleLabelChanged() { _caretDirty = true; }

TextCursor TextInputContainer::getCursorForPosition(const Vec2 &loc, font::CharSelectMode mode) {
	if (_label->empty()) {
		return TextCursor(0);
	}

	auto idx = _label->getCharIndex(_label->convertToNodeSpace(loc), mode);
	if (idx.first == maxOf<uint32_t>()) {
		// past the end of the text on the same line: put the caret at whichever end is nearer
		auto local = _label->convertToNodeSpace(loc);
		return TextCursor(local.x <= 0.0f ? 0u : uint32_t(_label->getCharsCount()));
	}
	return TextCursor(idx.second ? idx.first + 1 : idx.first);
}

bool TextInputContainer::hasHorizontalOverflow() const {
	return _label->getContentSize().width > _contentSize.width;
}

void TextInputContainer::moveHorizontalOverflow(float d) {
	_label->stopAllActionsByTag(TextInputAdjustTag);

	const auto minPos = _contentSize.width - _label->getContentSize().width;
	_label->setPositionX(math::clamp(_label->getPosition().x + d, sprt::min(minPos, 0.0f), 0.0f));
}

float TextInputContainer::getLabelOffset() const { return _label->getPosition().x; }

void TextInputContainer::setAutoScrollTarget(const Vec2 &worldLocation) {
	// NaN != NaN, so "already stopped" is tested through isValid(); otherwise a stop request would
	// take the start branch below
	if (_autoScrollTarget == worldLocation
			|| (!_autoScrollTarget.isValid() && !worldLocation.isValid())) {
		return;
	}

	_autoScrollTarget = worldLocation;
	if (!_autoScrollTarget.isValid()) {
		if (_scheduled) {
			unscheduleUpdate();
		}
		stopAllActionsByTag("RenderContinuously"_tag);
	} else {
		scheduleUpdate();
		if (!getActionByTag("RenderContinuously"_tag)) {
			runAction(Rc<RenderContinuously>::create(), "RenderContinuously"_tag);
		}
	}
}

void TextInputContainer::updateLabelPosition() {
	/* A single line is centred vertically in the box. max(0): a line taller than the box starts
	at the top and overflows downwards into the scissor. */
	const auto centre = [&](basic2d::Label *label) {
		label->setPositionY(
				sprt::max(0.0f, (_contentSize.height - label->getContentSize().height) / 2.0f));
	};

	centre(_label);
	centre(_placeholder);

	// The caret is positioned relative to the label, its parent. Its vertical origin is written
	// here so it does not lag the text by a frame; the horizontal part is flushed in visitDraw.
	_caret->setPositionY(-_label->getPosition().y);
	_caretDirty = true;
}

void TextInputContainer::updateCaretPosition() {
	const auto cpos =
			_label->empty() ? _label->getCursorOrigin() : _label->getCursorPosition(_cursorActive);

	// Only the horizontal position comes from the label; vertically the caret is pinned to the
	// bottom of the container, whose height it has. The caret is a child of the label, so the
	// container's bottom edge is at -label.y in this space.
	_caret->setPosition(Vec2(cpos.x, -_label->getPosition().y));

	// While a drag-selection pulls the text past an edge, the pointer owns the offset (see update).
	if (_autoScrollTarget.isValid()) {
		return;
	}

	const auto labelWidth = _label->getContentSize().width;
	const auto width = _contentSize.width;

	if (labelWidth <= width) {
		runAdjustLabel(0.0f);
		return;
	}

	// The caret must stay inside a margin from both edges; when it crosses one, re-centre the text
	// around it. Clamped so the text never leaves a gap at either end.
	const auto minPos = width - sprt::max(labelWidth, cpos.x);
	const auto margin = sprt::min(width / 4.0f, TextInputScrollMargin);
	const auto inContainer = _label->getNodeToParentTransform().transformPoint(cpos);
	if (inContainer.x < margin || inContainer.x > width - margin) {
		runAdjustLabel(math::clamp(width / 2.0f - cpos.x, minPos, 0.0f));
	}
}

void TextInputContainer::updateCaretBlink() {
	_caret->stopAllActionsByTag(TextInputBlinkTag);

	const bool visible = isEnabled() && !_readOnly && _cursor.length == 0;
	_caret->setVisible(visible);

	if (!visible || !_caretBlink) {
		return;
	}

	// restarted from the visible phase on every change, so the caret is solid while typing
	auto caret = _caret;
	_caret->runAction(Rc<RepeatForever>::create(Rc<Sequence>::create(TextInputBlinkHalfPeriod,
							  [caret] { caret->setVisible(false); }, TextInputBlinkHalfPeriod,
							  [caret] { caret->setVisible(true); })),
			TextInputBlinkTag);
}

void TextInputContainer::runAdjustLabel(float pos) {
	if (_label->getPosition().x == pos) {
		_label->stopAllActionsByTag(TextInputAdjustTag);
		return;
	}

	_label->stopAllActionsByTag(TextInputAdjustTag);

	// short hops snap, long ones glide
	const auto dist = sprt::fabs(_label->getPosition().x - pos);
	const float minT = 0.05f;
	const float maxT = 0.35f;
	float t = minT;
	if (dist > 80.0f) {
		t = maxT;
	} else if (dist > 16.0f) {
		t = progress(minT, maxT, (dist - 16.0f) / 64.0f);
	}

	_label->runAction(
			Rc<EaseActionTyped>::create(Rc<MoveTo>::create(t, Vec2(pos, _label->getPosition().y)),
					interpolation::Type::QuadEaseInOut),
			TextInputAdjustTag);
}

TextInput::~TextInput() { }

bool TextInput::init() {
	if (!VectorSprite::init()) {
		return false;
	}

	/* The InteractiveComponent must exist from the start: without one the state reads as 0, so
	`:disabled` would match and isEnabled() would report false. */
	applyControlEnabled(this, true);

	registerStyleAppliers("text-input");

	/* Notified when the label's inherited style arrives, which is after this node's own components
	phase. The caret and selection colours derive from the label's text colour. The system
	publishes itself on the frame stack under a tag; the label's components phase delivers to the
	nearest ancestor carrying it. */
	auto styleWatch = addSystem(Rc<CallbackSystem>::create());
	styleWatch->setFrameTag(TextInputStyleWatchTag);
	styleWatch->setChildComponentsDirtyCallback(
			[this](CallbackSystem *, Node *, const ComponentMask &mask) {
		if (mask.contains(InheritedColorStyle::Id.value)) {
			updateStyleColors();
		}
	});
	styleWatch->setSystemFlags(styleWatch->getSystemFlags() | SystemFlags::AddToFrameStack);

	setType("text-input");
	addStyleClass("xl-ui-text-input");
	setRenderingLevel(RenderingLevel::Surface);

	_container = addChild(makeContainer(), ZOrder(1));
	_container->setAnchorPoint(Anchor::BottomLeft);

	_listener = addSystem(Rc<InputListener>::create());

	// Text dropped onto the field is handled like a paste: same type rule, insertion point and
	// validation
	setDropTarget(this,
			DropTargetSlots{
				.accept = [this](const DragEvent &event) -> DragResponse {
		if (isReadOnly() || !event.data) {
			return DragResponse();
		}
		auto want = StringView("text/plain");
		if (event.data->preferType(makeSpanView(&want, 1)).empty()) {
			return DragResponse();
		}
		// Either is fine here: whether the source deletes its original is up to the source
		return DragResponse{event.allowed & (DragActions::Copy | DragActions::Move)};
	},
				.drop = [this](const DragEvent &event,
								DragActions) { return handleTextDrop(event); },
			});

	// A key event carries the last pointer location, so the default hit-test filter would deliver
	// keys only while the mouse hovers the field. Keyboard events bypass the hit test; pointer
	// events keep the default.
	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return _focused;
		}
		return cb(event);
	});

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: _hoverApplied = true; break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: _hoverApplied = false; break;
		default: break;
		}
		updateInteractiveState();
		return true;
	}, false);

	// maxTapCount 3: caret, word, everything. Immediate, since each refines the previous; waiting
	// for a possible second tap would delay every click by TapIntervalAllowed.
	_listener->addTapRecognizer([this](const GestureTap &tap) { return handleTap(tap); },
			InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 3,
				InputTapFlags::Immediate});

	// Continuous, so the hold reports every interval and each one widens the selection.
	_listener->addPressRecognizer(
			[this](const GesturePress &press) {
		switch (press.event) {
		case GestureEvent::Began: return handlePress(press, true); break;
		case GestureEvent::Activated: return handleLongPress(press); break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: return handlePress(press, false); break;
		default: break;
		}
		return false;
	},
			InputPressInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				TextInputLongPressInterval,
				InputPressFlags::Capture | InputPressFlags::Continuous});

	_listener->addSwipeRecognizer([this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began: return handleSwipeBegin(swipe.input->originalLocation); break;
		case GestureEvent::Activated:
			return handleSwipe(swipe.location(), swipe.delta / swipe.density);
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: return handleSwipeEnd(); break;
		}
		return false;
	}, InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft})});

	/* Cursor movement stays on a key recognizer (Shift only adds a flag). Commands - navigation,
	   accept, clipboard chords - are hotkeys, so they can be rebound and precedence against the
	   form's bindings follows the walk order. */
	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::LEFT));
	keys.set(toInt(InputKeyCode::RIGHT));
	keys.set(toInt(InputKeyCode::UP));
	keys.set(toInt(InputKeyCode::DOWN));
	keys.set(toInt(InputKeyCode::HOME));
	keys.set(toInt(InputKeyCode::END));
	_listener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	auto &hk = EngineHotkeys::get();
	auto bind = [this](HotkeyId id) {
		_listener->addHotkey(id, [this](HotkeyId id, const InputEvent &ev) {
			return handleTextHotkey(id, ev);
		}, HotkeyFlags::FocusedOnly | HotkeyFlags::Repeatable);
	};

	bind(hk.focusNext);
	bind(hk.focusPrev);
	bind(hk.textAccept);
	bind(hk.textAcceptKeypad);
	bind(hk.textSelectAll);
	bind(hk.textCopy);
	bind(hk.textCut);
	bind(hk.textPaste);
	bind(hk.undo);
	bind(hk.redo);
	bind(hk.redoAlt);

	_listener->setCursor(WindowCursor::Text);

	// Tap outside the field releases input. Priority 1 puts it above the scene graph, and its touch
	// filter accepts only points outside the widget.
	_focusListener = addSystem(Rc<InputListener>::create());
	_focusListener->setPriority(1);
	_focusListener->addTapRecognizer([this](const GestureTap &) {
		blur();
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_focusListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return !isTouched(event.currentLocation, 8.0f);
	});
	_focusListener->setEnabled(false);

	_handler.onData = sprt::bind(&TextInput::handleTextInput, this, sprt::placeholders::_1);

	// Always built, disabled by default; TextView enables it.
	_history.init(this);


	return true;
}

void TextInput::handleEnter(Scene *scene) {
	VectorSprite::handleEnter(scene);
	updateInteractiveState();
}

void TextInput::handleExit() {
	// Also done by the handler's destructor, but a node can leave the scene and come back.
	if (_handler.isActive()) {
		_handler.cancel();
	}
	VectorSprite::handleExit();
}

void TextInput::handleContentSizeDirty() {
	VectorSprite::handleContentSizeDirty();

	updateBackgroundImage();

	TextInputStyleComponent defaultStyle;
	const TextInputStyleComponent *style = &defaultStyle;
	if (auto c = getComponent<TextInputStyleComponent>()) {
		style = c;
	}

	// Room a subclass has taken out of the viewport (see getViewportInset); this stays the single
	// writer of the container's geometry.
	const auto inset = getViewportInset();

	const auto width =
			sprt::max(_contentSize.width - style->padding.horizontal() - inset.horizontal(), 0.0f);
	const auto height =
			sprt::max(_contentSize.height - style->padding.vertical() - inset.vertical(), 0.0f);

	_container->setContentSize(Size2(width, height));
	_container->setPosition(
			Vec2(style->padding.left + inset.left, style->padding.bottom + inset.bottom));
}

void TextInput::handleComponentsDirty(const ComponentMask &mask) {
	VectorSprite::handleComponentsDirty(mask);

	// The inherited colour as well as the field's own paint: caret and selection derive from the
	// text colour (InheritedColorStyle). An ancestor change is seen because the resolver rewrites
	// the component on this node too (XLInheritedStyle.h)
	if (mask.contains(TextInputStyleComponent::Id.value)
			|| mask.contains(InheritedColorStyle::Id.value)) {
		updateStyleColors();
	}
}

void TextInput::updateBackgroundImage() {
	if (_contentSize.width <= 0.0f || _contentSize.height <= 0.0f) {
		return;
	}

	// same shape as ui::Button: the resolved paint lives in a component, and an unstyled field
	// falls back to the struct's own defaults
	TextInputStyleComponent defaultStyle;
	const TextInputStyleComponent *style = &defaultStyle;
	if (auto c = getComponent<TextInputStyleComponent>()) {
		style = c;
	}

	auto image = Rc<VectorImage>::create(_contentSize);

	// inset the rect by half the stroke width so the outline is not clipped at the node's edges
	const float inset = style->outlineWidth > 0.0f ? style->outlineWidth * 0.5f : 0.0f;
	const Rect box(inset, inset, _contentSize.width - inset * 2.0f,
			_contentSize.height - inset * 2.0f);

	// shrink each corner radius by the inset so the outer edge of the stroke keeps the requested
	// radius; addBox() itself clamps each corner to the half-box
	auto outer = [&](float r) { return r > 0.0f ? sprt::max(r - inset, 0.0f) : 0.0f; };
	const float rtl = outer(style->borderRadiusTopLeft);
	const float rtr = outer(style->borderRadiusTopRight);
	const float rbr = outer(style->borderRadiusBottomRight);
	const float rbl = outer(style->borderRadiusBottomLeft);
	const bool rounded = rtl > 0.0f || rtr > 0.0f || rbr > 0.0f || rbl > 0.0f;

	auto path = image->addPath();
	path->openForWriting([&](PathWriter &writer) {
		if (rounded) {
			writer.addBox(box.origin.x, box.origin.y, box.size.width, box.size.height,
					/* addBox TL = visual bottom-left  */ rbl,
					/* addBox TR = visual bottom-right */ rbr,
					/* addBox BR = visual top-right    */ rtr,
					/* addBox BL = visual top-left     */ rtl);
		} else {
			writer.addRect(box);
		}
	})
			.setFillColor(style->backgroundColor)
			.setStyle(vg::DrawFlags::Fill);

	if (style->outlineWidth > 0.0f && style->outlineStyle != document::BorderStyle::None) {
		path->setStyle(vg::DrawFlags::FillAndStroke)
				.setStrokeColor(style->outlineColor)
				.setStrokeWidth(style->outlineWidth)
				.setAntialiased(true);

		// Same dash proportions as Panel - see the note there.
		const float w = style->outlineWidth;
		switch (style->outlineStyle) {
		case document::BorderStyle::Dashed: {
			const float dashes[] = {w * 3.0f, w * 2.0f};
			path->setDashArray(SpanView<float>(dashes, 2));
			break;
		}
		case document::BorderStyle::Dotted: {
			const float dots[] = {0.0f, w * 2.0f};
			path->setLineCup(vg::LineCup::Round).setDashArray(SpanView<float>(dots, 2));
			break;
		}
		default: break;
		}
	}

	setImage(sp::move(image));
}

void TextInput::updateStyleColors() {
	// A field matched by no rule has no component and still needs a visible caret, so everything
	// below works without one
	auto style = getComponent<TextInputStyleComponent>();

	/* The colour the label paints with (inherited-style components, see XLInheritedStyle.h), not
	Node::getColor(), which stays at the widget's default. */
	auto label = _container->getLabel();
	auto text = Color4F(label->getColor());
	const auto inherited = accumulateInheritedStyle<InheritedColorStyle>(label);
	if (inherited.defined & InheritedColorStyle::DefinedColor) {
		text = Color4F(inherited.color);
	}

	// Fallbacks derive from that text colour (the selection dimmed). A colour set by
	// --caret-color / --selection-color / --marked-color always wins.
	_container->setCaretColor(style && style->hasCaretColor ? Color4F(style->caretColor) : text);

	auto selection = text;
	selection.a = 0.35f;
	_container->setSelectionColor(
			style && style->hasSelectionColor ? Color4F(style->selectionColor) : selection);

	auto marked = text;
	marked.a = 0.5f;
	_container->setMarkedColor(
			style && style->hasMarkedColor ? Color4F(style->markedColor) : marked);
}

bool TextInput::setStyleValue(const ResolvedStyle &style, document::ParameterName name,
		const document::StyleValue &value) {
	using document::ParameterName;

	// CmdReset arrives before the parameters of every style pass; dropping the component is how a
	// rule that stopped matching is noticed. Custom properties are not delivered as parameters, so
	// they are re-read here.
	if (name == ParameterName::CmdReset) {
		bool changed = removeComponent<TextInputStyleComponent>();

		Color4B caret;
		Color4B selection;
		Color4B marked;
		const bool hasCaret =
				sprt::geom::readColor(style.getCustomProperty("--caret-color"), caret);
		const bool hasSelection =
				sprt::geom::readColor(style.getCustomProperty("--selection-color"), selection);
		const bool hasMarked =
				sprt::geom::readColor(style.getCustomProperty("--marked-color"), marked);

		if (hasCaret || hasSelection || hasMarked) {
			setOrUpdateComponent<TextInputStyleComponent>([&](NotNull<TextInputStyleComponent> c) {
				c->hasCaretColor = hasCaret;
				c->caretColor = caret;
				c->hasSelectionColor = hasSelection;
				c->selectionColor = selection;
				c->hasMarkedColor = hasMarked;
				c->markedColor = marked;
				return true;
			});
			changed = true;
		} else {
			updateStyleColors();
		}

		if (changed) {
			markContentSizeDirty();
		}
		return true;
	}

	bool known = true;
	bool changed = false;
	setOrUpdateComponent<TextInputStyleComponent>([&](NotNull<TextInputStyleComponent> c) {
		// raw px magnitude of the metric (em/% are not resolved here)
		const float px = value.sizeValue.value;
		switch (name) {
		case ParameterName::CssBackgroundColor:
			changed = c->backgroundColor != value.color4;
			c->backgroundColor = value.color4;
			break;
		case ParameterName::CssOutlineColor:
			changed = c->outlineColor != value.color4;
			c->outlineColor = value.color4;
			break;
		case ParameterName::CssOutlineWidth:
			changed = c->outlineWidth != px;
			c->outlineWidth = px;
			break;
		case ParameterName::CssOutlineStyle:
			changed = c->outlineStyle != value.borderStyle;
			c->outlineStyle = value.borderStyle;
			break;
		case ParameterName::CssBorderTopLeftRadius:
			changed = c->borderRadiusTopLeft != px;
			c->borderRadiusTopLeft = px;
			break;
		case ParameterName::CssBorderTopRightRadius:
			changed = c->borderRadiusTopRight != px;
			c->borderRadiusTopRight = px;
			break;
		case ParameterName::CssBorderBottomRightRadius:
			changed = c->borderRadiusBottomRight != px;
			c->borderRadiusBottomRight = px;
			break;
		case ParameterName::CssBorderBottomLeftRadius:
			changed = c->borderRadiusBottomLeft != px;
			c->borderRadiusBottomLeft = px;
			break;
		case ParameterName::CssPaddingTop:
			changed = c->padding.top != px;
			c->padding.top = px;
			break;
		case ParameterName::CssPaddingRight:
			changed = c->padding.right != px;
			c->padding.right = px;
			break;
		case ParameterName::CssPaddingBottom:
			changed = c->padding.bottom != px;
			c->padding.bottom = px;
			break;
		case ParameterName::CssPaddingLeft:
			changed = c->padding.left != px;
			c->padding.left = px;
			break;
		default: known = false; break;
		}
		return changed;
	});

	if (!known) {
		slog().warn("ui::TextInput", "Unknown style parameter: ", name);
		return false;
	}
	if (changed) {
		markContentSizeDirty();
	}
	return true;
}

void TextInput::setText(StringView str) {
	size_t size = sprt::unicode::getUtf16Length(str);
	WideString wide;
	wide.resize(size);
	sprt::unicode::toUtf16(wide.data(), wide.size(), str, &size);
	wide.resize(size);
	setText(WideStringView(wide));
}

void TextInput::setText(WideStringView str) {
	auto string = TextInputString::create(str);
	auto cursor = TextCursor(uint32_t(str.size()));

	if (_handler.isActive()) {
		// there is a platform authority: ask, do not tell (see the class comment)
		pushRequest(string, cursor);
		return;
	}

	// No handler running: no echo will come, so _inputState is written directly.
	_inputState.string = string;
	_inputState.cursor = cursor;
	_inputState.marked = TextCursor::InvalidCursor;
	_inputState.type = _inputType;
	_pendingCursor = cursor;

	_container->setCursor(cursor);
	_container->setMarked(TextCursor::InvalidCursor);
	_container->handleLabelChanged();
	_container->setPlaceholderVisible(_inputState.empty() && !_focused);
	updateDisplayString();

	if (_callback) {
		_callback(getText());
	}
}

StringView TextInput::getText() const {
	_textCache = string::toUtf8<Interface>(_inputState.getStringView());
	return _textCache;
}

WideStringView TextInput::getDisplayText() const { return _container->getLabel()->getString(); }

void TextInput::setPlaceholder(StringView str) {
	_placeholderText = str.str<Interface>();
	_container->getPlaceholder()->setString(str);
	_container->setPlaceholderVisible(_inputState.empty() && !_focused);
}

StringView TextInput::getPlaceholder() const { return _placeholderText; }

void TextInput::setReadOnly(bool value) {
	/* Under a lock the visible bit is the lock's, so it is not used to skip: the request is still
	recorded for unlock. */
	if (!isEditLocked(this) && value == isReadOnly()) {
		return;
	}
	applyControlReadOnly(this, value);
	_container->setReadOnly(value);

	/* Read-only is separate from disabled: a read-only field still takes taps and selections so its
	text can be copied. Exposed as `:read-only` and as a plain class for older sheets. */
	if (isReadOnly() && _handler.isActive()) {
		_handler.cancel();
	}
}

void TextInput::setCallback(ChangeCallback &&cb) { _callback = sp::move(cb); }

void TextInput::setEnterCallback(EnterCallback &&cb) { _enterCallback = sp::move(cb); }

void TextInput::setNavigateCallback(NavigateCallback &&cb) { _navigateCallback = sp::move(cb); }

void TextInput::insertText(WideStringView text, TextCursor replace) {
	const auto str = _inputState.getStringView();
	const auto size = uint32_t(str.size());

	auto start = sprt::min(replace.start, size);
	auto length = sprt::min(replace.length, size - start);

	WideString result;
	result.reserve(str.size() - length + text.size());
	result.append(str.data(), start);
	result.append(text.data(), text.size());
	result.append(str.data() + start + length, str.size() - start - length);

	auto string = TextInputString::create(WideStringView(result));
	auto cursor = TextCursor(start + uint32_t(text.size()));

	if (_handler.isActive()) {
		pushRequest(string, cursor);
		return;
	}

	// Same reasoning as setText(): with no handler there is no platform authority to defer to
	_inputState.string = string;
	_inputState.cursor = cursor;
	_inputState.marked = TextCursor::InvalidCursor;
	_pendingCursor = cursor;

	_container->setCursor(cursor);
	_container->setMarked(TextCursor::InvalidCursor);
	_container->handleLabelChanged();
	_container->setPlaceholderVisible(_inputState.empty() && !_focused);
	updateDisplayString();

	if (_callback) {
		_callback(getText());
	}
}

WideStringView TextInput::getTextForCursor(TextCursor cursor) const {
	return WideStringView(_inputState.getStringView(), cursor.start, cursor.length);
}

ClipboardSession *TextInput::acquireClipboard() {
	if (!_clipboard && _director) {
		_clipboard = Rc<ClipboardSession>::create(_director->getApplication());
	}
	return _clipboard;
}

bool TextInput::copy() {
	const auto cursor = selectionCursor();
	if (cursor.length == 0 || !_director) {
		return false;
	}

	// A masked field's contents must not leave the widget
	if (!canCopySelection()) {
		return false;
	}

	auto clipboard = acquireClipboard();
	if (!clipboard) {
		return false;
	}

	// The offer copies the bytes. False means the transport cannot carry it (a remote client);
	// cut() checks this before deleting
	auto str = string::toUtf8<Interface>(getTextForCursor(cursor));
	return clipboard->writeText(str) == Status::Ok;
}

bool TextInput::cut() {
	if (isReadOnly() || !copy()) {
		return false;
	}
	// What was selected, not where the caret is heading: see selectionCursor()
	HistoryEditName name(this, TextHistory::NameCut);
	insertText(WideStringView(), selectionCursor());
	return true;
}

bool TextInput::paste() {
	if (isReadOnly() || !_director) {
		return false;
	}

	auto clipboard = acquireClipboard();
	if (!clipboard) {
		return false;
	}

	// Staleness, type negotiation and single delivery are handled by the session
	return clipboard->readText([this](const ClipboardSession::Result &result) {
		if (!result) {
			return;
		}

		auto text = string::toUtf16<Interface>(result.text());

		// The caret as it is now, not when the read started. Length and character filtering
		// happen in validateInput() on the echo
		HistoryEditName name(this, TextHistory::NamePaste);
		insertText(WideStringView(text), insertionCursor());
	}, this) != 0;
}

bool TextInput::handleTextDrop(const DragEvent &event) {
	if (isReadOnly() || !event.data) {
		return false;
	}

	// The same type rule as paste
	auto want = StringView("text/plain");
	auto type = event.data->preferType(makeSpanView(&want, 1));
	if (type.empty()) {
		return false;
	}

	auto bytes = event.data->encode(type);
	if (bytes.empty()) {
		return false;
	}

	auto text = string::toUtf16<Interface>(
			StringView(reinterpret_cast<const char *>(bytes.data()), bytes.size()));

	// the caret as it is now, as a paste does; filtering happens in validateInput()
	HistoryEditName name(this, TextHistory::NameDrop);
	insertText(WideStringView(text), insertionCursor());
	return true;
}

void TextInput::focus() {
	if (!isEnabled() || isReadOnly() || _handler.isActive()) {
		return;
	}
	acquireInput(TextCursor(uint32_t(_inputState.size())));
}

void TextInput::blur() {
	if (_handler.isActive()) {
		_handler.cancel();
	}
	// A paste in flight belongs to the focus that started it
	if (_clipboard) {
		_clipboard->cancel();
	}
	_focusListener->setEnabled(false);
}

void TextInput::selectAll() {
	const auto count = uint32_t(_inputState.size());
	if (count == 0) {
		return;
	}
	setCursor(TextCursor(0u, count));
}

void TextInput::setEnabled(bool value) {
	// The edit lock overrides the request and remembers it for unlock.
	value = resolveEditLock(this, value);
	if (value == isEnabled()) {
		return;
	}

	applyControlEnabled(this, value);
	if (!value) {
		blur();
	}
}

void TextInput::setInputType(TextInputType type) {
	if (type == _inputType) {
		return;
	}
	_inputType = type;
	if (_handler.isActive()) {
		pushRequest(_inputState.cursor, _inputState.marked);
	}
}

void TextInput::setPasswordMode(TextInputPasswordMode mode) {
	if (mode == _passwordMode) {
		return;
	}
	_passwordMode = mode;

	// the password bit is part of what the OS is told, so the type follows the mode
	if (_passwordMode == TextInputPasswordMode::NotPassword) {
		_inputType &= ~TextInputType::PasswordBit;
	} else {
		_inputType |= TextInputType::PasswordBit;
	}

	updateDisplayString();
	if (_handler.isActive()) {
		pushRequest(_inputState.cursor, _inputState.marked);
	}
}

void TextInput::setMaxChars(size_t value) {
	if (value == _maxChars) {
		return;
	}
	_maxChars = value;

	// Enforced through the echo correction path, the single place that truncates.
	if (_maxChars > 0 && _inputState.size() > _maxChars) {
		auto state = _inputState;
		if (validateInput(state)) {
			handleTextInput(state);
			if (_handler.isActive()) {
				pushRequest(state.string, state.cursor, state.marked);
			}
		}
	}
}

void TextInput::setCursor(TextCursor cursor) {
	if (_handler.isActive()) {
		pushRequest(cursor);
		return;
	}

	_inputState.cursor = cursor;
	_pendingCursor = cursor;
	_container->setCursor(cursor, activeCursorPosition(cursor));
}

void TextInput::setCaretBlink(bool value) { _container->setCaretBlink(value); }

bool TextInput::isCaretBlink() const { return _container->isCaretBlink(); }

basic2d::Label *TextInput::getLabel() const { return _container->getLabel(); }

Rc<TextInputContainer> TextInput::makeContainer() { return Rc<TextInputContainer>::create(); }

void TextInput::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The counters are cumulative (several sources can hold the same flag), so each is pushed
		// only when the widget's own contribution flips.
		bool changed = false;
		const bool hover = _hoverApplied;
		const bool focus = _focused;
		const bool active = _activeApplied;

		if (hover != (state->hoverCounter > 0)) {
			changed |= state->handleHover(hover ? 1 : -1);
		}
		if (focus != (state->focusCounter > 0)) {
			changed |= state->handleFocus(focus ? 1 : -1);
		}
		if (active != (state->activeCounter > 0)) {
			changed |= state->handleActive(active ? 1 : -1);
		}
		return changed;
	});
}

void TextInput::acquireInput(TextCursor cursor) {
	if (!_director) {
		return;
	}

	_container->setCursor(cursor);
	_pendingCursor = cursor;
	_handler.run(_director->getTextInputManager(),
			TextInputRequest{
				.string = _inputState.string,
				.cursor = cursor,
				.marked = TextCursor::InvalidCursor,
				.type = _inputType,
			});
	_focusListener->setEnabled(true);
}

void TextInput::pushRequest(TextCursor cursor, TextCursor marked) {
	pushRequest(_inputState.string, cursor, marked);
}

void TextInput::pushRequest(TextInputString *string, TextCursor cursor, TextCursor marked) {
	if (!_handler.isActive()) {
		return;
	}

	_pendingCursor = cursor;

	// A request, not a write: the widget renders whatever comes back through handleTextInput.
	_handler.update(TextInputRequest{
		.string = string,
		.cursor = cursor,
		.marked = marked,
		.type = _inputType,
	});
}

void TextInput::handleTextInput(const TextInputState &data) {
	const bool wasFocused = _focused;
	const auto previousString = _inputState.string;
	const auto previousCursor = _inputState.cursor;
	const bool wasComposing = _inputState.marked.length > 0;

	// Focus follows what the platform granted, not what was asked for.
	if (_focused != data.enabled) {
		_focused = data.enabled;
		if (_focused) {
			// where typing goes is also where the selection is (asymmetric: a blur clears nothing)
			if (auto selection = SelectionSystem::findForNode(this)) {
				selection->selectEnclosing(this);
			}
		} else {
			_focusListener->setEnabled(false);
			_selectionAnchor = maxOf<uint32_t>();
			// Cancel a pending paste here too: focus is usually taken by the platform, not
			// released through blur()
			if (_clipboard) {
				_clipboard->cancel();
			}
		}
		updateInteractiveState();
	}

	auto state = data;
	const bool corrected = validateInput(state);

	_inputState = sp::move(state);

	// The echo supersedes whatever the widget had asked for.
	_pendingCursor = _inputState.cursor;

	_container->setEnabled(_focused);
	_container->setCursor(_inputState.cursor, activeCursorPosition(_inputState.cursor));
	_container->setMarked(_inputState.marked);

	const bool stringChanged = _inputState.string != previousString;
	if (stringChanged) {
		/* Every text change arrives here as an echo. The edit is recovered by diffing, which is
		exact because the processor only performs single-range edits. */
		if (_historyEchoes > 0) {
			// A history-driven edit coming back; not recorded again.
			--_historyEchoes;
		} else if (_history.isEnabled() && !_history.isApplying()) {
			// The removed text comes from the previous string: _inputState already holds the new
			// one, so sliceForHistory() would read the replacement.
			const auto before =
					previousString ? WideStringView(previousString->string) : WideStringView();
			const auto after = _inputState.getStringView();
			const auto d = TextDocument::diff(before, after);
			if (d.removed || d.inserted) {
				_history.recordEdit(d.pos, WideStringView(before.data() + d.pos, d.removed),
						WideStringView(after.data() + d.pos, d.inserted), previousCursor,
						_historyEditName, _historyClock);
			}
		}
		updateDisplayString();

		// The caret an undo restored, applied now that the string it belongs to has arrived.
		if (_historyPendingCursor != TextCursor::InvalidCursor) {
			const auto cursor = _historyPendingCursor;
			_historyPendingCursor = TextCursor::InvalidCursor;
			setCursor(cursor);
		}
	}
	_container->handleLabelChanged();
	_container->setPlaceholderVisible(_inputState.empty() && !_focused);

	if (corrected) {
		// Push the correction back, or the next keystroke would revert it.
		pushRequest(_inputState.string, _inputState.cursor, _inputState.marked);
	}

	// A marked range is a composition in progress, not committed text.
	if (stringChanged && _inputState.marked.length == 0 && _callback) {
		_callback(getText());
	}

	if (wasComposing && _inputState.marked.length == 0 && !stringChanged) {
		// composition committed without changing the string (e.g. unmark of an already-inserted
		// run): still a commit worth reporting
		if (_callback) {
			_callback(getText());
		}
	}

	if (wasFocused && !_focused) {
		_container->setPlaceholderVisible(_inputState.empty());
	}
}

bool TextInput::handleInputChar(char16_t) { return true; }

bool TextInput::validateInput(TextInputState &state) {
	bool changed = false;

	auto str = state.getStringView();

	// Enter and Tab may reach the field as text ('\r' remapped to '\n'); they are stripped here and
	// turned into actions.
	bool hasEnter = false;
	bool hasTab = false;
	WideString filtered;
	filtered.reserve(str.size());
	for (auto c : str) {
		if (c == u'\n' || c == u'\r') {
			hasEnter = true;
			continue;
		}
		if (c == u'\t') {
			hasTab = true;
			continue;
		}
		if (!handleInputChar(c)) {
			changed = true;
			continue;
		}
		filtered.push_back(c);
	}

	if (hasEnter || hasTab || changed) {
		changed = true;
		state.string = TextInputString::create(WideStringView(filtered));
	}

	if (_maxChars > 0 && state.size() > _maxChars) {
		state.string = TextInputString::create(WideStringView(state.getStringView(), 0, _maxChars));
		changed = true;
	}

	if (changed) {
		const auto size = uint32_t(state.size());
		if (state.cursor.start > size) {
			state.cursor.start = size;
			state.cursor.length = 0;
		} else if (state.cursor.start + state.cursor.length > size) {
			state.cursor.length = size - state.cursor.start;
		}
		state.marked = TextCursor::InvalidCursor;
	}

	// deferred to the end so the callbacks see the corrected state, not the raw echo
	if (hasEnter && _enterCallback) {
		_enterCallback();
	}
	if (hasTab) {
		blur();
	}

	return changed;
}

void TextInput::updateDisplayString() {
	auto label = _container->getLabel();

	switch (_passwordMode) {
	case TextInputPasswordMode::NotPassword:
	case TextInputPasswordMode::ShowAll: label->setString(_inputState.getStringView()); break;
	case TextInputPasswordMode::ShowNone: {
		WideString masked;
		masked.resize(_inputState.size(), u'•');
		label->setString(WideStringView(masked));
		break;
	}
	}

	// cursor geometry is read right after this, and it is only valid once the label has re-laid out
	label->tryUpdateLabel();
}

TextCursor TextInput::pendingCursor() const {
	return _pendingCursor == TextCursor::InvalidCursor ? _inputState.cursor : _pendingCursor;
}

uint32_t TextInput::activeCursorPosition(TextCursor cursor) const {
	if (cursor.length == 0 || _selectionAnchor == maxOf<uint32_t>()) {
		// no selection, or one that was made in a single act (a word, select-all): nothing is being
		// dragged, so there is no end to follow
		return maxOf<uint32_t>();
	}

	// the anchor is the end that stays put; the user is moving the other one
	return _selectionAnchor <= cursor.start ? cursor.start + cursor.length : cursor.start;
}

uint32_t TextInput::offsetCursor(int32_t delta) const {
	const auto cursor = pendingCursor();
	const auto size = int64_t(_inputState.size());

	int64_t from = cursor.start;
	if (cursor.length > 0) {
		if (_selectionAnchor == maxOf<uint32_t>()) {
			// nothing is being extended: moving off a selection collapses it to the edge you are
			// moving towards
			return uint32_t(math::clamp(delta < 0 ? cursor.start : cursor.start + cursor.length,
					uint32_t(0), uint32_t(size)));
		}

		// a selection is being extended: the step continues from the end the user is moving, so
		// Shift+Left after a rightwards selection shrinks it instead of jumping to its other end
		from = activeCursorPosition(cursor);
	}

	return uint32_t(math::clamp(from + delta, int64_t(0), size));
}

void TextInput::moveCursor(uint32_t target, bool select) {
	if (select) {
		if (_selectionAnchor == maxOf<uint32_t>()) {
			// anchor the selection at the end the caret is moving away from
			const auto cursor = pendingCursor();
			_selectionAnchor = cursor.length > 0 ? cursor.start + cursor.length : cursor.start;
		}
		const auto from = sprt::min(_selectionAnchor, target);
		const auto to = sprt::max(_selectionAnchor, target);
		setCursor(TextCursor(from, to - from));
	} else {
		_selectionAnchor = maxOf<uint32_t>();
		setCursor(TextCursor(target));
	}
}

bool TextInput::handleKey(const GestureData &data) {
	if (!_focused || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	const bool select = hasFlag(ev.input.modifiers, InputModifier::Shift);
	const auto size = uint32_t(_inputState.size());

	switch (ev.key.keycode) {
	case InputKeyCode::LEFT: moveCursor(offsetCursor(-1), select); return true;
	case InputKeyCode::RIGHT: moveCursor(offsetCursor(1), select); return true;
	// a single-line field has nowhere to go vertically, so Up/Down are the line ends
	case InputKeyCode::UP:
	case InputKeyCode::HOME: moveCursor(0, select); return true;
	case InputKeyCode::DOWN:
	case InputKeyCode::END: moveCursor(size, select); return true;
	default: break;
	}
	return false;
}

bool TextInput::handleTextHotkey(HotkeyId id, const InputEvent &) {
	if (!_focused) {
		return false;
	}

	auto &hk = EngineHotkeys::get();

	if (id == hk.focusNext || id == hk.focusPrev) {
		/* A key event rather than '\t' text, so Shift is available for backwards navigation.
		   Inside a form the navigate callback hands this to the form; standalone, the field
		   blurs. */
		if (_navigateCallback) {
			return _navigateCallback(id == hk.focusPrev);
		}
		blur();
		return true;
	} else if (id == hk.textAccept || id == hk.textAcceptKeypad) {
		// Declined when no callback is set, so the form's submit binding (visited later) gets the
		// key. An installed callback wins
		if (_enterCallback) {
			_enterCallback();
			return true;
		}
		return false;
	} else if (id == hk.textSelectAll) {
		selectAll();
		return true;
	} else if (id == hk.textCopy) {
		return copy();
	} else if (id == hk.textCut) {
		return cut();
	} else if (id == hk.textPaste) {
		return paste();
	} else if (id == hk.undo) {
		return undo();
	} else if (id == hk.redo || id == hk.redoAlt) {
		return redo();
	}
	return false;
}

TextCursor TextInput::getWordForPosition(const Vec2 &loc) const {
	// getCharIndex with Center picks the glyph the pointer is over, not the nearest boundary,
	// which is what "the word I am pointing at" means
	auto label = _container->getLabel();
	auto idx = label->getCharIndex(label->convertToNodeSpace(loc), font::CharSelectMode::Center);
	if (idx.first == maxOf<uint32_t>()) {
		return TextCursor::InvalidCursor;
	}
	return label->selectWord(idx.first);
}

void TextInput::applyGestureCursor(TextCursor cursor) {
	if (!_focused && !isReadOnly()) {
		acquireInput(cursor);
	} else {
		setCursor(cursor);
	}
}

bool TextInput::handleTap(const GestureTap &tap) {
	if (!isEnabled()) {
		return false;
	}

	// The release that ends a long press is also a tap; swallow it to keep the selection.
	if (_longPressApplied) {
		_longPressApplied = false;
		return true;
	}

	_selectionAnchor = maxOf<uint32_t>();

	switch (tap.count) {
	case 1: applyGestureCursor(_container->getCursorForPosition(tap.location())); return true;
	case 2: {
		auto word = getWordForPosition(tap.location());
		if (word != TextCursor::InvalidCursor) {
			applyGestureCursor(word);
			return true;
		}
		break;
	}
	case 3: applyGestureCursor(TextCursor(0u, uint32_t(_inputState.size()))); return true;
	default: break;
	}
	return false;
}

bool TextInput::handlePress(const GesturePress &press, bool begin) {
	if (!isEnabled()) {
		return false;
	}

	if (begin) {
		_longPressApplied = false;

		// The recognizer counts the hold in update(), which needs frames; held for the press.
		if (!getActionByTag(TextInputPressTag)) {
			runAction(Rc<RenderContinuously>::create(), TextInputPressTag);
		}
	} else {
		stopAllActionsByTag(TextInputPressTag);
	}

	_activeApplied = begin;
	updateInteractiveState();
	return begin;
}

bool TextInput::handleLongPress(const GesturePress &press) {
	// A drag took the gesture over and selects by itself.
	if (!isEnabled() || _dragSelecting || _panning || _inputState.empty()) {
		return true;
	}

	switch (press.tickCount) {
	case 1: {
		// Nothing under the finger (past the end of the text): select the whole text on the
		// next tick.
		auto word = getWordForPosition(press.location());
		if (word != TextCursor::InvalidCursor) {
			_selectionAnchor = word.start;
			_longPressApplied = true;
			applyGestureCursor(word);
		}
		break;
	}
	case 2:
		_selectionAnchor = 0;
		_longPressApplied = true;
		applyGestureCursor(TextCursor(0u, uint32_t(_inputState.size())));
		break;
	default:
		// everything is selected already - keep the gesture alive, but there is nothing to widen
		break;
	}

	return true;
}

bool TextInput::handleSwipeBegin(const Vec2 &pt) {
	if (!isEnabled() || !isTouched(pt, 8.0f)) {
		return false;
	}

	if (_focused && !isReadOnly()) {
		auto cursor = _container->getCursorForPosition(pt);
		_selectionAnchor = cursor.start;
		_dragSelecting = true;
		_listener->setExclusive();
		return true;
	}

	// not focused: a drag pans an overflowing field so its text can be read without editing it
	if (_container->hasHorizontalOverflow()) {
		_panning = true;
		_listener->setExclusive();
		return true;
	}
	return false;
}

bool TextInput::handleSwipe(const Vec2 &pt, const Vec2 &delta) {
	if (_dragSelecting) {
		auto cursor = _container->getCursorForPosition(pt);
		moveCursor(cursor.start, true);
		// keep pulling while the pointer sits outside the box
		_container->setAutoScrollTarget(_container->isTouched(pt) ? Vec2::INVALID : pt);
		return true;
	}

	if (_panning) {
		_container->moveHorizontalOverflow(delta.x);
		return true;
	}
	return false;
}

bool TextInput::handleSwipeEnd() {
	if (_dragSelecting) {
		_dragSelecting = false;
		_container->setAutoScrollTarget(Vec2::INVALID);
		return true;
	}
	if (_panning) {
		_panning = false;
		return true;
	}
	return false;
}

void TextInput::setUndoEnabled(bool value) {
	if (_history.isEnabled() == value) {
		return;
	}
	_history.setEnabled(value);

	/* Scheduled per frame only to give the history's idle window a clock. */
	if (value) {
		scheduleUpdate();
	} else {
		unscheduleUpdate();
	}
}

bool TextInput::undo() {
	// The history commits a run in progress, so Ctrl+Z mid-word undoes the word. False when there
	// is nothing, so the chord falls through to handlers below.
	return _history.undo();
}

bool TextInput::redo() { return _history.redo(); }

WideStringView TextInput::sliceForHistory(uint32_t pos, uint32_t len) const {
	auto str = _inputState.getStringView();
	if (pos >= str.size()) {
		return WideStringView();
	}
	len = sprt::min(len, uint32_t(str.size()) - pos);
	return WideStringView(str.data() + pos, len);
}

void TextInput::beginHistoryBatch() {
	auto str = _inputState.getStringView();
	_historyShadow = WideString(str.data(), str.size());
	_historyBatch = true;
}

void TextInput::applyHistoryEdit(uint32_t pos, uint32_t removed, WideStringView inserted) {
	if (_historyBatch) {
		// Into the shadow, not to the platform: separate requests against an un-echoed string
		// would overwrite each other. The batch is requested once, at the end.
		pos = sprt::min(pos, uint32_t(_historyShadow.size()));
		removed = sprt::min(removed, uint32_t(_historyShadow.size()) - pos);
		_historyShadow.replace(pos, removed, inserted.data(), inserted.size());
		return;
	}

	// A request, like every other edit here: the platform owns this text. Its echo is counted,
	// not recorded.
	++_historyEchoes;
	insertText(inserted, TextCursor(pos, removed));
}

void TextInput::endHistoryBatch() {
	if (!_historyBatch) {
		return;
	}
	_historyBatch = false;

	auto current = _inputState.getStringView();
	if (WideStringView(_historyShadow) == current) {
		// An entry that changed nothing asks for nothing, and leaves no echo to account for.
		_historyPendingCursor = TextCursor::InvalidCursor;
		return;
	}

	++_historyEchoes;
	setText(WideStringView(_historyShadow));
}

void TextInput::setHistoryCursor(TextCursor cursor) {
	// Not setCursor(): the requested edit has not been echoed, and a cursor push now would send the
	// old string and cancel it. Applied on the echo instead.
	_historyPendingCursor = cursor;
}

void TextInput::recordHistoryEdit(uint32_t pos, uint32_t removed, WideStringView inserted,
		TextCursor cursorBefore) {
	_history.recordEdit(pos, sliceForHistory(pos, removed), inserted, cursorBefore,
			_historyEditName, _historyClock);
}

void TextInput::update(const UpdateTime &time) {
	VectorSprite::update(time);

	// `global` rather than `app`: AppThread computes app-time as (start - now), so it decreases
	// and underflows (see XLAppThread::performUpdate).
	_historyClock = time.global;
	_history.tickIdle(_historyClock);
}

} // namespace stappler::xenolith::ui
