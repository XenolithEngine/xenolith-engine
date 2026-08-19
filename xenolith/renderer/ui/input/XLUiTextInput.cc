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

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr uint32_t TextInputAdjustTag = "XLUiTextInputAdjust"_tag;
static constexpr uint32_t TextInputBlinkTag = "XLUiTextInputBlink"_tag;

// Half a blink period. 0.53s is the classic value and reads as deliberate rather than nervous.
static constexpr float TextInputBlinkHalfPeriod = 0.53f;

static constexpr float TextInputCaretWidth = 1.5f;

// how far past the viewport edge the caret may travel before the label is slid back
static constexpr float TextInputScrollMargin = 60.0f;

// pixels per second while a drag-selection pulls the text past an edge
static constexpr float TextInputAutoScrollSpeed = 300.0f;

// How long the finger has to sit still before the field starts selecting, and the step of every
// widening after that: hold once for the word, keep holding for the whole text.
static constexpr TimeInterval TextInputLongPressInterval = TimeInterval::milliseconds(500);

// Frames stop coming when nothing on screen changes, and a finger resting on the field changes
// nothing - so the press recognizer, which counts its intervals in update(), would never tick.
static constexpr uint32_t TextInputPressTag = "XLUiTextInputPress"_tag;

// Register the per-attribute style appliers for nodes of type "text-input" once, the first time a
// TextInput is constructed - the same "resolve by type" hook ui::Button uses.
static void ensureTextInputStyleAppliers() {
	using document::ParameterName;
	static bool once = [] {
		StyleResolver::registerTypeApplier("text-input",
				[](StyleResolver &res, Node *node, const ResolvedStyle &s,
						document::ParameterName name, const document::StyleValue &val) {
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
		return true;
	}();
	(void)once;
}

ComponentId TextInputStyleComponent::Id;

TextInputContainer::~TextInputContainer() { }

bool TextInputContainer::init() {
	if (!Node::init()) {
		return false;
	}

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(0));
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

	// ApplyForAll, not ApplyForNodesBelow: "below" means children with a NEGATIVE z-order (see
	// Node::wrapVisit), and the label and placeholder sit at ZOrder(0), i.e. "above". Scoping the
	// scissor to the below-set would leave the overflowing text unclipped.
	// 2px of horizontal bleed so a caret at position 0 or at the very end is not shaved off.
	_scissor = addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
	_scissor->enableScissor(Padding(0.0f, 2.0f));

	return true;
}

void TextInputContainer::update(const UpdateTime &time) {
	Node::update(time);

	// Vec2::INVALID is a pair of NaNs, and NaN compares equal to nothing - "is a target set" has to
	// be asked as isValid(), never as == Vec2::INVALID.
	if (!_autoScrollTarget.isValid() || !hasHorizontalOverflow()) {
		return;
	}

	// A drag that left the box keeps pulling the text: the pointer is parked outside, so there is
	// no further gesture event to react to and the motion has to come from the clock.
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

	_placeholder->setPosition(Vec2(0.0f, 0.0f));

	// The caret spans the whole inner box of the field, not the glyph height: a bar shorter than
	// the box reads as misaligned against the text, and an empty field would show a stub. The font
	// height is only the fallback for the moment before the box has a size of its own.
	_caret->setContentSize(Size2(TextInputCaretWidth,
			_contentSize.height > 0.0f ? _contentSize.height : float(_label->getFontHeight())));
	_caretDirty = true;
}

bool TextInputContainer::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	// Deferred on purpose: setCursor/handleLabelChanged only mark, so N mutations in one frame
	// (typing pushes a string change and a cursor change together) cost one recomputation.
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
	// without a selection there is only one end, and an unknown moving end keeps the historical
	// behaviour of following the start of the range
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
	// NaN != NaN, so "already stopped" has to be tested through isValid() as well - otherwise every
	// stop request would fall into the start branch below and leave the field rendering forever
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

void TextInputContainer::updateCaretPosition() {
	const auto cpos =
			_label->empty() ? _label->getCursorOrigin() : _label->getCursorPosition(_cursorActive);

	// Only the horizontal position comes from the label; vertically the caret is pinned to the
	// bottom of the container, whose height it has. The caret is a child of the label, so the
	// container's bottom edge is at -label.y in this space.
	_caret->setPosition(Vec2(cpos.x, -_label->getPosition().y));

	// While a drag-selection pulls the text past an edge, the pointer owns the offset (see update):
	// a re-centring action on top of it would fight the per-frame slide.
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

	const bool visible = _enabled && !_readOnly && _cursor.length == 0;
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

	// short hops snap, long ones glide; a fixed duration would crawl for a one-character step and
	// lag for a select-all jump
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

	ensureTextInputStyleAppliers();

	setType("text-input");
	addStyleClass("xl-ui-text-input");
	setRenderingLevel(RenderingLevel::Surface);

	_container = addChild(makeContainer(), ZOrder(1));
	_container->setAnchorPoint(Anchor::BottomLeft);

	_listener = addSystem(Rc<InputListener>::create());

	// Text dropped onto the field lands the same way pasted text does - same type rule, same
	// insertion point, same validation. A drop target and a paste target really are one handler
	addSystem(Rc<DropTarget>::create(DropTargetSlots{
		.accept = [this](const DragEvent &event) -> DragResponse {
		if (_readOnly || !event.data) {
			return DragResponse();
		}
		auto want = StringView("text/plain");
		if (event.data->preferType(makeSpanView(&want, 1)).empty()) {
			return DragResponse();
		}
		// Either is fine here: whether the source deletes its original is the SOURCE's business
		return DragResponse{event.allowed & (DragActions::Copy | DragActions::Move)};
	},
		.drop = [this](const DragEvent &event, DragActions) { return handleTextDrop(event); },
	}));

	// A key event carries the pointer location (the platform backends fill it in from the last
	// mouse position), so the default filter - "is the node under the pointer" - would only deliver
	// arrows while the mouse happens to hover the field. A focused text field owns the keyboard
	// wherever the pointer is, so keyboard events bypass the hit test entirely; pointer events keep
	// the default behaviour.
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

	// maxTapCount 3: one tap places the caret, two select a word, three select everything - and
	// Immediate, because these three are refinements of each other, not alternatives. Waiting to
	// learn whether a second tap follows would delay the caret of every single click by
	// TapIntervalAllowed, which is exactly the lag the user sees.
	_listener->addTapRecognizer([this](const GestureTap &tap) { return handleTap(tap); },
			InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 3,
				InputTapFlags::Immediate});

	// Continuous, so the hold keeps reporting every interval instead of firing once: that is what
	// turns "keep holding" into the next, wider selection.
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

	/* Cursor movement stays on a recognizer: Left and Shift+Left are the same motion with a
	   different flag, not two commands, and the runtime never claims those keys anyway.

	   Everything that IS a command - navigation away from the field, accept, and the clipboard
	   chords - is a hotkey, so it can be rebound and so the precedence against the form's own
	   bindings is the walk order rather than two key masks that have to agree. */
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

	_listener->setCursor(WindowCursor::Text);

	// Tap outside the field releases input. Priority 1 puts it above the scene graph, and its touch
	// filter accepts ONLY points outside the widget, so it never competes with the field's own tap.
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

	return true;
}

void TextInput::handleEnter(Scene *scene) {
	VectorSprite::handleEnter(scene);
	updateInteractiveState();
}

void TextInput::handleExit() {
	// The handler's destructor would do this too, but a node can leave the scene and come back;
	// holding the OS keyboard for an off-scene widget is never right.
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

	const auto width = sprt::max(_contentSize.width - style->padding.horizontal(), 0.0f);
	const auto height = sprt::max(_contentSize.height - style->padding.vertical(), 0.0f);

	_container->setContentSize(Size2(width, height));
	_container->setPosition(Vec2(style->padding.left, style->padding.bottom));
}

void TextInput::handleComponentsDirty(const ComponentMask &mask) {
	VectorSprite::handleComponentsDirty(mask);

	if (mask.contains(TextInputStyleComponent::Id.value)) {
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

	// shrink each corner radius by the inset so the OUTER edge of the stroke keeps the requested
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
	auto style = getComponent<TextInputStyleComponent>();
	if (!style) {
		return;
	}

	// Fallbacks derive from the text colour, so an unconfigured field still has a visible caret and
	// a selection that reads as "the same ink, dimmed".
	const auto text = Color4F(_container->getLabel()->getColor());

	_container->setCaretColor(style->hasCaretColor ? Color4F(style->caretColor) : text);

	auto selection = text;
	selection.a = 0.35f;
	_container->setSelectionColor(
			style->hasSelectionColor ? Color4F(style->selectionColor) : selection);

	auto marked = text;
	marked.a = 0.5f;
	_container->setMarkedColor(style->hasMarkedColor ? Color4F(style->markedColor) : marked);
}

bool TextInput::setStyleValue(const ResolvedStyle &style, document::ParameterName name,
		const document::StyleValue &value) {
	using document::ParameterName;

	// CmdReset arrives before the parameters of every style pass and means "undo whatever the last
	// pass left"; a rule that stopped matching simply goes missing, so dropping the component is
	// the only way to notice. The custom properties are re-read here because, unlike a parameter,
	// they are never delivered as one.
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

	// No handler running, so nothing owns the state but this widget - the one place where writing
	// _inputState directly is correct, because there is no echo to wait for.
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
	if (value == _readOnly) {
		return;
	}
	_readOnly = value;
	_container->setReadOnly(value);
	if (_readOnly && _handler.isActive()) {
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

bool TextInput::copy() {
	const auto cursor = _inputState.cursor;
	if (cursor.length == 0 || !_director) {
		return false;
	}

	// A masked field's contents are exactly what must not leave the widget
	if (_passwordMode != TextInputPasswordMode::NotPassword) {
		return false;
	}

	auto str = string::toUtf8<Interface>(
			WideStringView(_inputState.getStringView(), cursor.start, cursor.length));

	// writeToClipboard copies the bytes and retains the Ref, so the local String may die here
	_director->getApplication()->writeToClipboard(
			BytesView(reinterpret_cast<const uint8_t *>(str.data()), str.size()),
			StringView("text/plain"), this);
	return true;
}

bool TextInput::cut() {
	if (_readOnly || !copy()) {
		return false;
	}
	insertText(WideStringView(), _inputState.cursor);
	return true;
}

bool TextInput::paste() {
	if (_readOnly || !_director) {
		return false;
	}

	const auto serial = ++_pasteSerial;
	_director->getApplication()->readFromClipboard(
			[this, serial](Status st, BytesView data, StringView) {
		// A superseded answer: the field was pasted into again, or blurred while the read was in
		// flight. Applying it would drop text on top of a newer edit
		if (!sprt::status::isSuccessful(st) || serial != _pasteSerial) {
			return;
		}

		auto text = string::toUtf16<Interface>(
				StringView(reinterpret_cast<const char *>(data.data()), data.size()));

		// The caret as it is NOW, not as it was when the read started - the user may have moved it.
		// Length and character filtering happen in validateInput() on the echo
		insertText(WideStringView(text), pendingCursor());
	}, [](SpanView<StringView> types) -> StringView {
		// The same rule a dropped payload is matched with, so a field cannot end up accepting a
		// type on drop that it refuses on paste. It runs on an unknown thread and only looks at
		// strings, which is exactly what preferMimeType is
		auto want = StringView("text/plain");
		return preferMimeType(types, makeSpanView(&want, 1));
	}, this);
	return true;
}

bool TextInput::handleTextDrop(const DragEvent &event) {
	if (_readOnly || !event.data) {
		return false;
	}

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

	// the caret as it is NOW, exactly as a paste does; filtering happens in validateInput()
	insertText(WideStringView(text), pendingCursor());
	return true;
}

void TextInput::focus() {
	if (!_enabled || _readOnly || _handler.isActive()) {
		return;
	}
	acquireInput(TextCursor(uint32_t(_inputState.size())));
}

void TextInput::blur() {
	if (_handler.isActive()) {
		_handler.cancel();
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
	if (value == _enabled) {
		return;
	}

	_enabled = value;
	if (!_enabled) {
		blur();
	}

	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		return state->updateState(_enabled ? (state->state | InteractiveState::Enabled)
										   : (state->state & ~InteractiveState::Enabled));
	});
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

	// Enforced through the same correction path as a too-long echo, so there is exactly one place
	// that truncates.
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
	const bool wasComposing = _inputState.marked.length > 0;

	// Focus follows what the platform actually granted, not what was asked for - which is what
	// makes `:focus` in CSS mean something.
	if (_focused != data.enabled) {
		_focused = data.enabled;
		if (!_focused) {
			_focusListener->setEnabled(false);
			_selectionAnchor = maxOf<uint32_t>();
		}
		updateInteractiveState();
	}

	auto state = data;
	const bool corrected = validateInput(state);

	_inputState = sp::move(state);

	// The platform has spoken: whatever the widget had asked for is superseded.
	_pendingCursor = _inputState.cursor;

	_container->setEnabled(_focused);
	_container->setCursor(_inputState.cursor, activeCursorPosition(_inputState.cursor));
	_container->setMarked(_inputState.marked);

	const bool stringChanged = _inputState.string != previousString;
	if (stringChanged) {
		updateDisplayString();
	}
	_container->handleLabelChanged();
	_container->setPlaceholderVisible(_inputState.empty() && !_focused);

	if (corrected) {
		// The correction has to travel back, or the platform keeps editing the string it thinks it
		// has and the next keystroke undoes the fix.
		pushRequest(_inputState.string, _inputState.cursor, _inputState.marked);
	}

	// A marked range is a composition in progress, not committed text: reporting it as a change
	// would make an autocomplete widget fire on every syllable being assembled.
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

	// Enter and Tab reach the field as text, because the runtime's processor consumes the key event
	// and inserts the character ('\r' remapped to '\n'). Stripping them here is what turns them
	// back into the actions they were.
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
			// moving towards, as every editor does
			return uint32_t(math::clamp(delta < 0 ? cursor.start : cursor.start + cursor.length,
					uint32_t(0), uint32_t(size)));
		}

		// a selection IS being extended: the step continues from the end the user is moving, so
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
		/* Shift is the only reason navigation has to be a key event rather than the '\t' the
		   platform used to insert: a stripped character carries no modifiers, so backwards
		   navigation cannot be expressed on that path. Inside a form the navigate callback hands
		   this to the form; standalone, the field just gives up focus. */
		if (_navigateCallback) {
			return _navigateCallback(id == hk.focusPrev);
		}
		blur();
		return true;
	} else if (id == hk.textAccept || id == hk.textAcceptKeypad) {
		// Declining when no callback is set is what lets the form's submit binding, which is
		// visited after this one, have the key. An explicitly installed callback wins on purpose
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
	if (!_focused && !_readOnly) {
		acquireInput(cursor);
	} else {
		setCursor(cursor);
	}
}

bool TextInput::handleTap(const GestureTap &tap) {
	if (!_enabled) {
		return false;
	}

	// The release that ends a long press is a tap too. Swallow it: it would otherwise land as a
	// single tap and collapse the selection the hold has just made.
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
	if (!_enabled) {
		return false;
	}

	if (begin) {
		_longPressApplied = false;

		// The recognizer counts the hold in update(), which only runs while frames are produced -
		// and a resting finger produces none. Held for exactly as long as the press is.
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
	// A drag took the gesture over: it is selecting by itself, and widening under it would fight
	// the pointer.
	if (!_enabled || _dragSelecting || _panning || _inputState.empty()) {
		return true;
	}

	switch (press.tickCount) {
	case 1: {
		// Nothing under the finger (past the end of the text) - fall through to the whole text on
		// the next tick rather than selecting a word at random.
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
	if (!_enabled || !isTouched(pt, 8.0f)) {
		return false;
	}

	if (_focused && !_readOnly) {
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

} // namespace stappler::xenolith::ui
