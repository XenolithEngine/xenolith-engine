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

#include "XLCommon.h" // IWYU pragma: keep

#include "particles/ParticleLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

struct ParticleLocaleInfo {
	StringView id;
	StringView nativeName;
};

static constexpr ParticleLocaleInfo s_locales[] = {
	{StringView("en-us"), StringView("English")},
	{StringView("ru-ru"), StringView("Русский")},
};

static constexpr size_t s_localeCount = sizeof(s_locales) / sizeof(s_locales[0]);

static size_t s_current = 0;

static void syncParticleLocale();

void defineParticleLocales() {
	using locale::define;

	define("en-us",
			{
				pair("Particles:Preset", "Preset"),
				pair("Particles:Emitter", "Emitter"),
				pair("Particles:Restart", "Restart"),
				pair("Particles:Reset", "Reset"),
				pair("Particles:Unavailable", "GPU particles are unavailable"),

				pair("Particles:Section:Emission", "Emission"),
				pair("Particles:Section:Time", "Time"),
				pair("Particles:Section:Direction", "Direction and speed"),
				pair("Particles:Section:Acceleration", "Accelerations"),
				pair("Particles:Section:Rotation", "Rotation and size"),
				pair("Particles:Section:Color", "Color"),
				pair("Particles:Section:Texture", "Animation and texture"),
				pair("Particles:Section:Flags", "Flags"),

				pair("Particles:Count", "Count"),
				pair("Particles:Explosiveness", "Explosiveness"),
				pair("Particles:Randomness", "Randomness"),
				pair("Particles:Origin", "Origin"),
				pair("Particles:Lifetime", "Lifetime"),
				pair("Particles:Fps", "Steps per second"),
				pair("Particles:Seed", "Fixed seed"),
				pair("Particles:UseLifetimeMax", "Cycle = max lifetime"),
				pair("Particles:Direction", "Direction, spread"),
				pair("Particles:Velocity", "Velocity"),
				pair("Particles:LinearVelocity", "Linear velocity"),
				pair("Particles:Gravity", "Gravity"),
				pair("Particles:Acceleration", "Acceleration"),
				pair("Particles:RadialAcceleration", "Radial acceleration"),
				pair("Particles:TangentialAcceleration", "Tangential acceleration"),
				pair("Particles:OrbitalVelocity", "Orbital velocity"),
				pair("Particles:RadialVelocity", "Radial velocity"),
				pair("Particles:Angle", "Angle"),
				pair("Particles:AngularVelocity", "Angular velocity"),
				pair("Particles:Scale", "Scale"),
				pair("Particles:ParticleSize", "Particle size"),
				pair("Particles:AlignWithVelocity", "Align with velocity"),
				pair("Particles:Color", "Color"),
				pair("Particles:Hue", "Hue shift"),
				pair("Particles:Texture", "Texture"),
				pair("Particles:FrameGrid", "Frame grid"),
				pair("Particles:LocalCoords", "Local coordinates"),
				pair("Particles:OrderByLifetime", "Newest on top"),

				pair("Particles:Preset:fire", "Fire"),
				pair("Particles:Preset:smoke", "Smoke"),
				pair("Particles:Preset:fountain", "Fountain"),
				pair("Particles:Preset:snow", "Snow"),
				pair("Particles:Preset:explosion", "Explosion"),
				pair("Particles:Preset:vortex", "Vortex"),
				pair("Particles:Preset:flipbook", "Flipbook"),

				pair("Particles:Texture:circle", "Soft circle"),
				pair("Particles:Texture:square", "Square"),
				pair("Particles:Texture:spark", "Spark"),
				pair("Particles:Texture:frames", "Frame sheet 4×4"),
				pair("Particles:Section:ColorCurve", "Color curve"),
				pair("Particles:ColorCurve", "Enabled"),
				pair("Particles:Stop", "Stop"),
				pair("Particles:StopPosition", "Stop position"),
				pair("Particles:StopColor", "Stop color"),
				pair("Particles:AnimCurve", "Frame curve"),
				pair("Particles:CurveParams", "Curve parameters"),
				pair("Particles:Mode:move", "Move"),
				pair("Particles:Mode:points", "Emission points"),
				pair("Particles:Drive", "Circle"),
				pair("Particles:ClearPoints", "Clear points"),
				pair("Particles:Copy", "Copy"),
				pair("Particles:Paste", "Paste"),
				pair("Particles:Save", "Save…"),
				pair("Particles:Open", "Open…"),
				pair("Particles:Curve:none", "None"),
				pair("Particles:Curve:linear", "Linear"),
				pair("Particles:Curve:easeIn", "Ease in"),
				pair("Particles:Curve:easeOut", "Ease out"),
				pair("Particles:Curve:easeInOut", "Ease in-out"),
				pair("Particles:Curve:sineEaseIn", "Sine in"),
				pair("Particles:Curve:sineEaseOut", "Sine out"),
				pair("Particles:Curve:quadEaseIn", "Quad in"),
				pair("Particles:Curve:quadEaseOut", "Quad out"),
				pair("Particles:Curve:cubicEaseInOut", "Cubic in-out"),
				pair("Particles:Curve:expoEaseIn", "Expo in"),
				pair("Particles:Curve:circEaseOut", "Circ out"),
				pair("Particles:Curve:elasticEaseOut", "Elastic out"),
				pair("Particles:Curve:backEaseInOut", "Back in-out"),
				pair("Particles:Curve:bounceEaseOut", "Bounce out"),
				pair("Particles:Curve:bezier", "Bézier"),
			});

	define("ru-ru",
			{
				pair("Particles:Preset", "Пресет"),
				pair("Particles:Emitter", "Эмиттер"),
				pair("Particles:Restart", "Перезапуск"),
				pair("Particles:Reset", "Сброс"),
				pair("Particles:Unavailable", "GPU-частицы недоступны"),

				pair("Particles:Section:Emission", "Эмиссия"),
				pair("Particles:Section:Time", "Время"),
				pair("Particles:Section:Direction", "Направление и скорость"),
				pair("Particles:Section:Acceleration", "Ускорения"),
				pair("Particles:Section:Rotation", "Поворот и размер"),
				pair("Particles:Section:Color", "Цвет"),
				pair("Particles:Section:Texture", "Анимация и текстура"),
				pair("Particles:Section:Flags", "Флаги"),

				pair("Particles:Count", "Число"),
				pair("Particles:Explosiveness", "Взрывность"),
				pair("Particles:Randomness", "Случайность"),
				pair("Particles:Origin", "Центр"),
				pair("Particles:Lifetime", "Время жизни"),
				pair("Particles:Fps", "Шагов в секунду"),
				pair("Particles:Seed", "Фиксированное зерно"),
				pair("Particles:UseLifetimeMax", "Цикл = макс. жизнь"),
				pair("Particles:Direction", "Направление, разброс"),
				pair("Particles:Velocity", "Скорость"),
				pair("Particles:LinearVelocity", "Линейная скорость"),
				pair("Particles:Gravity", "Гравитация"),
				pair("Particles:Acceleration", "Ускорение"),
				pair("Particles:RadialAcceleration", "Радиальное ускорение"),
				pair("Particles:TangentialAcceleration", "Касательное ускорение"),
				pair("Particles:OrbitalVelocity", "Орбитальная скорость"),
				pair("Particles:RadialVelocity", "Радиальная скорость"),
				pair("Particles:Angle", "Угол"),
				pair("Particles:AngularVelocity", "Угловая скорость"),
				pair("Particles:Scale", "Масштаб"),
				pair("Particles:ParticleSize", "Размер частицы"),
				pair("Particles:AlignWithVelocity", "По скорости"),
				pair("Particles:Color", "Цвет"),
				pair("Particles:Hue", "Сдвиг тона"),
				pair("Particles:Texture", "Текстура"),
				pair("Particles:FrameGrid", "Сетка кадров"),
				pair("Particles:LocalCoords", "Локальные координаты"),
				pair("Particles:OrderByLifetime", "Новые поверх"),

				pair("Particles:Preset:fire", "Огонь"),
				pair("Particles:Preset:smoke", "Дым"),
				pair("Particles:Preset:fountain", "Фонтан"),
				pair("Particles:Preset:snow", "Снег"),
				pair("Particles:Preset:explosion", "Взрыв"),
				pair("Particles:Preset:vortex", "Вихрь"),
				pair("Particles:Preset:flipbook", "Покадровая анимация"),

				pair("Particles:Texture:circle", "Мягкий круг"),
				pair("Particles:Texture:square", "Квадрат"),
				pair("Particles:Texture:spark", "Искра"),
				pair("Particles:Texture:frames", "Лист кадров 4×4"),
				pair("Particles:Section:ColorCurve", "Кривая цвета"),
				pair("Particles:ColorCurve", "Включена"),
				pair("Particles:Stop", "Точка"),
				pair("Particles:StopPosition", "Позиция точки"),
				pair("Particles:StopColor", "Цвет точки"),
				pair("Particles:AnimCurve", "Кривая кадров"),
				pair("Particles:CurveParams", "Параметры кривой"),
				pair("Particles:Mode:move", "Перемещение"),
				pair("Particles:Mode:points", "Точки эмиссии"),
				pair("Particles:Drive", "По кругу"),
				pair("Particles:ClearPoints", "Очистить точки"),
				pair("Particles:Copy", "Копировать"),
				pair("Particles:Paste", "Вставить"),
				pair("Particles:Save", "Сохранить…"),
				pair("Particles:Open", "Открыть…"),
				pair("Particles:Curve:none", "Нет"),
				pair("Particles:Curve:linear", "Линейная"),
				pair("Particles:Curve:easeIn", "Разгон"),
				pair("Particles:Curve:easeOut", "Торможение"),
				pair("Particles:Curve:easeInOut", "Разгон и торможение"),
				pair("Particles:Curve:sineEaseIn", "Синус, разгон"),
				pair("Particles:Curve:sineEaseOut", "Синус, торможение"),
				pair("Particles:Curve:quadEaseIn", "Квадратичная, разгон"),
				pair("Particles:Curve:quadEaseOut", "Квадратичная, торможение"),
				pair("Particles:Curve:cubicEaseInOut", "Кубическая"),
				pair("Particles:Curve:expoEaseIn", "Экспонента, разгон"),
				pair("Particles:Curve:circEaseOut", "Окружность, торможение"),
				pair("Particles:Curve:elasticEaseOut", "Упругая"),
				pair("Particles:Curve:backEaseInOut", "С отскоком назад"),
				pair("Particles:Curve:bounceEaseOut", "Подпрыгивание"),
				pair("Particles:Curve:bezier", "Безье"),
			});

	syncParticleLocale();
}

// The system locale may already be one of ours
static void syncParticleLocale() {
	auto current = locale::getLocale();
	for (size_t i = 0; i < s_localeCount; ++i) {
		if (current.starts_with(s_locales[i].id.sub(0, 2))) {
			s_current = i;
		}
	}
}

bool setParticleLocale(StringView id) {
	for (size_t i = 0; i < s_localeCount; ++i) {
		if (s_locales[i].id == id) {
			s_current = i;
			locale::setLocale(id);
			return true;
		}
	}
	return false;
}

StringView getParticleLocale() { return s_locales[s_current].id; }

StringView getNextParticleLocaleName() {
	return s_locales[(s_current + 1) % s_localeCount].nativeName;
}

StringView cycleParticleLocale() {
	setParticleLocale(s_locales[(s_current + 1) % s_localeCount].id);
	return getNextParticleLocaleName();
}

} // namespace stappler::xenolith::examples
