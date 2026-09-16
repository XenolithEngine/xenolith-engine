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
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLELOCALE_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLELOCALE_H_

#include "XLFontLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// The example's strings, English and Russian, as `@Locale:Particles:*` tags. Define them before the
// stylesheet enters the scene.
void defineParticleLocales();

// Switch to a locale by id (`en-us`, `ru-ru`); false for an unknown id
bool setParticleLocale(StringView id);

StringView getParticleLocale();

// The native name of the language the switch button changes to
StringView getNextParticleLocaleName();
StringView cycleParticleLocale();

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLELOCALE_H_ */
