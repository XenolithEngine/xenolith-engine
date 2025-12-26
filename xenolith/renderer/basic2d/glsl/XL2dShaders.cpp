/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

// Excluded from documentation/codegen tool
///@ SP_EXCLUDE

#include "XL2dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::shaders {

#include "xl_2d_material.frag.h"
#include "xl_2d_material.vert.h"
#include "xl_2d_pseudosdf.frag.h"
#include "xl_2d_pseudosdf.vert.h"
#include "xl_2d_pseudosdf_shadow.frag.h"
#include "xl_2d_pseudosdf_shadow.vert.h"
#include "xl_2d_frameclipper.frag.h"
#include "xl_2d_particle_update.comp.h"

SpanView<uint32_t> MaterialFrag(reinterpret_cast<const uint32_t *>(xl_2d_material_frag),
		sizeof(xl_2d_material_frag) / sizeof(uint32_t));
SpanView<uint32_t> MaterialVert(reinterpret_cast<const uint32_t *>(xl_2d_material_vert),
		sizeof(xl_2d_material_vert) / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfFrag(reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_frag),
		sizeof(xl_2d_pseudosdf_frag) / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfVert(reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_vert),
		sizeof(xl_2d_pseudosdf_vert) / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfShadowFrag(
		reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_shadow_frag),
		sizeof(xl_2d_pseudosdf_shadow_frag) / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfShadowVert(
		reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_shadow_vert),
		sizeof(xl_2d_pseudosdf_shadow_vert) / sizeof(uint32_t));

SpanView<uint32_t> FrameClipperFrag(reinterpret_cast<const uint32_t *>(xl_2d_frameclipper_frag),
		sizeof(xl_2d_frameclipper_frag) / sizeof(uint32_t));

SpanView<uint32_t> ParticleUpdateComp(
		reinterpret_cast<const uint32_t *>(xl_2d_particle_update_comp),
		sizeof(xl_2d_particle_update_comp) / sizeof(uint32_t));

} // namespace stappler::xenolith::basic2d::shaders
