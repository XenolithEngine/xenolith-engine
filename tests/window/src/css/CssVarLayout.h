/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_CSS_CSSVARLAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_CSSVARLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiStyleResolver.h" // IWYU pragma: keep - document::ParameterName below

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// CSS custom properties: `--name: value` declarations and `var(--name, fallback)` references.
//
// A custom property has no type - its value is raw text substituted into a declaration and
// parsed there, so the same variable can carry a colour, a length or a whole shorthand. It is
// always inherited, which is what makes `:root { --brand: … }` plus an override on a subtree
// the idiomatic way to theme part of a tree.
//
// The two cascade rules that are easy to get wrong are both pinned here: a variable declared by
// a MORE specific rule must be visible to a use in a less specific one (the variable is resolved
// on the element, not in declaration order), while the substituted declaration itself must
// still lose to a more specific literal declaration of the same property.
class CssVarLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runStatic();
	void runThemeSwitch();
	void report();

	void expectColor(StringView what, Node *, const Color4B &);
	void expectAppliedColor(StringView what, basic2d::Layer *, const Color4B &);
	void expectMetric(StringView what, Node *, document::ParameterName, float expected);
	void expectVar(StringView what, Node *, StringView name, StringView expected);
	void expectNoValue(StringView what, Node *, document::ParameterName);

	basic2d::Layer *_plain = nullptr; // colour straight from :root
	basic2d::Layer *_sized = nullptr; // length from a variable
	basic2d::Layer *_fallback = nullptr; // var() with a fallback for an undeclared name
	basic2d::Layer *_nested = nullptr; // a variable defined in terms of another
	basic2d::Layer *_cycle = nullptr; // --a: var(--b); --b: var(--a) - must be dropped
	basic2d::Layer *_late = nullptr; // variable declared by a MORE specific rule
	basic2d::Layer *_beaten = nullptr; // var() value must lose to a more specific literal

	Node *_theme = nullptr; // subtree that overrides --brand at runtime
	basic2d::Layer *_themed = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_CSSVARLAYOUT_H_
