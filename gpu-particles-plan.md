# План: GPU-система частиц basic2d (семантика Godot) и пример управления

Этапы именуются `Particles-N`. Решения внутри плана нумеруются `Р1…` и этапами не являются.

> **Состояние.** Закрыт (2026-09-16): Particles-1…7 выполнены. Решения, которые остаются в силе,
> перенесены в [docs/usage/basic2d/particles.adoc](docs/usage/basic2d/particles.adoc) — там их и
> искать; этот файл — история. Где текст и код расходятся — прав код.

Цель — довести существующую GPU-систему частиц `basic2d` до рабочего состояния **на уже объявленном
наборе параметров** (`ParticleEmitterData`), придать каждому параметру смысл одноимённого параметра
Godot (`GPUParticles2D` + `ParticleProcessMaterial`) и сделать приложение `examples/window/particles`,
в котором каждым параметром можно управлять и видеть результат.

Источники семантики:
- <https://docs.godotengine.org/en/stable/classes/class_gpuparticles2d.html>
- <https://docs.godotengine.org/en/stable/classes/class_particleprocessmaterial.html>
- `scene/2d/cpu_particles_2d.cpp` в godotengine/godot — CPU-эталон той же модели; из него взяты
  формулы фазы эмиссии и порядок интегрирования.

---

## 1. Текущее состояние

### 1.1. Как устроено

```
ParticleSystem (параметры, copy-on-write)          particle/XL2dParticleSystem.{h,cc}
  └─ ParticleEmitter : Sprite  ── pushCommands ──►  particle/XL2dParticleEmitter.{h,cc}
       ├─ CommandList::pushParticleEmitter          (команда с transformIndex, без геометрии)
       └─ FrameContextHandle2d::particleEmitters    (id → ParticleSystemRenderInfo)
            ▼
ParticleEmitterAttachment::handleInput              backend/vk/XL2dVkParticlePass.cc
  ParticlePersistentData::updateEmitters            (буферы эмиттера и частиц живут между кадрами)
            ▼
ParticlePass (Compute, RenderOrdering 1)            xl_2d_particle_update.comp
  один dispatch на эмиттер: эмиссия + симуляция + запись 6 вершин на частицу
            ▼
VertexPass: span с particleSystemId → cmdDrawIndirect по вершинам из compute
```

- Есть только в Vulkan и только в `QueueType::Default` (`XL2dVkShadowPass.cc:53-61`). Flat-очередь
  отбрасывает команду (`XL2dVertexPlan.cc:329`); WebGPU, Metal, GLES и soft-бэкэнды держат
  attachment-заглушку.
- Проход отключается без `shaderStorageBufferArrayDynamicIndexing` (`ParticlePass::prepare`).
- Шаг симуляции фиксирован (`frameInterval`, мкс), за вызов — не больше `_maxFramesPerCall = 2`.
- Единственный пользователь — пример `examples/window/particles` (Particles-1); тестов и документации
  нет. Дефекты §1.2 найдены чтением кода и SPIR-V, затем проверены запуском (§1.3).

### 1.2. Дефекты

| # | Дефект | Где | Статус (Particles-1) |
|---|---|---|---|
| Д1 | Эмиссия из точек закомментирована: частица всегда рождается в (0,0) | `xl_2d_particle_update.comp:86-91` | **исправлен в Particles-3**: равновероятная точка из буфера доп. данных, пустой список — `(0,0)` |
| Д2 | Не инициализируются `qAngularVelocity`, `orbitalVelocity`, `radialVelocity`, `qLinearAcceleration`, `radialAcceleration`, `tangentialAcceleration`, `hue` (закомментировано) | `.comp:110-123` | **исправлен в Particles-4**: все параметры выбираются при рождении, шаг — модель Р6 |
| Д3 | `linearVelocity` не входит в перемещение; поворот `angle` берёт неинициализированную `qAngularVelocity` | `.comp:135-137` | **исправлен в Particles-4**: одна векторная скорость `normal·velocity + linearVelocity`, угол — `angle + angularVelocity·t` |
| Д4 | Буфер частиц при создании заполняется только `rng`; остальные поля — что было в памяти | `XL2dVkParticlePass.cc:113-132` | **исправлен в Particles-2**: частица обнуляется целиком, `rng` — от ОС или от зерна |
| Д5 | Квад строится из `sizeValue` эмиттера: `scale`, `angle` частицы не применяются | `.comp:215-218` | **исправлен в Particles-4**: квад — `particleQuad`, размер × `scale`, поворот `angle` |
| Д6 | Цвет вершины жёстко `vec4(0,0,0,1)`; `color`, `colorCurve`, `hue` не используются, кривая даже не загружается на GPU | `.comp:229-247` | **исправлен в Particles-5**: цвет ноды × `color` × `colorCurve(t)`, затем поворот тона |
| Д7 | Текстурные координаты всегда 0..1 — `textureRect` спрайта и кадры анимации игнорируются | `.comp:220-223` | **исправлен в Particles-5**: UV — ячейка сетки кадров внутри `textureRect`, кадр — `animFrameCurve` |
| Д8 | **Несколько эмиттеров ломают друг друга**: у всех `outCommandPointer` — начало буфера команд и вершины пишутся с нуля, а `firstVertex` в indirect-команде — накопленное смещение. Первый эмиттер рисует вершины всех, остальные — ничего | `.comp:211`, `XL2dVkParticlePass.cc:506,523` | **исправлен в Particles-3**: у эмиттера свой диапазон вершин и своя indirect-команда |
| Д9 | Вершины пишутся через `atomicAdd` → `OpAtomicIAdd`. `spirv-val` его пропускает, но это опкод из набора OpenCL-ядер; на Vulkan-драйверах он не гарантирован. Порядок вершин к тому же недетерминирован | `.comp:162-211` | **исправлен в Particles-3**: `atomicAdd` нет, частица пишет свой слот `6·i` |
| Д10 | Ошибка в формуле: `(particleProgress - gentime) * gendt` вместо деления на `gendt` | `.comp:158` | **исправлен в Particles-3**: окно эмиссии — целочисленный шаг цикла (`particleEmitFrame`) |
| Д11 | Окно эмиссии `[gentime, gentime + gendt·n)` не переходит через 1.0: при переходе цикла частицы с малой фазой пропускаются | `.comp:157` | **исправлен в Particles-3**: цикл шага `cycle + t / framesInGen`, проверено харнесом при `nframes = 2` |
| Д12 | `randomness` смешивает случайное и «по индексу» распределение параметров — у Godot это случайный сдвиг фазы эмиссии | `.comp:71-81` | **исправлен в Particles-3**: параметры `init + rnd·frand()`, `randomness` — сдвиг фазы (Р2) |
| Д13 | При отставании рендера `clock` эмиттера растёт на `nframes·interval` и не догоняет время кадра: симуляция навсегда замедляется | `XL2dVkParticlePass.cc:492-496, 548` | **исправлен в Particles-3**: лишнее отставание отбрасывается (`particleAdvanceClock`) |
| Д14 | Любой сеттер создаёт новый `ParticleSystemData` → на рендере полное пересоздание буферов с копией частиц, даже если число частиц не менялось | `XL2dVkParticlePass.cc:92`, `:134-160` | **исправлен в Particles-2**: поколения параметров/перезапуска, буфер частиц пересоздаётся только при смене `count` (лог `vk::ParticlePass`) |
| Д15 | `addFlags/clearFlags/setFlags` не делают copy-on-write — меняют данные, уже отданные рендеру | `XL2dParticleSystem.cc:308-312` | **исправлен в Particles-2**: флаги через тот же copy-on-write |
| Д16 | `setEmissionPoints` игнорирует точки; нет сеттеров для `color`, `origin`, `hue`, кривой кадров анимации | `XL2dParticleSystem.cc:94` | **исправлен в Particles-2** на CPU и в буфере доп. данных; шейдер точки ещё не читает (Д1) |
| Д17 | Значения по умолчанию — нули (`memset`): `scale = 0`, `color = 0`, частицы невидимы | `XL2dParticleSystem.h:48` | **исправлен в Particles-2**: значения Godot, размер по умолчанию — регион текстуры эмиттера |
| Д18 | Ключ эмиттера — id системы: две ноды с одной `ParticleSystem` затирают друг друга | `XL2dParticleEmitter.cc:101-104` | **исправлен в Particles-2**: ключ — `ParticleEmitter::getEmitterId`, буферы у каждой ноды свои |
| Д19 | `LocalCoords` не реализован: частицы всегда рисуются в трансформе ноды | `XL2dParticleEmitter.cc:86-97` | **исправлен в Particles-4**: без флага частицы живут в dp контента сцены |
| Д20 | `OrderByLifetime`, `AlignWithVelocity`, `UseLifetimeMax` объявлены и нигде не читаются | — | `UseLifetimeMax` — Particles-3, `AlignWithVelocity` — Particles-4, `OrderByLifetime` — Particles-5 |
| Д21 | Поле `pcb.timeline` считается и не используется | `XL2dVkParticlePass.cc:512` | **исправлен в Particles-3**: `ParticleConstantData` — только адреса и индекс эмиттера |

### 1.3. Базовая линия (Particles-1)

Запуск `examples/window/particles` headless, 1024×720, эмиттер `count 64`, `lifetime 1.5`,
`velocity 120 + 40`, `normal π/4 + π/2`, мягкий круг 24×24; снимки через инспектор после 30, 60 и
90 кадров, затем `particles.emitters {n:2}` и `particles.restart {bare:true}`.

- Картина: чёрные квады одного размера вылетают из начала координат ноды по прямым лучам и за цикл
  выстраиваются в дугу в четверть круга (Д1, Д6, Д12). NVIDIA RTX 4070 Ti SUPER (610.57) и RADV
  (Mesa 26.2.1, `MESA_VK_DEVICE_SELECT='1002:164e!'`) рисуют одно и то же.
- Два эмиттера с разными системами оба видны (Д8 маскирован, см. таблицу).
- `shaderStorageBufferArrayDynamicIndexing` есть на обоих устройствах: строки
  `ParticleUpdateComp pipeline disabled` в логе нет.
- **Д9.** `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` (слой 1.4.357, вставка подтверждена
  `VK_LOADER_DEBUG=layer`): ни одного сообщения о шейдере, дескрипторах или `ParticlePass` ни на
  NVIDIA, ни на RADV. Офлайн `spirv-val --target-env vulkan1.2 / vulkan1.3 / spv1.5` на модуле из
  `glslang/xl_2d_particle_update.comp.h` — без ошибок; модуль объявляет только `Shader` и
  `PhysicalStorageBufferAddresses`, в нём 5 `OpAtomicIAdd` (4 под `ENABLE_FEEDBACK`, 1 безусловный
  на `vertexCount`). Практической несовместимости нет; остаётся недетерминированный порядок вершин —
  Р8 убирает `atomicAdd` по этой причине.
- Прочие ошибки валидации — не про частицы: в headless инстанс и устройство создаются без
  `VK_KHR_surface`/`VK_KHR_swapchain`, но с зависящими от них расширениями и раскладкой
  `PRESENT_SRC_KHR` (`VUID-vkCreateInstance-ppEnabledExtensionNames-01388`,
  `VUID-vkCreateDevice-ppEnabledExtensionNames-01387`, `VUID-VkAttachmentDescription-*Layout-parameter`,
  `VUID-VkImageMemoryBarrier-oldLayout-parameter`).
- `--novalidation` вопреки описанию ставит `InstanceFlags::Validation` (`XLContextInfo.cc:148-156`), но
  в headless-запуске слой всё равно не вставляется; причина не выяснялась. Для проверок —
  `VK_INSTANCE_LAYERS`.
- `FrameQueue: Fail to prepare render pass: ParticlePass` появляется один раз при закрытии окна — не
  дефект частиц.
- Баннер «частицы недоступны» различает только API и тип очереди: поддержку dynamic indexing с
  app-потока узнать нельзя, для этого нужен публичный запрос (кандидат в Particles-2).

---

## 2. Набор параметров и его смысл

Набор полей `ParticleEmitterData` сохраняется. Пара `{init, rnd}` означает диапазон
`[init, init + rnd]`, то есть Godot `*_min = init`, `*_max = init + rnd` (Р1).

| Поле | Аналог Godot | Смысл в этом плане | Единицы |
|---|---|---|---|
| `count` | `amount` | число частиц в одном цикле эмиссии; частота — `count / lifetime` | шт. |
| `frameInterval`, `dt` | `fixed_fps` | фиксированный шаг симуляции | мкс / с |
| `explosiveness` | `explosiveness` | доля цикла, в которую сжимаются рождения; 1 — все сразу | 0..1 |
| `randomness` | `randomness` | случайный сдвиг фазы рождения частицы в пределах `1/count` цикла (Р2) | 0..1 |
| `lifetime` | `lifetime` + `lifetime_randomness` | время жизни частицы в диапазоне; длина цикла — Р3 | с |
| `emissionType = Points`, `emissionData` | `emission_shape = POINTS` / `POINT` | случайная точка из списка; пустой список — точка `(0,0)` (Р15) | dp |
| `origin` | `velocity_pivot` / начало эмиттера | центр для `orbital*`, `radial*`, `tangential*` | dp, локально |
| `normal` | `direction` + `spread` | направление начальной скорости: угол из `[init, init+rnd]` (Р4) | рад |
| `velocity` | `initial_velocity` | модуль начальной скорости вдоль `normal` | dp/с |
| `linearVelocity` | ближе всего `directional_velocity` без кривой | постоянная добавка к начальной скорости в осях эмиттера (Р5) | dp/с |
| `acceleration` | `linear_accel` | ускорение вдоль **текущей** скорости | dp/с² |
| `linearAcceleration` | `gravity` | постоянное векторное ускорение | dp/с² |
| `radialAcceleration` | `radial_accel` | ускорение от `origin` | dp/с² |
| `tangentialAcceleration` | `tangential_accel` | ускорение перпендикулярно направлению от `origin` | dp/с² |
| `orbitalVelocity` | `orbit_velocity` | вращение позиции вокруг `origin`, не накапливается в скорости (Р6) | рад/с |
| `radialVelocity` | `radial_velocity` | смещение от `origin`, не накапливается в скорости (Р6) | dp/с |
| `angle` | `angle` | начальный поворот спрайта | рад |
| `angularVelocity` | `angular_velocity` | скорость поворота спрайта | рад/с |
| `scale` | `scale` | множитель базового размера | — |
| `sizeValue`, `sizeNormal` | размер текстуры | базовый размер квада (`setParticleSize`); по умолчанию — `textureRect` спрайта | dp |
| `color` | `color` | начальный цвет, умножается на текстуру | RGBA |
| `colorCurveOffset` | `color_ramp` | цвет по доле прожитой жизни, умножается на `color` | — |
| `hue` | `hue_variation` | сдвиг тона, выбирается при рождении | оборот, −1..1 |
| `animFrameCurveOffset` | `anim_offset` + `anim_offset_curve` | доля жизни → кадр листа, 0 — первый, 1 — последний (Р9) | 0..1 |
| `LocalCoords` | `local_coords` | частицы следуют за нодой; без флага живут в координатах сцены (Р7) | флаг |
| `AlignWithVelocity` | `particle_flag_align_y` | ось Y квада вдоль скорости, вместо `angle` | флаг |
| `OrderByLifetime` | `draw_order = LIFETIME` | новые частицы рисуются поверх старых (Р8) | флаг |
| `UseLifetimeMax` | — | длина цикла = верхняя граница `lifetime` (Р3) | флаг |

Углы в API — радианы, как в существующих сеттерах. Отличие от Godot, у которого градусы и обороты в
секунду для `orbit_velocity`, закрывает UI примера — он показывает градусы.

---

## 3. Решения

- **Р1. Диапазоны.** Значение параметра при рождении — `init + rnd · frand()` для каждой частицы
  отдельно (для `Vec2` — независимо по осям). Сеттеры `setX(value, rnd)` и геттеры `getXMin/Max`
  остаются. Детерминированной «раскладки по индексу» больше нет.
- **Р2. `randomness` — как в Godot.** `phase_i = i / count`; при `randomness > 0` к ней прибавляется
  `randomness · hash(seed, cycle_i, i) / count`, где `cycle_i` — цикл, в котором рождается частица;
  затем `phase_i *= 1 − explosiveness`. Хеш — `pcg`, одинаковый в GLSL и C++.
- **Р3. Цикл и время жизни.** Длина цикла — `lifetime.init`; с флагом `UseLifetimeMax` —
  `max(init, init + rnd)`. Время жизни частицы — `init + rnd · frand()`, в кадрах. Если частица ещё
  жива, когда наступает её фаза в следующем цикле, она перерождается (так делает Godot). Эквивалент
  Godot `lifetime_randomness = r` — `setLifetime(L, −L·r)` без флага.
- **Р4. Направление.** `normal` задаёт одностороннюю пару `[init, init + rnd]`. Godot-пара
  `direction = d, spread = s` — это `setNormal(d − s, 2s)`; UI показывает именно direction/spread.
- **Р5. `linearVelocity`** — часть начальной скорости: `v0 = n · velocity + linearVelocity`.
- **Р6. Модель симуляции — по `cpu_particles_2d.cpp`.** Одна векторная скорость частицы вместо
  раздельных `velocity`/`linearVelocity`. Шаг `dt`:
  ```
  diff  = pos − origin
  force = linearAcceleration
        + acceleration        · normalize(v)
        + radialAcceleration  · normalize(diff)
        + tangentialAcceleration · perp(normalize(diff))
  v    += force · dt
  pos  += v · dt
  pos   = origin + rotate(pos − origin, orbitalVelocity · dt)
  pos  += normalize(pos − origin) · radialVelocity · dt
  angle += angularVelocity · dt
  ```
  `normalize` нулевого вектора — ноль. Внутренний `ParticleData` перестраивается под эту модель
  (это состояние GPU, не набор параметров); размер остаётся кратным 16 байтам.
- **Р7. `LocalCoords`.** С флагом позиции хранятся в координатах ноды, квад рисуется через трансформ
  ноды — это нынешнее поведение. Без флага (по умолчанию Godot — `local_coords = false`) частица при
  рождении переводится в координаты сцены текущим аффинным трансформом ноды: позиция, направление
  скорости и `origin`; размер умножается на масштаб ноды. Рисуется трансформом `viewProjection` без
  модели. Трансформ ноды передаётся в compute каждый кадр (§5).
  *Уточнено в Particles-4:*
  - пространство сцены — dp `SceneContent`: `nodeToScene = content⁻¹ · model`, отрисовка
    `viewProjection · content`, потому что `modelTransform` кончается в пикселях;
  - трансформ применяется как в Godot: базис с масштабом к позиции, `origin` и начальной скорости,
    поворот ноды — к углу, `sqrt(|det|)` — к размеру;
  - `linearAcceleration` остаётся в осях сцены;
  - смена `LocalCoords` перезапускает систему.
- **Р8. Порядок отрисовки и запись вершин.** У частицы `i` фиксированный слот `6·i` в вершинах
  эмиттера; мёртвая частица пишет вырожденный квад. `atomicAdd` уходит (Д9), `vertexCount = 6·count`
  известен на CPU. Без флага порядок — по индексу. С `OrderByLifetime` слот частицы —
  `(i − newest − 1) mod count`, где `newest` — индекс последней рождённой частицы: старые рисуются
  раньше, новые поверх. При `lifetime.rnd ≠ 0` порядок по оставшейся жизни приблизительный; точная
  GPU-сортировка — вне охвата.
- **Р9. Кадры анимации.** Сетка кадров `hFrames × vFrames` — свойство ноды и текстуры (в Godot она в
  `CanvasItemMaterial`), поэтому задаётся на `ParticleEmitter`, а не в `ParticleEmitterData`. Кадр —
  `min(floor(curve(t) · N), N − 1)`, `N = h·v`; без кривой — кадр 0.
- **Р10. Кривые и точки — один буфер дополнительных данных** по адресу `emissionData`:
  `[ParticleEmissionPoints{count}][vec2 × count][curve: uint n, float × n·k]…`.
  `colorCurveOffset` и `animFrameCurveOffset` — смещения в байтах от начала буфера, `0` — кривой нет.
  64-битный адрес второго буфера в `ParticleEmitterData` положить некуда, а так поля используются
  ровно по назначению. Выборка кривой в шейдере — линейная интерполяция соседних точек.
- **Р11. Цвет.** `color · colorCurve(t)`, затем поворот тона на `2π · hue` (матрица Godot), результат —
  в `Vertex.color`. Текстура умножается во фрагментном шейдере, как у спрайта.
  *Уточнено в Particles-5:*
  - векторы матрицы Godot — строки: так сохраняются серый и яркость, положительный поворот ведёт
    красный к синему;
  - результат умножается на цвет и прозрачность ноды (`modulate`);
  - кривая хранит выборки в `i/n`, поэтому, чтобы дойти до последнего кадра, её последняя выборка
    должна быть 1.
- **Р12. Время.** Если отставание больше `maxFramesPerCall` шагов, лишнее время **отбрасывается**:
  `clock = frameClock − (frameClock − clock) mod frameInterval`. Симуляция замедляется на
  тормозящем кадре, но не отстаёт навсегда (Д13).
- **Р13. Обновление без пересоздания.** `ParticlePersistentData` различает три случая:
  изменились только параметры → перезаливка буфера эмиттера и доп. данных;
  изменился `count` → новый буфер частиц с копией пересекающейся части (как сейчас);
  `restart()` → переинициализация всех частиц. Признак — поколение параметров и поколение
  перезапуска в `ParticleSystemData`, а не сравнение указателей.
- **Р14. Идентичность.** Ключ в `particleEmitters` и в `ParticlePersistentData` — id экземпляра
  `ParticleEmitter`. Одна `ParticleSystem` может быть у нескольких нод; буферы частиц у каждой свои.
- **Р15. Эмиссия.** `Points` с непустым списком — равновероятная точка; пустой список — `(0,0)`,
  то есть Godot `POINT`. Других форм нет — см. §9.
- **Р16. Значения по умолчанию — Godot.** `count = 8`, `lifetime = 1 с`, `frameInterval = 1/60 с`,
  `scale = 1`, `color = белый`, `normal = −π/4` при `rnd = π/2` (direction 0, spread 45°), остальное — 0,
  `LocalCoords` выключен.
- **Р17. Две управляющие операции сверх набора**, без которых пример и проверки невозможны:
  `ParticleSystem::restart()` и `setSeed(uint32)` — фиксированное зерно для `rng` частиц и хеша фазы
  (в Godot 4.4 это `use_fixed_seed` / `seed`). Без зерна — как сейчас, случайные байты ОС.
- **Р18. Эталон на CPU.** Эмиссия, шаг симуляции и выборка кривых выносятся в
  `glsl/include/XL2dGlslParticleSim.h` — заголовок, который собирается и как GLSL, и как C++ через
  `sprt_glsl.h` (так уже устроен `pcg16_*`). Шейдер и консольный харнесс исполняют один и тот же
  текст.

---

## 4. Эмиссия по кадрам

За dispatch эмиттер получает `nframes ≤ maxFramesPerCall` шагов с началом в `genframe` цикла из
`framesInGen` кадров. Частица `i` проходит шаги по одному:

```
for k in 0..nframes:
    t0 = (genframe + k) / framesInGen,  t1 = t0 + 1 / framesInGen       // может перейти через 1.0
    cycle_k = cycle + (t0 ≥ 1 ? 1 : 0)
    if phase_i(cycle_k) ∈ [t0, t1) по модулю 1:                        // Д10, Д11
        emit(i)                                                         // Р1, Р5, Р7, Р15
    elif alive(i):
        step(i)                                                         // Р6
        life(i) −= 1
```

`cycle` ведётся на CPU и растёт при переходе `genframe` через `framesInGen`. Вершины пишутся после
цикла для всех частиц (Р8). Для `explosiveness = 1` все фазы равны 0 и частицы рождаются в первом
шаге цикла.

---

## 5. Данные на GPU

- **`ParticleEmitterData`** — поля не меняются. Используются `padding*` только если выравнивание
  потребует; новых параметров нет. Заливается при изменении параметров (Р13).
- **Буфер доп. данных** — точки и кривые (Р10).
- **`ParticleData`** (112 байт, Particles-4) — состояние частицы по Р6: `rng`, `position`,
  `velocity`, `origin`, `color`, `angle`, `angularVelocity`, `scale`, `hue`, `fullLifetime`,
  `currentLifetime`, `orbitalVelocity`, `radialVelocity`, `linearAcceleration`, `acceleration`,
  `radialAcceleration`, `tangentialAcceleration` — все выбраны при рождении. При создании
  обнуляется целиком (Д4).
- **`ParticleFrameData`** — новый хост-видимый буфер кадра, запись на эмиттер. С Particles-3:
  `emitterPointer`, `vertexOffset`, `particleBufferIndex`, `materialIndex`, `framesInGen`, `genframe`,
  `nframes`, `cycle`, `seed`, `dt`. С Particles-4: трансформ ноды `transformX/transformY` (строки
  `vec4`), `transformRotation`, `transformScale`. С Particles-5: `textureRect`, `nodeColor`,
  `hFrames/vFrames`, `newest` — 144 байта. Push-константы сокращаются до адресов буферов и индекса
  эмиттера — иначе пришлось бы укладываться в гарантированные Vulkan 128 байт.
- **Выход** — общий буфер вершин и массив `ParticleIndirectCommand`; каждый эмиттер пишет в свой
  диапазон и свою команду (Д8). `vertexCount` заполняет CPU.
- **Обратная связь** (Particles-6c):
  - **Состояние цикла** (`cycle`, кадр цикла, `nframes`, порядковый номер кадра) эмиттер получает
    всегда, когда включён `ParticleEmitter::setFeedbackEnabled`.
  - **Счётчики** — только из второго конвейера `ParticleUpdateFeedbackComp`: тот же шейдер с
    `ENABLE_FEEDBACK` (spec constant id 1). Он собирается, только если задано
    `XL_PARTICLE_FEEDBACK=1`.
    - Каждая частица пишет свою запись `ParticleFeedbackRecord{births, steps, alive}` в
      host-visible буфер по адресу `ParticleFrameData::feedbackPointer`, CPU суммирует.
    - Атомарных операций нет, порядок вершин не меняется.
  - **Снимок** первых N частиц — `cmdCopyBuffer` из буфера частиц после симуляции в том же командном
    буфере. От переменной окружения не зависит.
  - **Доставка:** `Fence::addRelease` на потоке GL-цикла → `ParticleFeedbackReceiver` →
    `performOnAppThread`.

---

## 6. Приложение `examples/window/particles`

Одна программа, Vulkan, `QueueType::Default`. На wasm `Makefile` останавливается с `$(error)`, как
пример `form` на wasm64: частиц в WebGPU нет.

### 6.1. Экран

```
┌────────────────────────────────────────────┬────────────────────────────┐
│ Сцена                                      │ Пресет [Fire ▾] ⟳ Перезапуск│
│   сетка, маркер ноды и origin,              │ ▸ Эмиссия                  │
│   точки эмиссии                             │ ▸ Время                    │
│                                            │ ▸ Направление и скорость   │
│        ✦ ParticleEmitter                   │ ▸ Ускорения                │
│                                            │ ▸ Поворот и размер         │
│                                            │ ▸ Цвет                     │
│ count 256 · cycle 12 · frame 31/60 · fb ✓  │ ▸ Анимация и текстура      │
│ бэкэнд: vk / Default                        │ ▸ Флаги                    │
└────────────────────────────────────────────┴────────────────────────────┘
```

- **Сцена.** Нода-эмиттер перетаскивается мышью — видна разница `LocalCoords`; режим «ведение по
  кругу» двигает ноду автоматически. Клик в режиме «точки» добавляет точку эмиссии, правый клик —
  удаляет, кнопка очищает список. Маркер `origin` перетаскивается отдельно.
- **Панель** — `ui::AccordionView`, разделы по таблице §2. Диапазон — пара `ui::NumberField`
  «мин / макс» плюс `ui::Slider` на минимум; `Vec2` — `ui::VectorField`; цвет — `ui::ColorField`;
  флаги — `ui::Checkbox`; `normal` — «направление / разброс» (Р4). Углы — в градусах.
- **Кривая цвета** — список опорных точек (позиция `Slider` + `ColorField`, добавить/удалить) и полоса
  предпросмотра, собранная из `CurveBuffer`. **Кривая кадров** — `ui::Select` типа интерполяции
  (`interpolation::Type`) и её параметры; сетка кадров — два `NumberField`.
- **Текстуры** строятся процедурно при старте (без бинарных ассетов): мягкий круг, квадрат,
  искра, лист 4×4, где каждый кадр — свой однотонный цвет. Последний нужен проверкам: кадр
  анимации определяется по цвету пикселя.
- **Пресеты** — `data::Value`: Fire, Smoke, Fountain, Snow, Explosion (`explosiveness = 1`),
  Vortex (`orbital` + `radial`), Flipbook. Сохранение и загрузка JSON — через `DialogRequest` и буфер
  обмена.
- **Строка состояния** — число частиц, цикл, кадр цикла, счётчики обратной связи при
  `XL_PARTICLE_FEEDBACK=1`, бэкэнд и очередь. Если частицы недоступны (не Vulkan, Flat,
  нет dynamic indexing), сцена показывает причину вместо пустого поля.
- **Команды инспектора** (`list_commands` / `invoke_command`): `particles.preset <name>`,
  `particles.set <param> <json>`, `particles.restart`, `particles.seed <n>`, `particles.move <x> <y>`,
  `particles.stats`, `particles.snapshot <n>`. Ими проверки ведут приложение без мыши.

### 6.2. Что уходит в движок

Сериализация `ParticleSystem ↔ data::Value` (`encode()` / `init(const Value &)`) — в `basic2d`, рядом
с системой: формат пригодится студии. Имена ключей — имена полей §2; кривые — список точек и тип.
В примере остаются UI, пресеты и процедурные текстуры.

Раскладка файлов — по образцу `examples/window/form`: `Makefile`, `main.cpp` (сцена и конфиг окна),
`src/particles/ParticleDemoLayout.{h,cpp}`, `ParticleParamsPanel.{h,cpp}`,
`ParticlePresets.{h,cpp}`, `ParticleTextures.{h,cpp}`, `ParticleLocale.{h,cpp}` (строки тегами,
en + ru).

---

## 7. Этапы

### Particles-1 — каркас примера и базовая линия ✓

Выполнен 2026-09-15: пример, команды `particles.restart` / `particles.stats` / `particles.emitters`,
результаты — §1.2 (статусы) и §1.3.

- `examples/window/particles`: окно, сцена `Default`, одна нода `ParticleEmitter` с процедурной
  текстурой, команды `particles.restart` и `particles.stats`, баннер «частицы недоступны».
- Снять скриншоты текущего состояния headless (`--headless`, `step_frame`, `screenshot`) и
  записать в §1 подтверждённые запуском дефекты; неподтверждённые пометить.
- Проверить валидационными слоями Vulkan нынешний `OpAtomicIAdd` (Д9) — результат записать.

Готово: пример собирается через `xenolith-cli`, стартует headless, скриншот снимается.

### Particles-2 — данные и API (`particle/`, `backend/vk`) ✓

Выполнен 2026-09-15. Сверх списка: размер частицы по умолчанию — регион текстуры эмиттера
(`ParticleSystemRenderInfo::defaultSize`, подставляется при заливке, если система размер не задала);
снимок кадра в `ParticleEmitterAttachmentHandle` (§10); `TransferSrc` у буфера частиц — копия при
смене `count` без него нарушала `VUID-vkCmdCopyBuffer-srcBuffer-00118`. Команды примера:
`restart`, `reset {defaults}`, `emitters {n, shared}`, `set {…encode}`, `seed {n}`, `stats`;
при старте — самопроверка `encode → init(Value) → encode`.

- Р16 (значения по умолчанию), Д15 (copy-on-write флагов), Д16 (точки сохраняются, сеттеры
  `setColor`, `setOrigin`, `setHue`, `setAnimFrameCurve`), Р17 (`restart`, `setSeed`).
- Р14: ключ по экземпляру эмиттера.
- Р13: поколения параметров и перезапуска; три пути обновления в `ParticlePersistentData`.
- Р10: буфер доп. данных — точки и кривые; загрузка `colorCurve`.
- Д4: обнуление частиц; инициализация `rng` от зерна.
- Сериализация `ParticleSystem ↔ data::Value` (§6.2).

Готово: смена любого параметра, кроме `count`, не пересоздаёт буфер частиц (видно по счётчику
аллокаций в логе); две ноды с одной системой живут независимо.

### Particles-3 — цикл эмиссии и запись вершин (`glsl/`, `backend/vk`) ✓

Выполнен 2026-09-15.
- Эмиссия и шаг вынесены в `glsl/include/XL2dGlslParticleSim.h`: хеш фазы, `particleEmitFrame`,
  `particleCycleFrames`, `particleEmit`, `particleUpdate`, `particleSeedRng`, а для C++ ещё время и
  позиция цикла. Шейдер и харнес исполняют этот текст.
- Окно эмиссии сравнивается целыми номерами шагов цикла, без float-границ.
- Время жизни L шагов покрывает шаг рождения и ещё L − 1 шаг. Частица, у которой на шаге её фазы
  остаётся 1, умирает и рождается снова в одном шаге.
- Обратная связь из шейдера удалена до Particles-6.
- Найдено и исправлено попутно:
  - `pcg16_random_r` в `sprt_glsl.h` возвращал 32 бита вместо 16: вращение не маскировалось, поэтому
    `pcg16_random_float_r` выдавал значения до ~65536;
  - в `pcg16_boundedrand_r` был неверный порог;
  - `pcg16_random_full_r` полагался на порядок вычисления операндов;
  - `ParticleSystem::apply` из Particles-2 обнулял отсутствующие во входе диапазоны (`Value::Null`
    — «базовый тип»). Пример теперь проверяет частичный `apply` при старте.
- Харнес: `tests/particles` (`particlestest`, 132 проверки), в `tests/run-checks.py` и
  `docs/agents/test-projects.md`.

- `XL2dGlslParticleSim.h` (Р18) и перенос в него эмиссии.
- §4: пошаговая эмиссия с переходом через цикл (Д10, Д11), Р2 (`randomness`), Р3 (`UseLifetimeMax`),
  Р15 (точки).
- Р8 и §5: фиксированные слоты, `ParticleFrameData`, отдельная команда на эмиттер (Д8, Д9, Д21).
- Р12: отбрасывание отставания (Д13).
- Консольный харнес `tests/particles` (только runtime + заголовок): на CPU-эталоне проверяет число
  живых частиц по кадрам для `explosiveness` 0/0.5/1, `randomness` 0/1, `UseLifetimeMax`, переход
  через цикл при `nframes > 1`, изменение `count` на лету. Регистрация в `tests/run-checks.py` (`CLI`,
  `OWES` для `xenolith/renderer/basic2d/particle` и `glsl/include/XL2dGlslParticle*`).

Готово: харнес зелёный; в примере два эмиттера с разными пресетами рисуются одновременно.

### Particles-4 — кинематика (`glsl/`, `particle/`) ✓

Выполнен 2026-09-16.
- Модель шага Р6 и начальная скорость Р5 — в `particleEmit`/`particleStep`
  (`XL2dGlslParticleSim.h`); квад — `particleQuad` с `scale`, `angle` и `AlignWithVelocity`.
- Режим координат сцены — см. уточнение в Р7.
- Харнес сравнивает траекторию одной частицы с дискретной замкнутой формой по каждому параметру
  (170 проверок). Сюда входят углы квада и перевод при рождении в режиме сцены.
- В примере:
  - пресеты `fountain`, `snow`, `vortex` (`ParticlePresets.{h,cpp}`);
  - команды `particles.preset`, `particles.move`, `particles.drive`;
  - `particles.drive` без `localCoords` оставляет кольцевой шлейф, с флагом облако едет с нодой.

- Р6: вся модель шага — `velocity`, `linearVelocity`, четыре ускорения, `orbital`, `radial`.
- `angle`, `angularVelocity`, `scale`, `AlignWithVelocity` (Д3, Д5, Д20).
- Р7: `LocalCoords` и режим координат сцены (Д19), трансформ ноды в `ParticleFrameData`.
- Харнес: траектория одной частицы на CPU-эталоне против аналитики для каждого параметра по
  отдельности (равноускоренное движение, окружность при `orbital`, прямая при `radial`).

Готово: пресеты Fountain, Snow и Vortex выглядят как задуманы; перетаскивание ноды оставляет шлейф
без `LocalCoords` и уносит частицы с флагом.

### Particles-5 — внешний вид (`glsl/`, `particle/`, `XL2dVertexPlan`) ✓

Выполнен 2026-09-16.
- Функции в `XL2dGlslParticleSim.h`: выборка кривой из буфера доп. данных, поворот тона, цвет, кадр
  анимации, ячейка сетки. Для C++ добавлен `particleNewest`.
- `ParticleEmitter::setFrameGrid`; кадр получает `textureRect`, сетку, цвет ноды, `newest`.
- Слот частицы с `OrderByLifetime` — `(i − newest − 1) mod count`.
- Найдено и исправлено попутно:
  - частицы были перевёрнуты по V относительно спрайта;
  - эмиттер с непрозрачной текстурой получал уровень `Solid`, и альфа частиц игнорировалась.
    Теперь по умолчанию он `Transparent`;
  - цвет ноды не доходил до частиц.
- Харнес: кривые, поворот тона против матрицы Godot, кадры, ячейки, `particleNewest` (207 проверок).
- В примере:
  - текстура-лист 4×4 и пресеты `fire`, `smoke`, `flipbook`;
  - на скриншоте Flipbook найдены все 16 цветов кадров;
  - с `orderByLifetime` новые кадры лежат поверх.
- Ограничения:
  - аддитивное смешивание недоступно: в очереди Default нет конвейера для `BlendFactor::One`, и
    спрайт с таким смешиванием не рисуется (`No pipeline for attachment 'MaterialInput2d'`).
    Fire идёт на обычном альфа-смешивании;
  - текстура, созданная `addExternalImage` с нулевым временем жизни и не назначенная сразу,
    выгружается. В примере — 600 с.

- Р11: `color`, `colorCurve`, `hue` (Д6).
- `textureRect` спрайта; Р9: сетка кадров на `ParticleEmitter`, `animFrameCurve` (Д7).
- Р8: `OrderByLifetime`.
- Харнес: выборка кривой и поворот тона против эталонных значений.

Готово: Fire и Smoke меняют цвет за жизнь; Flipbook проходит все 16 кадров.

### Particles-6 — приложение управления (`examples/window/particles`) ✓

По решению пользователя разбит на три этапа.

#### Particles-6a — панель, пресеты, команды, локаль ✓

Выполнен 2026-09-16.
- **Таблица параметров** `ParticleParams.{h,cpp}`:
  - вид, единицы UI, диапазоны, секция, тег локали;
  - `particleParamToUi`/`particleParamFromUi` — единственное место перевода единиц (градусы, шаги в
    секунду, направление/разброс по Р4, флаги, зерно, свойства ноды `texture`/`frameGrid`).
- **Панель** `ParticleParamsPanel.{h,cpp}`:
  - `AccordionView` из 8 секций;
  - виджеты по виду: `NumberField` (+`Slider`), `VectorField`, `ColorField`, `Checkbox`, `Select`;
  - изменение виджета → патч → `ParticleSystem::apply`;
  - изменения из команд панель подхватывает по поколению параметров системы.
- **Пресеты:** 7 штук (добавлен Explosion); текстуры квадрат и искра; локаль en/ru тегами
  (`ParticleLocale.{h,cpp}`).
- **Команды:** `particles.panel.state`, `particles.panel.set`, `particles.panel.section`,
  `particles.locale`.
- **Проверка:**
  - самопроверка `panel ok`;
  - скрипт по всем 28 параметрам в обе стороны и три пресета (117 проверок);
  - настоящий ввод — перетаскивание слайдера и клик по чекбоксу.
- **Найдено в движке:** вьюпорт `AccordionView` не догонял ширину, которую раскладка родителя задавала
  после его фазы размера. Исправлено синхронизацией в `handleLayoutChildren`.
- **Особенности кита:**
  - открытая секция в `Fit` ровно `minSize.height` вместе с заголовком;
  - вложенный flex-ряд измеряется по подписям, а не по полям — строкам нужна явная высота.

#### Particles-6b — сцена мышью, редакторы кривых, JSON ✓

Выполнен 2026-09-16. Движок не менялся.
- **Модель редакторов** — ключи примера сверх `ParticleSystem::encode`, `editor`:
  - `colorStops: [{t, color}…]` → `colorCurve` (32 выборки, линейно между точками);
  - `animCurve: {type, params}` → `animFrameCurve` (64 выборки `interpolateTo`, последняя — 1;
    `none` — без кривой);
  - `ParticleCurves.{h,cpp}`: таблица 16 типов интерполяции с числом параметров и умолчаниями;
    `deriveColorStops` — до 8 точек из готовой кривой без редакторских данных.
- **Единая точка изменений** `ParticleDemoLayout::applyPatch`: ключи ноды → ключи редактора →
  `ParticleSystem::apply`. Через неё идут панель, команды, JSON и пресеты; пресеты несут
  `colorStops` (Fire, Smoke, Explosion) и `animCurve` (Flipbook) вместо готовых выборок.
- **Панель:**
  - секция «Кривая цвета»: флажок, полоса предпросмотра из 16 `Layer` с `SimpleGradient`, выбор
    точки, «+»/«−», позиция (`NumberField` + `Slider`) и цвет точки;
  - в «Анимации и текстуре» — тип кривой кадров и 4 параметра;
  - шапка: режим сцены (перемещение | точки), «По кругу», «Очистить точки», «Копировать»,
    «Вставить», «Сохранить…», «Открыть…».
- **Сцена** — маркеры выбранного эмиттера детьми его ноды: рамка-ручка, ромб `origin`, квадраты
  точек эмиссии.
  - ручка: тап выбирает, свайп перетаскивает ноду;
  - ромб: свайп двигает `origin`;
  - фон сцены в режиме «точки»: левый тап добавляет точку, правый удаляет ближайшую в 16 dp.
  - Смещение захвата считается от `input->originalLocation`: при `Began` свайп уже отошёл на порог,
    и от `location()` нода отставала от курсора на этот порог.
- **JSON:** `{version: 1, preset, system, node, editor}`; загрузка перезапускает систему.
  - Файлы — `DialogRequest` `SaveFile`/`OpenFile` через `AppWindow::openDialog`; без поддержки
    диалогов причина пишется в строку состояния.
  - Буфер обмена — `ClipboardSession` лейаута.
- **Команды:** `particles.points`, `particles.mode`, `particles.select`, `particles.json.get/set/save/load`,
  асинхронные `particles.copy`/`particles.paste`; `particles.set` и `particles.panel.set` принимают
  `colorStops`/`animCurve`.
- **Проверка:**
  - самопроверки `roundtrip ok`, `panel ok`;
  - скрипт команд (17 проверок): точки пресетов в панели, `panel.set` кривых, выключение кривой,
    `points`, `json.get → json.set`, `json.save/load` и отказ на отсутствующем файле,
    `copy → preset → paste`;
  - настоящий ввод (6 проверок): перетаскивание ноды (+100, +100 dp точно) и `origin`, левый/правый
    клик в режиме точек, клик в режиме перемещения ничего не добавляет, слайдер позиции точки;
  - скриншоты: маркеры; Fire с синими точками кривой; Flipbook `easeIn(3)` — пикселей кадра 0
    7915 против 1787 у `linear`;
  - валидация — только ошибки headless-фона; `tests/run-checks.py` зелёный.

#### Particles-6c — `XL_PARTICLE_FEEDBACK` и snapshot ✓

Выполнен 2026-09-16.
- **GLSL:**
  - `ParticleFrameData::feedbackPointer` на смещении 88 (144 байта сохранены). В std430 `uvec2`
    выравнивается по 8 байтам, поэтому 84 не годится; добавлен `static_assert` смещения.
  - `ParticleFeedbackRecord` — 16 байт.
  - `particleUpdate` получил `inout ParticleUpdateCounters{births, steps}` — один текст для GPU и
    харнесса.
  - Шейдер пишет запись при `ENABLE_FEEDBACK != 0` и ненулевом адресе: эмиттеры без приёмника
    записей не имеют.
- **`basic2d`:**
  - `ParticleFeedback`, `ParticleSnapshot` (с `ParticleFeedback` своего кадра), `ParticleFeedbackReceiver`
    (`deliver*` с любого потока, результат — на app-потоке, `detach` в `handleExit` завершает
    ожидающие снимки неуспехом);
  - `ParticleEmitter::setFeedbackEnabled`, `getFeedback`, итоги рождений и шагов,
    `requestSnapshot(n, cb)`;
  - `ParticleSystemRenderInfo` несёт приёмник и запрос снимка, пока кадр не ответит.
- **`vk::ParticlePass`:**
  - второй пайплайн по переменной окружения, `prepare` удаляет оба;
  - буферы записей, копия снимка с барьером compute→transfer, чтение в `addRelease` с захватом пула
    кадра.
  - **Порядок кадров** — сквозной номер `ParticlePersistentData`, он не сбрасывается при перезапуске.
    С номером эмиттера, обнулявшимся при перезапуске, приёмник отбрасывал бы новые отчёты как
    старые.
- **Пример:**
  - строка состояния `cycle C · frame F/N · fb ●alive +births/s ~steps/s` (без переменной — `fb off`);
    скорости — по итогам за полсекунды, потому что при частоте кадров выше 60 Гц в большинстве кадров
    шагов нет;
  - `feedback` в `particles.stats`;
  - асинхронная `particles.snapshot {n, emitter?}`.
- **Проверка:**
  - харнесс 212 проверок (+5: рождения и шаги счётчиков против состояния частиц, `count` рождений за
    цикл, вспышка);
  - headless без переменной (8 проверок) и с `XL_PARTICLE_FEEDBACK=1` (12 проверок):
    - монотонность номера кадра, в том числе через кадр скриншота;
    - снимки одного зерна совпадают побайтово на тех же шагах двух перезапусков (150 общих шагов);
    - `alive` равен числу живых частиц снимка того же кадра (10/10);
    - перезапуск сразу виден в отчёте;
    - Explosion: итог рождений растёт только целыми вспышками по `count`, по одной на начало цикла;
    - снимок эмиттера, удалённого до кадра, отвечает неуспехом;
  - регрессия 6a/6b (117 + 17 + 6) зелёная с переменной;
  - валидация в обоих режимах — только ошибки headless-фона;
  - `tests/run-checks.py` зелёный.

### Particles-7 — проверки и документация ✓

Выполнен 2026-09-16.
- **CPU-эталон в примере** (`ParticleReference.{h,cpp}`):
  - тот же `XL2dGlslParticleSim.h`; четыре внешние функции симуляции определены примером поверх
    буфера слов `ParticleSystemData::writeExtraData`;
  - частицы, данные эмиттера и трансформ ноды готовятся так же, как в `vk::ParticlePass`.
  - `particles.snapshot {reference: true}` добавляет эталон на шагах снимка:
    `cycle · framesInGen + cycleFrame`.
  - Шаги из `step_frame` не выводятся: часы headless — реальное время, отставание отбрасывается.
- **`tests/window/particles-check.py`** (39 проверок, 42 с):
  - 7 пресетов и набор «всё сразу» (все параметры и флаги, в сцене и с `LocalCoords` на сдвинутой
    ноде);
  - целые (`rng`, времена жизни) — точно;
  - float — p95 ≤ 1e-3 и максимум ≤ 1e-2; измерено p95 < 1e-5, максимум ~3e-3 за 200 шагов;
  - признаки изображения: частицы видны, у Flipbook 16 из 16 кадров, у Fire G/R у основания
    0.70 против 0.26 вверху;
  - воспроизводимость одного зерна;
  - второй запуск с `XL_PARTICLE_FEEDBACK=1` — `alive` против снимка и эталона, вспышки Explosion.
  - Отрицательная проверка: эталон с `dt · 1.01` краснеет в 7 сверках из 9; Snow и Flipbook
    движутся слишком медленно для 1 %.
- **`run-checks.py`:**
  - `WINDOW_BINARIES` — у проверки свой бинарник: он передаётся аргументом, несобранный пример даёт
    «not built, skipped», оставшийся процесс убивается как stale;
  - `EXAMPLE_CHECKS` — изменения `examples/window/particles` выбирают проверку;
  - по имени её выбирают `XL2dParticle*`, `XL2dVkParticlePass`, `XL2dGlslParticleSim`,
    `xl_2d_particle_update.comp`.
- **Документация:**
  - `docs/usage/basic2d/particles.adoc` — параметры против Godot, модель, обратная связь,
    ограничения, ловушки, проверки;
  - строки в `docs/usage/codestyle/index.adoc`, в маршрутах `code-style` и в `docs/agents/test-*`.
- **Найдено вне частиц: `sprt::dtoa` терял нули сразу после десятичной точки** (1.0278 → `1.278`,
  1.05e21 → `1.5e21`).
  - Любой double в JSON движка с таким нулём читался обратно другим числом.
  - Проверка частиц увидела это как «выбросы», одинаковые у GPU и эталона.
  - Исправлено в `runtime/include/sprt/runtime/detail/dtoa.h` (`dtoa` и `dtoa_len` согласованно).
  - `tests/stappler` json-git теперь сверяет значение после чтения, а не только текст: круг
    write → read → write этот дефект не видел, неверный текст стабилен.
- **Gate:**
  - `tests/run-checks.py full -j4` — 40 заданий, 26435 проверок, 1 красный:
    `selection-nav-check.py`. Он флапает и на `testapp`, собранном до правки `dtoa`: 2 провала из ~7
    прогонов по отдельности.
  - После пересборки `tests/window` с новым `dtoa` `suite window -j4` — 33 задания, 1709 проверок,
    красные `selection-nav-check.py` и один раз `context-menu-check.py` («there is a menu to choose
    from»). Последний зелёный отдельно ×2 и под нагрузкой `-j4` вместе с пятью самыми долгими
    скриптами ×2.

---

## 8. Проверка

- **Консоль** (`tests/particles`, Particles-3…5) — логика эмиссии и шага на общем с шейдером коде,
  без GPU. Быстрая, входит в `run-checks.py` по умолчанию для затронутых путей.
- **GPU против эталона** (Particles-7) — снимок `ParticleData` всех частиц против того же заголовка на
  CPU, на шагах снимка, с фиксированным зерном. Целые — точно; float — p95 ≤ 1e-3, максимум ≤ 1e-2
  относительно `max(1, |v|)` (исходный допуск «`1e-3 · |pos|` на шаг» оказался слишком свободным:
  ошибка `dt` в 1 % под ним не видна).
- **Изображение** — только признаки (есть ли пиксели, цвет кадра, цвет по времени), не эталонные
  PNG: растеризация частиц тут не предмет проверки.
- Каждый этап — сборка примера и `tests/window` через `xenolith-cli`, затем `tests/run-checks.py`;
  перед коммитом — `tests/run-checks.py full`.

---

## 9. Вне охвата

Сознательно не входят и остаются кандидатами на следующий план:
- формы эмиссии `SPHERE`, `BOX`, `RING`, `DIRECTED_POINTS`;
- `damping`, `scale_over_velocity`, `anim_speed`, кривые на каждый параметр (`*_curve`),
  `color_initial_ramp`, `alpha_curve`;
- `emitting`, `one_shot`, `preprocess`, `speed_scale`, `amount_ratio`, `interpolate`, `fract_delta`;
- трейлы, суб-эмиттеры, коллизии, турбулентность, аттракторы;
- точная сортировка по оставшейся жизни;
- тени от частиц (`XL2dVertexPlan.cc:940` их пропускает);
- WebGPU, Metal, GLES, soft; передача частиц в удалённой сессии рендера
  (`FrameContextHandle2d::serialize` их пропускает).

---

## 10. Риски

- **Гонка на `ParticlePersistentData` — была реальной, закрыта в Particles-2.**
  - Потоки: `handleInput` идёт на потоке GL-цикла (`AttachmentHandle::submitInput`); команды
    пишутся на пуле потоков (`performInQueue` → `performAsync`, `XLVkQueuePass.cc`).
  - Кадры одной очереди пересекаются при нескольких окнах на одной очереди, при кадрах
    скриншотов и offscreen (`DoNotPresent`) и при инвалидации.
  - Решение: кадр пишет только снимок в `ParticleEmitterAttachmentHandle` (буферы, копия данных,
    `genframe/nframes`, staging). Постоянные данные меняются только на потоке GL-цикла: в
    `handleInput` и в `finalize`. Неуспешный кадр помечает эмиттеры на повторную заливку.
    Эмиттеры стираются только у своего владельца `(client, windowId)`.
  - Остаточные эффекты:
    - при пересечении кадров порядок submit не гарантирован, и первый кадр после spawn/resize может
      увидеть незалитый буфер — сбой на один кадр;
    - буферы эмиттеров окна, которое закрылось при живой общей очереди, не освобождаются, пока жив
      attachment.
  - Отдельно: серия `screenshot` без `frame` даёт `VUID-vkCmdDraw-None-09600` (presentation-образ в
    `TRANSFER_SRC_OPTIMAL`) — это путь захвата, не частицы.
- **Dynamic indexing буферов.** Без `shaderStorageBufferArrayDynamicIndexing` проход выключен;
  на таких устройствах пример должен это показать, а проверка — пропустить GPU-часть с явной
  пометкой, а не пройти.
- **Headless-GPU.** Проверки Particles-7 требуют Vulkan-устройства с compute; на машине без него
  (lavapipe годится) — пропуск с пометкой.
- **Размер вершинного буфера.** `6 · count · sizeof(Vertex)` на кадр для всех эмиттеров; для
  десятков тысяч частиц это заметно. Переход на инстансинг (один квад + буфер частиц в вершинном
  шейдере) — отдельное решение, если измерение в Particles-6 это покажет.
