---
name: css-engine
description: >-
  Write CSS for the Xenolith/Stappler engine's CSS engine (stappler_document +
  xenolith_renderer_ui StyleSystem) CORRECTLY — it is a CSS subset, NOT web CSS.
  Use before writing or debugging any .css in this repo (resources/style.css,
  pug templates, ui:: atoms). Covers what flex/position/measure/selectors/units
  properties are really supported, and — critically — what web CSS features are
  silently ignored (margin:auto, var(), :nth-child, min/max-width on flex items,
  position:relative offsets, prefers-color-scheme, transform/box-shadow).
---

# Xenolith CSS engine — authoring reference

The engine is a **CSS subset** backed by `stappler_document` and applied to the
scene graph by `xenolith_renderer_ui` (`ui::StyleSystem` + `ui::StyleResolver` +
`ui::LayoutSystem`). It is NOT a browser. Many web CSS features parse silently and
do nothing. This card lists what REALLY works, so you don't waste an iteration
writing CSS that the engine drops on the floor.

Origin: Y is **up** (scene graph). Styling is component-driven — the resolver reads
CSS into `ResolvedStyle` and per-type appliers (e.g. `button`, `panel`, `label`)
map it onto nodes. A recursive `StyleResolver(true)` on a layout root styles the
whole subtree.

## CRITICAL — web features that are SILENTLY dropped

DO NOT use these (they parse but do nothing, or don't exist):

| Web feature | Status here |
|---|---|
| `var(--x)` / custom properties | **absent** — no `var()`, no `--name` registry |
| `margin: auto` (flex auto-margins) | parsed, **ignored by layout** — use `justify-content` or a `flex-grow:1` spacer |
| `min-width`/`max-width` on a flex item | parsed, **not applied to flex layout** — use `flex-basis`/`grow`/`shrink` |
| `position: relative` offsets | **no effect** — only `position: absolute` is implemented |
| `:nth-child`/`:first-child`/`:not()`/`:empty`/`:lang()` | **unsupported** — rule is skipped |
| `::before`/`::after`/`::marker` pseudo-elements | **unsupported** — rule is skipped |
| `[attr]`/`[attr=val]` attribute selectors | parsed, **never match** |
| `prefers-color-scheme` | **absent** — use `@media (light-level: dim)` or `x-option` |
| `transform` `box-shadow` `text-shadow` `filter` `transition` `animation` `cursor` `overflow` `box-sizing` `object-fit` `letter-spacing` | **not registered** (unknown-property warning) |
| `background` shorthand | **absent** — write `background-color` etc. individually |
| bare number for size (`width: 100`) | **rejected** — must have a unit (`100px`/`100%`/`1em`); only `line-height: 1.5` takes a bare number |
| elliptical `border-radius: H / V` | only H read; `/ V` dropped |

Unknown properties whose name starts with `-` (vendor/`-xl-`) are suppressed
silently; unknown unprefixed properties emit an "Unknown CSS parameter" log.

## Flexbox (fully supported)

A `display: flex` node becomes a flex container (`FlexLayoutInfo`); each direct
child becomes a flex item (`FlexItemInfo`). Applied in `XLUiStyleResolver.cc`.

Container:
- `flex-direction`: `row | row-reverse | column | column-reverse`
- `flex-wrap`: `nowrap | wrap | wrap-reverse`
- `justify-content`: `flex-start | flex-end | center | space-between | space-around | space-evenly` (also `start/end/left/right/normal`)
- `align-items` / `align-content`: `flex-start | flex-end | center | stretch | space-between | space-around` (note: `stretch` is the default for align-items; `normal`/`baseline`/`auto` collapse to `stretch`)
- `gap` / `row-gap` / `column-gap`: `<length>` | `%` | `normal`(=0). `gap: 8px` = both; `gap: 8px 12px` = row column.
- `padding` / `padding-{top,right,bottom,left}`: `%` is vs the container's **own** width.

Item (on a direct child):
- `flex-grow`, `flex-shrink`: `<number>`
- `flex-basis`: `<length>` | `%` | `em` | `auto` | **`fit-content`** | `content`(=auto)
- `flex` shorthand: `none`(=0 0 auto) | `initial`(=0 1 auto) | `[<grow> <shrink>?] [<basis>]`. Bare `flex: 1` → grow=1, shrink=1, basis=0px.
- `order`: `<integer>` (lower first; default 0)
- `align-self`: `auto`(inherit) | flex-start | flex-end | center | stretch
- `margin` / `margin-{top,right,bottom,left}`: `%` vs parent width. **`auto` ignored** — no flex auto-margins.

## Right-aligning inside a flex row (the common need)

`margin-left: auto` does NOT work. Use ONE of:
```css
.row { justify-content: space-between; }   /* first & last to the edges */
/* OR a grow spacer between the left and right groups: */
.row .spacer { flex-grow: 1; }
```

## Width / height

- `width` / `height`: `<length>` | `%`(vs parent) | `em` | `auto` | **`fit-content`**
- `%` resolves against the **parent** size (parent content-box).
- On a **Label**: `width` (non-auto) → `Label::setWidth` (wraps); `height` is ignored (label measures its height from text).
- On **Layer/Button/…** inside a flex/grid parent: the size is published to a
  `MeasureComponent` as the intrinsic size; `LayoutSystem` still controls the final
  `ContentSize`. Explicit width/height wins over fit-content.
- Outside any layout parent: `width`/`height` → direct `setContentSize`.
- `min-*`/`max-*` parse but **don't constrain flex items**.

## fit-content / measure — who can be measured

- `flex-basis: fit-content` (or `width: fit-content` / `flex: 0 0 fit-content`) asks
  the layout to measure the node's content via `handleMeasure`.
- **Only `Label` measures natively** (text reflow). A flex container also measures
  itself (flex dry-run) when used as a fit-content item.
- **Layer / Button / Sprite / Panel / empty Node do NOT measure** — give them an
  explicit `width`/`height`, or `flex-grow`, or make them a flex container with
  fit-content basis. A badge sized to its label needs an explicit width unless it's
  a fit-content flex container.

## Position

- `position: absolute` is the **only** implemented value. Offsets `top right bottom left`
  are resolved vs the **parent**.
  - If `width: auto` and **both** `left`+`right` are set → width stretches to fill
    `parentWidth - left - right`. Same for height with `top`+`bottom`. **Absolute stretch works.**
  - If size + both offsets are set, right/bottom is ignored.
  - Anchor is forced to (0,1) (top-left, since engine Y is up).
- `relative`/`fixed`/`sticky`/`static`: **no positional effect** (only `-xl-anchor-point`/`-xl-position` run).

## Colors

Properties: `color`, `background-color`, `outline-color`, `border-*-color`. `transparent` ok.
Formats: `#rgb` `#rgba` `#rrggbb` `#rrggbbaa`, `rgb(r,g,b)`, `rgba(r,g,b,a)` (commas),
`hsl(h,s%,l%)`, named (`white gray black red …` + full Material `Red_500` etc.).
Note: setting a color with alpha also sets `opacity`. `opacity: 0..1`.

## Border-radius

Fully supported: `border-radius: 1..4 values` (TL TR BR BL ordering), or per-corner
`border-top-left-radius` etc. `<length>`|`%`|`em`. `50%` → circle. **No elliptical `H / V`.**

## Font / text (all inherited)

`font-size` (`px`/`em` or named xx-small…xx-large; NOT rem), `font-weight` (`normal`/`bold`/1..1000),
`font-style`, `font-family`, `text-align` (`left right center justify`), `text-decoration`,
`text-transform` (`none uppercase lowercase`), `line-height` (metric or bare-number multiplier),
`white-space`, `hyphens`, `vertical-align`, `color`.

## Selectors

Supported: `*`, tag, `.class`, `#id`, compound (`tag.cls`, `tag.a.b`),
descendant (`A B`), child (`A > B`), adjacent sibling (`A + B`), general sibling (`A ~ B`),
comma lists. Specificity is standard (a=id, b=class+pseudo, c=tag).

**Where the selector operands come from** (`NodeIdentity` in `XLNode.h`):
`#id` ← **`Node::setName()`** — a node's *name* IS its CSS id; tag/type selector ←
`Node::setType()`; `.class` ← `Node::addStyleClass()`. `Node::setTag()` (numeric) and
`setDataValue()` are invisible to CSS. Names are not unique — two nodes with the same
name are both matched by `#name`.
Interactive pseudo-classes that DO work: **`:hover :focus :active :checked :enabled :disabled`**.

NOT supported (rule is dropped): `::before`/`::after`/pseudo-elements, structural
pseudo-classes (`:nth-child :first-child :not() :empty …`), attribute selectors `[attr]`.

## @media queries

`@media (feature: value) [and …] [,…]`, optional `not`/`only`. Features resolved at runtime:
`orientation` (landscape/portrait), `pointer` (fine/coarse/none), `hover`,
**`light-level`** (normal/dim/washed — closest to prefers-color-scheme),
`scripting`, **`platform`** (macos/ios/windows/android/linux/web — host platform),
`width`/`min-width`/`max-width`, `height`/`min-/max-`, `aspect-ratio`, `resolution`,
**`x-option`** (arbitrary app-set flag). No `prefers-color-scheme`/`color-gamut`.

## Units

`px` `%` `em` `rem` `vw vh vmin vmax` `pt`(×4/3) `pc`(×15) `mm` `cm` `in`(×90). `fr` **only inside grid tracks**.
`auto`, `fit-content`, bare number (rejected except `line-height`).

## display

`flex`/`inline-flex` → flex layout. `grid`/`inline-grid` → grid layout (tracks, `repeat(N,…)`, `gap`, area placement; NO `minmax`/`fit-content()`/named lines/`auto-fit`).
`none` → VisibilityComponent skips the subtree (collapses layout). `block`/`inline`/etc → no block flow (no effect beyond "not a layout container").

## Hide vs collapse

- `display: none` → subtree skipped (collapses flex/grid layout).
- `visibility: hidden` → box kept, not painted (inherited).
- `opacity: 0` → still laid out + painted, transparent.

## z-order (NOT `z-index`)

There is no `z-index`. Use **`-xl-z-order: <int>`** → `Node::setZOrder` (higher = drawn/placed later).
Within flex/grid it sets logical placement order (applied before the `order`-based reorder).

## Engine extensions (`-xl-*`)

| Property | Effect |
|---|---|
| `-xl-anchor-point: <x> [<y>]` | `setAnchorPoint`, normalized 0..1 (0,0=bottom-left). Ignored under `position: absolute`. |
| `-xl-position: <x> [<y>]` | `setPosition` directly, bypassing layout (`%` vs parent). Not applied under `position: absolute`. |
| `-xl-z-order: <int>` | z-order / placement order. |

## Mental model for laying out a screen

1. Root layout = a flex column (`display:flex; flex-direction:column`). It fills its parent (`height: 100%` or `flex: 1`).
2. Fixed-height bands (`header`, `footer`) take `height: <px>; width: 100%`. The flexible middle takes `flex: 1`.
3. Right-align within a row: `justify-content: space-between`, or a `flex-grow:1` spacer — never `margin: auto`.
4. Overlays/fullscreen covers: `position: absolute; top:0; right:0; bottom:0; left:0;` (stretches) — kept out of the flex flow.
5. Anything text-sized (badge, chip, tag) needs either an explicit width or to be a `display:flex` fit-content container with a `Label` child (Label is the one node that measures).
6. Never hand-position children of a `display:flex` container in `handleContentSizeDirty` — it fights the layout pass. Let flex do it; only size `position:absolute` nodes by hand (the engine doesn't auto-stretch an absolute node unless both opposite offsets are set, which is the recommended way).

## Anti-pattern checklist

- `margin: auto` for flex centering/right-align → use `justify-content` or `flex-grow` spacer.
- `var(--x)` → no custom properties.
- `transform`/`box-shadow`/`overflow`/`transition`/`cursor` → don't exist.
- `:nth-child`/`::before`/`[attr]` selectors → rule never matches.
- `min-width`/`max-width` constraining a flex item → use `flex-basis`/`grow`/`shrink`.
- `position: relative` expecting an offset → only `absolute` is implemented.
- `prefers-color-scheme` → use `light-level` or `x-option`.
- Sizes need units: `width: 100` is invalid; `100px`/`100%`/`1em`. `line-height: 1.5` is the only bare-number exception.

## Source map (for deeper questions)

- CSS property parse table: `stappler/document/SPDocStyleCss.cc:279` (`s_cssParameters`)
- Property-name enum: `stappler/document/SPDocStyle.h:304` (`ParameterName`)
- Selector parse: `stappler/document/SPDocStyleContainer.cc:887`..`:1011`
- Unit parse: `runtime/src/geom/SPRuntimeGeometry.cc:31` (`Metric::readStyleValue`)
- Color parse: `runtime/src/geom/SPRuntimeColor.cc:1489`
- Flex model: `xenolith/renderer/ui/layout/XLUiLayoutFlex.h:58`
- Flex algorithm: `xenolith/renderer/ui/layout/XLUiLayoutSystem.cc:639`
- CSS→layout bridge: `xenolith/renderer/ui/style/XLUiStyleResolver.cc:1307` (`applyLayout`), `:982` (`applyDefault`)

## Related skills

- `gui-debug` — `inspect_scene`/`get_logs` to verify the tree and business logic.
- `cli-build` — build/run the debug app to see CSS changes.
