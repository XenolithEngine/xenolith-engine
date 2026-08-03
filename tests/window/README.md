# Example GUI application on the Vulkan API

The application builds a basic graphics scene and draws it with the Vulkan API in an OS window.

## Requirements

Build tools: a C and a C++ compiler, make. Vulkan installed system-wide, or the
[Vulkan SDK](https://github.com/libstappler/libstappler-doc/blob/master/docs-ru/other/vulkan.md).

## Layout

    Makefile                  project description
    src/AppSetup.cpp          application entry point and command-line hooks
    src/ExampleScene.cpp,.h   the base scene
    src/TestRegistry.cpp,.h   the list of demo layouts - the single place a test is declared
    src/TestLayout.cpp,.h     common base of every layout: caption, stylesheet, socket commands
    src/*Layout.cpp,.h        the demo layouts themselves
    resources/                bundled assets
    client/                   companion app for the shared-queue (remote rendering) demo

## Building

With the Vulkan SDK:

```
make VULKAN_SDK_PREFIX=<platform prefix inside the SDK>
```

With system-wide Vulkan:

```
make
make install
```

A successful build looks like this:

```
Build for x86_64
Build executable: stappler-build/host/debug/gcc/testapp
Enabled modules: xenolith_backend_vkgui xenolith_renderer_material2d stappler_build_debug_module xenolith_backend_vk xenolith_renderer_basic2d xenolith_renderer_basic2d_shaders xenolith_scene xenolith_font xenolith_application xenolith_platform xenolith_resources_icons xenolith_core  stappler_font stappler_bitmap stappler_brotli_lib stappler_vg stappler_tess stappler_geom stappler_data stappler_filesystem stappler_core
Modules was updated
[glslangValidator] xl_2d_material.frag/main.frag
...
[testapp: 100% 29/29] [g++] main.o
[Link] stappler-build/host/debug/gcc/testapp
```

The resulting application lands in `stappler-build/host/debug/gcc/testapp`

## Running

By default the application opens an OS window, so it has to run on a system with graphical output.

```
$ stappler-build/host/debug/gcc/testapp --help
testapp <options>:
Options:
  -v, --verbose / -q, --quiet / -h, --help
  -W<#>, --width <#>      - Window width
  -H<#>, --height <#>     - Window height
  -D<#.#>, --density <#.#>  - Pixel density for a window
  --l <locale>, --locale <locale>
  --bundle <bundle-name>
  --gapi <api>            - Select graphics API backend (vulkan, webgpu, metal)
  --renderdoc / --novalidation / --device <#>
  --headless              - Run without a window system: render into offscreen
                            images and accept control over the inspector socket
  --decor <decoration-description>
$ stappler-build/host/debug/gcc/testapp
```

![Sample output](sample.png)

## Headless runs and screenshots

With `--headless` the application starts with no window system at all: Vulkan draws into a
pseudo-swapchain of ordinary device images. That works over SSH, in CI, on a server, with the
monitor asleep.

In this mode the scene inspector socket takes the place of the window: it is how the scene graph is
inspected, the log is read, layouts are switched, commands are sent to them and the "screen" is
captured.

### Starting it

```sh
cd tests/window
XENOLITH_INSPECTOR_ADDRESS=unix:/tmp/xl-$$.sock \
	./stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp \
		--headless --width 1024 --height 768 &
```

Things worth knowing:

* **Use a per-run socket path.** The default one (`/tmp/xenolith-inspector.sock`) is unconditionally
  unlinked before bind, so two running applications silently take it from each other. The client
  gets the same `XENOLITH_INSPECTOR_ADDRESS`.
* **The working directory is `tests/window`.** Resources (`resources/xenolith-2-480.png`, fonts) are
  resolved against it; `Fail to add image: ..., file not found` in the log means the wrong cwd, not
  a broken headless mode.
* **`--width/--height` are the surface in pixels.** `--density` only changes the logical scale and
  does not multiply the surface: to reproduce a 1024×768 window on a HiDPI screen, pass
  `--width 2048 --height 1536 --density 2`.
* **Stop it with the `quit` command**, not with a signal: it closes the window, tears the context
  down and exits with status 0.

### The application's commands

| Command | Arguments | What it does |
|---|---|---|
| `layouts` | — | the list of layouts: `name`, `title`, `description`, `env`, `hideFps` |
| `layout` | `name`, `settle` (seconds, 1.0 by default) | switch the layout; the answer arrives once the new one has been rendering for `settle` seconds |

The answer to `layout` *is* the "you may shoot now" signal: the command deliberately does not reply
before the scene has laid out and finished its entry animations. That is why the script below needs
neither a `sleep` nor a tuned delay.

The layout on screen adds its own commands under a `<name>.` prefix - the same actions its control
bar offers (`TestLayout::registerCommands`). For example:

| Command | What it does |
|---|---|
| `flex.mode` | flexbox ↔ grid; likewise `flex.dir`, `flex.wrap`, `flex.justify`, `flex.align` |
| `pug.theme` | swap the whole stylesheet (light ↔ dark); likewise `pug.accent`, `pug.rebuild` |
| `fit-content.append` | extend a label: `{ "count": 2 }`; likewise `fit-content.wrap` |

A layout's commands live exactly as long as it is on screen; the protocol command `commands` lists
whatever is registered at the moment. Each of them takes its own `settle` too (0.5 s by default).

### Talking to the socket

The easiest way is the `scene-inspector` MCP server together with the `gui-debug` skill: they
provide `inspect_scene`, `get_logs`, `list_commands`, `invoke_command`, `screenshot` and `quit_app`
ready to use.

A client of your own is a one-line handshake followed by `[u32 LE size][JSON]` frames:

```python
import json, socket, struct, base64

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/xl-1234.sock")
s.sendall(b"xenolith/1 json\n")          # answer: "# xenolith/1 ok json\n"
buf = s.recv(4096).split(b"\n", 1)[1]

def call(cmd, serial=[0], **args):
    serial[0] += 1
    req = dict(args, serial=serial[0], cmd=cmd)
    payload = json.dumps(req).encode()
    s.sendall(struct.pack("<I", len(payload)) + payload)
    global buf
    while True:                           # answers may arrive out of order
        while len(buf) < 4 or len(buf) < 4 + struct.unpack("<I", buf[:4])[0]:
            buf += s.recv(65536)
        size = struct.unpack("<I", buf[:4])[0]
        resp, buf = json.loads(buf[4:4 + size]), buf[4 + size:]
        if resp.get("serial") == serial[0]:
            return resp

# scene commands are reached through the `invoke` protocol command
call("invoke", name="layout", args={"name": "flex"})
call("invoke", name="flex.mode")
png = call("screenshot")["result"]["data"]        # "BASE64:<base64url, unpadded>"
open("flex-grid.png", "wb").write(
        base64.urlsafe_b64decode(png[7:] + "=" * (-len(png[7:]) % 4)))
call("quit")
```

### The whole sweep: one screenshot per layout

```python
for it in call("invoke", name="layouts")["result"]["layouts"]:
    call("invoke", name="layout", args={"name": it["name"]})   # returns once it has settled
    data = call("screenshot")["result"]["data"]
    ...                                                        # save as it["name"] + ".png"
call("quit")
```

This fully replaces the earlier environment-driven batch mode (`XL_SCREENSHOT_TESTS`/`DIR`/`DELAY`,
`XL_FLEX_GRID`, `XL_PUG_DARK`, `XL_FITCONTENT_APPEND`): that one shot a list fixed up front in a
single run and then exited, whereas the same thing is now driven from outside, in any order.

## Android

To run on Android, import the gradle project from the proj.android directory into Android Studio.

How applications work on Android is explained
[here](https://github.com/libstappler/libstappler-doc/blob/master/docs-ru/other/android.md#%D1%81%D0%BE%D0%B7%D0%B4%D0%B0%D0%BD%D0%B8%D0%B5-%D0%BF%D1%80%D0%B8%D0%BB%D0%BE%D0%B6%D0%B5%D0%BD%D0%B8%D1%8F-%D0%BD%D0%B0-android).

## Mac

The XCode project lives in the proj.macos directory.

To create a new one:

* Create an application project in XCode
* Delete the automatically generated code
* Turn off user script sandboxing (Build Settings -> User Script Sandboxing -> No)
* Add a user script to Build Phases, as early in the list as possible. Its body:
  `make -C .. mac-export RELEASE=1` - replace `..` with the path from the XCode project file to the
  project's Makefile.
* Run the build (it will fail)
* Add the generated macos.projectconfig.xcconfig to the project and assign it as the configuration
  of the build target (not of the project!) for both Debug and Release
* Add libstappler-root/core/proj.macos/core.xcodeproj and
  libstappler-root/xenolith/proj.macos/xenolith.xcodeproj as subprojects
* Add the libstappler-core and libstappler-xenolith targets as Target Dependencies
* Link the module libraries the application needs (Link Binary With Libraries)
* Add the project sources to XCode for building (Compile Sources)

To add Vulkan:

* Add the libstappler-root/xenolith/proj.macos/vulkan directory to be copied
  (Copy Bundle Resources -> Add others)
* Add the SDK libraries to the XCode project: libMoltenVK.dylib,
  libVkLayer_khronos_validation.dylib, libvulkan.1.dylib, libvulkan.<SDKVER>.dylib (do not add them
  to the target in the dialog!)
* Add libMoltenVK.dylib, libvulkan.1.dylib, libvulkan.<SDKVER>.dylib to General -> Frameworks,
  Libraries and Embedded Content (press + -> Add others)
* Add libVkLayer_khronos_validation.dylib under Build Phases -> Embed Libraries

To keep the project portable in git:

* Open the project file in a text editor (*.xcodeproj -> Show Package Contents -> project.pbxproj)
* Replace absolute paths starting with /Users with relative ones
