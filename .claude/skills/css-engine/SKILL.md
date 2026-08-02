---
name: css-engine
description: >-
  Write CSS for the Xenolith/Stappler engine's CSS engine (stappler_document +
  xenolith_renderer_ui StyleSystem) CORRECTLY — it is a CSS subset, NOT web CSS.
  Use before writing or debugging any .css in this repo (resources/style.css,
  pug templates, ui:: atoms). Covers what flex/position/measure/selectors/units
  properties are really supported, and — critically — what web CSS features are
  silently ignored (var(), :nth-child, min/max on a flex item's CROSS axis,
  position:relative offsets, prefers-color-scheme, transform/box-shadow).
---

# Xenolith CSS engine — authoring reference

The engine is a **CSS subset** backed by `stappler_document` and applied to the
scene graph by `xenolith_renderer_ui` (`ui::StyleSystem` + `ui::StyleResolver` +
`ui::LayoutSystem`). It is NOT a browser. Many web CSS features parse silently and
do nothing. This card lists what REALLY works, so you don't waste an iteration
writing CSS that the engine drops on the floor.

Full reference, with the rationale, the cookbook of workarounds and the test that
pins every claim: **[docs/usage/ui/css-subset.adoc](../../../docs/usage/ui/css-subset.adoc)**.
Read it when this card is not enough; edit it (not just this card) when the
engine changes.

Origin: Y is **up** (scene graph). Styling is component-driven — the resolver reads
CSS into `ResolvedStyle` and per-type appliers (e.g. `button`, `panel`, `label`)
map it onto nodes. A recursive `StyleResolver(true)` on a layout root styles the
whole subtree.

## CRITICAL — web features that are SILENTLY dropped

DO NOT use these (they parse but do nothing, or don't exist):

| Web feature | Status here |
|---|---|
| `var(--x)` / custom properties | **absent** — no `var()`, no `--name` registry |
| `min-width`/`max-width` on a flex item's **cross** axis | parsed, **not applied** — the main axis IS enforced, see below |
| `position: relative` offsets | **no effect** — only `position: absolute` is implemented |
| `position: fixed` / `sticky` | **no effect** |
| `:nth-child`/`:first-child`/`:not()`/`:empty`/`:lang()` | **unsupported** — rule is skipped |
| `::before`/`::after`/`::marker` pseudo-elements | **unsupported** — rule is skipped |
| `[attr]`/`[attr=val]` attribute selectors | parsed, **never match** |
| `prefers-color-scheme` | **absent** — use `@media (light-level: dim)` or `x-option` |
| `transform` `box-shadow` `text-shadow` `filter` `transition` `animation` `cursor` `overflow` `box-sizing` `object-fit` `letter-spacing` | **not registered** (unknown-property warning) |
| `background` shorthand | **absent** — write `background-color` etc. individually |
| `border-radius` / `outline-*` on a plain `Layer` | **dropped** — only the typed widgets (`panel` `badge` `checkbox` `button`) can draw them |
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
- `margin` / `margin-{top,right,bottom,left}`: `%` vs parent width. **`auto` works**: on the main
  axis the auto margins of a line split the leftover space and `justify-content` gets none of it
  (`margin-left:auto` → push to the end, `margin: 0 auto` → centre); on the cross axis an item's
  own auto margins centre/push it and OVERRIDE `align-self`, `stretch` included. Grid ignores it.

## Right-aligning inside a flex row (the common need)

Any of these works:
```css
.row > .last  { margin-left: auto; }               /* just this item to the end */
.row          { justify-content: space-between; }  /* first & last to the edges */
.row > .spacer { flex-grow: 1; }                   /* explicit slack-eating gap */
```

## Width / height

- `width` / `height`: `<length>` | `%`(vs parent) | `em` | `auto` | **`fit-content`**
- `%` resolves against the **parent** size (parent content-box).
- On a **Label**: `width` (non-auto) → `Label::setWidth` (wraps); `height` is ignored (label measures its height from text).
- On **Layer/Button/…** inside a flex/grid parent: the size is published to a
  `MeasureComponent` as the intrinsic size; `LayoutSystem` still controls the final
  `ContentSize`. Explicit width/height wins over fit-content.
- Outside any layout parent: `width`/`height` → direct `setContentSize`.
- `min-width`/`max-width` (and `min-height`/`max-height`) **do constrain a flex item on the
  container's MAIN axis** — `min-*`/`max-*` along the row for `flex-direction: row`, along the
  column for `column`. They bound the flex base size and the size grow/shrink produces, and the
  space a clamped item gives up is redistributed to the others (CSS "resolve the flexible
  lengths"). On the **cross** axis they still do nothing — size that with `align-self`/`height`.
- Outside a flex/grid container `min-*`/`max-*` are not applied at all.

## fit-content / measure — who can be measured

Who can answer is NOT a fixed class list — it is whoever implements the protocol:
- a system with `SystemFlags::HandleMeasure` (`Label` has one; `ui::LayoutSystem` has one, which
  is why a nested flex container measures itself; your own widget can have one);
- or the `MeasureComponent` fallback (fill `maxContent`/`minContent`/`normal`, no code needed).

When it is asked:
- `flex-basis: fit-content` (or `width: fit-content` / `flex: 0 0 fit-content`) — always;
- `flex-basis: auto` (the default) with NO definite CSS size on that axis — also measured (plain
  CSS: auto → size property → content);
- an explicit CSS `width`/`height` is definite and wins — never measured on that axis;
- the cross axis is re-measured at the final main size (wrapped label: width → height).

**Layer / Sprite / an empty Node answer neither**, so their current ContentSize stands in: give
them an explicit size, a `flex-grow`, or make them a flex container with a measurable child.
A measured item gets `handleLayoutApplied` with the box it finally received.

## Position

- `position: absolute` is the **only** implemented value. Offsets `top right bottom left`
  are resolved vs the **parent**.
  - If `width: auto` and **both** `left`+`right` are set → width stretches to fill
    `parentWidth - left - right`. Same for height with `top`+`bottom`. **Absolute stretch works.**
  - If size + both offsets are set, right/bottom is ignored.
  - Anchor is forced to (0,1) (top-left, since engine Y is up).
  - **The box leaves the flow.** Inside a `display:flex`/`grid` parent it is not an item: it
    takes no space, is not moved by the container, and its siblings are sized as if it were not
    there (the resolver marks it with `ui::OutOfFlowComponent`). So an overlay belongs INSIDE
    the container it covers — no need to keep it outside any more. Its `width`/`height` are
    committed directly rather than handed to the layout.
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

**It is also the placement order.** Inside a flex/grid container the layout reads the children
already sorted by z-order, so raising a node to draw it on top also moves it in the row or
column. That is by design, and it means:

- to set the order *within* the container without touching drawing → **`order: <int>`** (lower
  first, applied after the z-order sort);
- to lift a node above the content without disturbing the layout → take it out of the flow with
  `position: absolute`, then raise it with `-xl-z-order`.

`RenderingLevel` (`Solid`/`Surface`/`Transparent`, code-side only) is NOT an alternative: it
picks the render pass, and geometry behind opaque solid geometry is depth-rejected in the later
passes whatever level it has.

## Engine extensions (`-xl-*`)

| Property | Effect |
|---|---|
| `-xl-anchor-point: <x> [<y>]` | `setAnchorPoint`, normalized 0..1 (0,0=bottom-left). Ignored under `position: absolute`. |
| `-xl-position: <x> [<y>]` | `setPosition` directly, bypassing layout (`%` vs parent). Not applied under `position: absolute`. |
| `-xl-z-order: <int>` | z-order / placement order. |

## Mental model for laying out a screen

1. Root layout = a flex column (`display:flex; flex-direction:column`). It fills its parent (`height: 100%` or `flex: 1`).
2. Fixed-height bands (`header`, `footer`) take `height: <px>; width: 100%`. The flexible middle takes `flex: 1`.
3. Right-align within a row: `margin-left: auto` on the item, `justify-content: space-between`, or a `flex-grow:1` spacer.
4. Overlays/fullscreen covers: `position: absolute; top:0; right:0; bottom:0; left:0;` (stretches). It leaves the flow, so put it inside the container it covers and raise it with `-xl-z-order`.
5. Anything text-sized (badge, chip, tag) needs either an explicit width or to be a `display:flex` fit-content container with a `Label` child (Label is the one node that measures).
6. Never hand-position children of a `display:flex` container in `handleContentSizeDirty` — it fights the layout pass. Let flex do it; only size `position:absolute` nodes by hand (the engine doesn't auto-stretch an absolute node unless both opposite offsets are set, which is the recommended way).

## Anti-pattern checklist

- `var(--x)` → no custom properties.
- `transform`/`box-shadow`/`overflow`/`transition`/`cursor` → don't exist.
- `:nth-child`/`::before`/`[attr]` selectors → rule never matches.
- `min-width`/`max-width` on a flex item's CROSS axis → no effect (the main axis works).
- `position: relative` expecting an offset → only `absolute` is implemented.
- `-xl-z-order` to raise a node *without* moving it in a flex row → it is the placement order
  too. Use `order` for placement, `position: absolute` to leave the flow entirely.
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

## Related

- Full reference: [docs/usage/ui/css-subset.adoc](../../../docs/usage/ui/css-subset.adoc)
- Measurement protocol: [docs/usage/ui/content-measurement.adoc](../../../docs/usage/ui/content-measurement.adoc)

## Related skills

- `gui-debug` — `inspect_scene`/`get_logs` to verify the tree and business logic.
- `cli-build` — build/run the debug app to see CSS changes.
