# xenolith_font

Font rendering for Xenolith: glyph layout (positioning) plus GPU-side glyph rasterization, atlas
packing and caching. Built around a single mutable atlas image per controller that grows on demand as
new glyphs are requested.

## Key types

| Type | File | Role |
|---|---|---|
| `FontController` (abstract base) | `XLFontController.h` | Positioning + source/family/alias state. Owns no GPU resources; defines virtual hooks `submitGlyphs` / `makeDependency` / `getImage` / `getTexture`. |
| `FontControllerLocal` | `XLFontControllerLocal.*` | GPU leaf: owns the atlas `DynamicImage` + `Texture`, drives `FontComponent` for raster. |
| `FontControllerRemote` | `XLFontControllerRemote.*` | Headless client leaf: positions locally, forwards rasterization to a server over `remote::Domain::Font`. |
| `FontComponent` | `XLFontComponent.*` | The gAPI endpoint (`FontGapi`): `compileImage` / `updateImage` into the gl Loop + `vk::FontQueue`. Owns the `FontLibrary`. |
| `RemoteFontServerEndpoint` | `XLRemoteFontServerEndpoint.*` | Server side of the remote split: a dedicated network `FontControllerLocal` + persistent font store. |
| `FontLibrary` | `../../stappler/font/SPFontLibrary.*` | FreeType context; opens `FontFaceData` / `FontFaceObject`, mints FaceIds. |
| `vk::FontQueue` | `backend/vk/XLVkFontQueue.*` | The render queue that rasterizes glyphs (FreeType on worker threads), packs the atlas, and uploads it. |

The remote split is documented separately; this file documents the **font atlas image lifecycle**,
which is identical for the local controller and the server's network controller.

---

## Font atlas image lifecycle

### The objects

```
FontController._image : core::DynamicImage         (the mutable atlas; R8_UNORM)
    └─ _instance : DynamicImageInstance             (current "generation N" embodiment)
           └─ data : ImageData { image: ImageObject, atlas: DataAtlas, extent, views }
FontController._texture : Texture                   (thin wrapper over the DynamicImage)
    └─ getIndex() == _instance->data.image->getIndex()
```

- **`core::DynamicImage`** — a *mutable* GPU image. Its pixels and metadata (the atlas) change as
  glyphs are added, but it hands out a stable handle. (`xenolith/core/XLCoreDynamicImage.*`)
- **`DynamicImageInstance`** — one concrete embodiment: `{ ImageData(ImageObject + DataAtlas + extent),
  view, userdata, gen }`. A new instance is produced on every atlas update.
  (`xenolith/core/XLCoreDynamicImage.h`)
- **`core::DataAtlas`** — maps a `font::CharId` to **four** `FontAtlasValue{pos, tex}` (one per glyph
  quad anchor). This is what turns a baked `CharId` into texture coordinates. (`xenolith/core/XLCoreObject.*`)
- **`Texture`** — an almost-stateless wrapper; forwards to the current `DynamicImageInstance`.
  (`xenolith/application/resources/XLTexture.*`)

### 1. Creation — a 2×2 placeholder

`FontComponent::makeInitialImage` (`xenolith/font/XLFontComponent.cc`) builds a 2×2 `R8_UNORM`
`DynamicImage` with bytes `{0,255,255,0}`. At this point `_instance == nullptr` — there is no GPU object
yet. `FontControllerLocal::initialize` immediately wraps it in a `Texture`.

### 2. GPU compile — instance #1 (`gen = 1`)

`_gapi->compileImage` → `core::Loop::compileImage` → `Device::compileImage`
(`xenolith/backend/vk/XLVkDevice.cc`): the CPU bytes are uploaded, an `ImageObject` is allocated, and it
is assigned an **index N** from the global atomic counter `s_ImageViewCurrentIndex`
(`xenolith/core/XLCoreObject.cc`). Then `DynamicImage::setImage(obj)`
(`xenolith/core/XLCoreDynamicImage.cc`) installs the first `DynamicImageInstance` (`gen = 1`, `index = N`).

### 3. Update — instance #2, #3, … (`gen + 1`), **same index**

Each glyph batch (`FontComponent::updateImage`) runs through `vk::FontQueue` and rebuilds the atlas. The
**critical invariant**: the new `ImageObject` is allocated via
`allocator->preallocate(..., forceId = instance->data.image->getIndex())`
(`xenolith/backend/vk/XLVkAllocator.cc`) — i.e. the new `VkImage` **reuses the old index N**. Then
`DynamicImage::updateInstance` (`xenolith/core/XLCoreDynamicImage.cc`) builds a new
`DynamicImageInstance` (`gen + 1`, new `ImageObject` + new `DataAtlas`), atomically swaps `_instance`,
and notifies the registered `_materialTrackers`.

```
create (2x2)        compile                update (glyph batch)        update …
 _instance=null  →  instance#1 gen=1   →   instance#2 gen=2        →   instance#3 gen=3
                    ImageObject idx=N      ImageObject idx=N            ImageObject idx=N   (forced stable)
                    VkImage 2x2            VkImage WxH + DataAtlas      VkImage W'xH' + DataAtlas'
```

**`ImageObject::getIndex()` is stable** from the first compile onward (deliberately forced on every
update). The `VkImage`, the extent, the contents and the atlas all change — but the index does not.
That stability is what lets the bound material survive atlas growth without being re-keyed (see
*Texture → Material binding* below).

### What flows on each update (the data path)

For one `FontUpdateRequest{ FontFaceObject, chars }`:

1. **Rasterize (worker threads).** `DeferredRequest::runFontRenderer` → FreeType →
   `CharTexture{ bitmap, x, y, w, h, fontID, charID }`. (`xenolith/font/XLFontDeferredRequest.cc`)
2. **Collect bitmaps.** `FontAttachmentHandle::pushCopyTexture` writes pixels into the `_frontBuffer`
   staging buffer and records a `VkBufferImageCopy`; `persistent` glyphs are also copied into
   `_persistentTargetBuffer`. (`xenolith/font/backend/vk/XLVkFontQueue.cc`)
3. **Pack + atlas.** `writeAtlasData` packs the rectangles (`font::emplaceChars`), computes the final
   `Extent2`, creates a `core::DataAtlas`, and via `pushAtlasTexture` adds **4 `FontAtlasValue{pos,tex}`
   per glyph** keyed by `font::CharId`. (`xenolith/font/backend/vk/XLVkFontQueue.cc`)
4. **Install.** `submitResult` (on the gl Loop thread) calls
   `image->updateInstance(_targetImage, atlas, userdata, view)` → new instance.
   (`xenolith/font/backend/vk/XLVkFontQueue.cc`)
5. **Signal.** `frame.signalDependencies(success)` wakes any frame that was waiting on the batch's
   `core::DependencyEvent`. (`xenolith/font/backend/vk/XLVkFontQueue.cc`)

### Incremental vs full

- **Rasterization is incremental.** Already-rasterized glyphs are *not* re-run through FreeType; their
  bitmaps are pulled from the persistent buffer carried over in the previous instance's `userdata`.
- **Atlas packing and upload are full.** Adding a single glyph rebuilds the entire `DataAtlas` and
  uploads a fresh `VkImage` (new size). The CharId → texcoord mapping is therefore regenerated each
  update, but the bitmap pixels for old glyphs are copied GPU-side, not re-rasterized.

### Dependency gating

`FontController::addTextureChars` mints a `core::DependencyEvent`; the batch's `updateImage` adds it as a
*signal* dependency of the font frame. A scene frame that uses those glyphs adds the same event to its
`waitDependencies`, so it cannot render until the atlas update that covers its glyphs has landed. A
glyph whose texcoord is not yet in the atlas degrades to a placeholder texel rather than corrupting the
frame.

---

## Texture → Material binding

The image is bound into a material by its **`ImageObject::getIndex()`** (the stable index N):

1. `Sprite/Label::getMaterialInfo` (`xenolith/renderer/basic2d/XL2dSprite.cc`) sets
   `MaterialInfo.images[0] = _texture->getIndex()` (= N), plus sampler / colorMode / pipeline.
2. `MaterialInfo::hash()` hashes the whole struct, **including `images[0]`**
   (`xenolith/application/nodes/XLNodeInfo.h`). Materials are keyed by this hash.
3. `FrameContext::getMaterial(info)` (`xenolith/application/director/XLFrameContext.cc`) looks the hash
   up → a `MaterialId` (or `acquireMaterial` creates one).
4. `Material::init(... DynamicImageInstance ...)` (`xenolith/core/XLCoreMaterial.cc`) extracts
   `_atlas = instance->data.atlas` — so the material holds a pointer to the live `DataAtlas`.
5. The vertex pass writes `vertex.material = materialId | (transform << 16)` while `vertex.object`
   already holds the baked `font::CharId`. At frame-prep, `plan.atlas = material->getAtlas()` and
   `plan.atlas->getObjectByName(vertex.object)` resolves `{pos, tex}` → writes `vertex.tex`, offsets
   `vertex.pos`, and clears `vertex.object`. (`xenolith/renderer/basic2d/backend/vk/XL2dVkVertexPass.cc`)

Because index N is stable, the `MaterialInfo` (and thus the `MaterialId`) does **not** change as the
atlas grows; `updateInstance` refreshes the material's image + atlas under the same id via
`_materialTrackers`. Vertices keep referencing the same material; only the GPU image and the texcoords
underneath it change.

### Two id spaces (matters for the remote split)

- **`ImageObject::_index`** (uint64, global atomic counter) — the *image index* that goes into
  `MaterialInfo` / descriptors / the material hash. Forced stable for a font atlas.
- **`remote::ObjectRegistry` id** (uint64, per-connection `pointer → id` map) — the *wire handle*.
  `encodeMaterialImage` sends `reg.share(image->image)` (`xenolith/remote/XLRemoteSerialize.cc`); the
  receiver rebuilds the `ImageData` via `factory.resolveImageData(id)`.

These are different numbers. In the remote renderer the **vertex pass runs on the server**, so the
client bakes `MaterialId` (into `CmdInfo.material`) and `CharId` (into `vertex.object`) into the
serialized command list. For those to resolve, the client's font `Texture` must expose the *same*
`getIndex()` as the server's atlas image, so the client `MaterialInfo` hashes to the server-pushed font
`MaterialId`. The `DataAtlas` itself never crosses the wire — only the matching index is needed; the
server's own atlas resolves the `CharId`s.
