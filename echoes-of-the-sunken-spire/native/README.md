# KayKit Native — Echoes of the Sunken Spire as native C++ PC games

Two action-RPGs that ran in the browser, rewritten as **native desktop programs**.
No engine, no SDL, no OpenGL, no JavaScript runtime — a hand-written C++17
engine with a software rasterizer, a glTF/GLB loader with skeleton animation,
X11 (Linux) and Win32 (Windows) back ends, and gamepad support.

```
src/base.h        math (vec/mat/quat), JSON, zlib-free inflate (RFC 1950/1951), PNG decode, BMP write
src/gltf.h/.cpp   glTF 2.0 + GLB loader: meshes, materials, embedded textures, skins, animation clips
src/render.h/.cpp multithreaded software rasterizer, frustum culling, depth buffer, 2D HUD overlay
src/font_data.h   5x7 bitmap font
src/platform.h/.cpp window + input: X11, Win32/XInput, or headless
src/game.h/.cpp   shared action-RPG layer: movement, OoT-style combat, lock-on, enemies, dialogue, HUD
src/worlds.cpp    the two campaigns built from the KayKit assets
src/main.cpp      CLI, frame loop, screenshots
```

## Build

Linux (g++ 9+ and the X11 headers):

```bash
make              # -> ./kaykit
```

Debian/Ubuntu: `sudo apt install build-essential libx11-dev`

Windows (MSVC): open an *x64 Native Tools Command Prompt for VS* and run

```bat
build.bat         :: -> kaykit.exe
```

There is no dependency on SDL/zlib/freetype: PNG decoding, DEFLATE inflation,
skinning and rasterization are all implemented in the sources.

## Run

The executable looks for `./assets` next to itself, and the game is chosen by
`game.txt` (`hollowmere` or `spire`) — or by what is in `assets/`.

```bash
./kaykit                                   # game from game.txt / assets
./kaykit --game hollowmere --data assets
./kaykit --game spire --data assets
```

Useful options:

```
--game hollowmere|spire    campaign to load
--data <dir>               asset root (the KayKit public/assets folder)
--width/--height            window size (default 1280x720)
--render-width/-height      internal render resolution (default 640x360, upscaled)
--play                      skip the title screen
--headless --frames N       render offscreen and exit (automated checks)
--shot out.bmp              save the last frame
--smoke                     scripted self-test; prints player/quest state
--warp X Z                  drop the player at a world position (debug)
```

Render resolution is deliberately decoupled from window size: 640x360 software
rasterization upscaled to 1280x720 keeps the framerate high and looks like the
chunky low-poly originals. `--render-width 1280 --render-height 720` gives a
crisper, slower image.

## Controls

| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move | WASD | left stick |
| Look | mouse (click to capture, ESC releases) | right stick |
| Attack | LMB | X / RT |
| Block | RMB | LT |
| Dodge / jump / glide | Space | A |
| Interact, talk, light, ring | F | B |
| Lock on | Tab | R3 |
| Emberbolt | Q | Y |
| Elixir | E | LB / D-pad up |
| Sprint | Shift | RB |
| Pause | Esc | Start |

Sword and shield are equipped from the first frame; other weapons are lying in
the world as real pickups that swap the held model and the damage stats.

## What is in each campaign

**Hollowmere** — graveyard hub, 16 skeletons, four warding lanterns to light,
Maren the NPC by the gate, and the Hollow King at the back of the yard.
Lighting all four lanterns spawns the boss.

**Echoes of the Sunken Spire** — Sunlit Valley, the Heartwood canopy, the Glass
Sinks and the Aethel-Spire itself. Ring the resonance bell, recover three
echoes (one teaches you to glide), sketch six landmarks, then climb the spiral
stair to the apex terrace and face the Pale Warden.

Art is KayKit by Kay Lousberg (CC0) and is loaded straight from the same
`assets/` folders the browser builds used.

## Portable single-file builds (the .exe)

`Hollowmere.exe` and `Echoes-of-the-Sunken-Spire.exe` are **one file each**: the
entire asset tree is baked into the executable, so there is no `assets/` folder,
no installer and no config. Copy the .exe anywhere, double-click it.

| file | assets inside | size |
|---|---|---|
| `Hollowmere.exe` | 186 files, 10.6 MB | 11.8 MB |
| `Echoes-of-the-Sunken-Spire.exe` | 1628 files, 24.8 MB | 26.1 MB |

Build them with:

```bash
./build_exe.sh all            # needs g++-mingw-w64-x86-64
./build_exe.sh hollowmere     # just one
```

### How it works

`tools/embed_assets.py` packs `assets/` into a flat blob plus an index
(`assets_index.cpp`); the blob is turned into an object file by
`ld -r -b binary` and linked in. `src/vfs.h` exposes a read-only filesystem
that `readFile()`/`fileExists()` in `base.h` consult, so the loader, the glTF/PNG
decoders and the campaign picker need no changes. Lookup is by path **suffix**
and case-insensitive, which is why the exe still finds
`characters/Knight.glb` when the code asks for `./assets/characters/Knight.glb`
on a machine where that folder does not exist. Directory entries and a
`game.txt` are embedded too, so the "which campaign is this?" probe answers
correctly with nothing on disk.

Only compiled in with `-DKAYKIT_EMBEDDED_ASSETS`; a normal `make` still reads
`./assets` from disk exactly as before.

### Things to know

* The exe is a **GUI-subsystem** binary (`-mwindows`), so double-clicking does not
  open a console window. Pass `--help`, `--headless`, `--smoke` or `--shot` on a
  command line and it attaches to the parent console so the log is visible.
* It links **statically** (`-static`), which is what removes the
  `libwinpthread-1.dll` dependency. The only imports left are Windows system
  DLLs: `KERNEL32`, `USER32`, `GDI32`, `XINPUT1_3`, `msvcrt`.
* The binaries are unsigned, so Windows SmartScreen will show
  "Windows protected your PC" on first launch: *More info -> Run anyway*.
* Verified under Wine 10: both exes build their world, render, and write a
  correct frame with **no assets folder present**, in both the headless and the
  windowed path.
* `build_exe.sh` also emits a Linux binary with the same embedded assets
  (`/tmp/kaykit-exe-<game>/kaykit-linux`) - same code path, handy for testing
  asset lookup without Windows.

### Windows build fix

`build.bat` (MSVC) could never have linked: `wndProc` was forward-declared inside
an anonymous namespace in `platform.cpp` but defined outside it, so the symbol
the window class pointed at was undefined. Both definitions now live in the same
namespace, which is why the mingw cross-build links.

## Windows release

The shipped Windows build is a single portable `.exe` per campaign - no
installer, no `assets` folder, no Visual C++ redistributable:

| release | inside | download |
|---|---|---|
| Hollowmere | `Hollowmere.exe` (11.9 MB) | `Hollowmere-windows-x64.zip` (5.0 MB) |
| Echoes of the Sunken Spire | `Echoes-of-the-Sunken-Spire.exe` (26.2 MB) | `Echoes-of-the-Sunken-Spire-windows-x64.zip` (9.0 MB) |

Each zip holds the executable, a `README.txt` with the controls, and a
`licences/` folder with the CC0 texts. Requires 64-bit Windows 7+.

What makes them read as a real Windows program rather than a bare console
binary:

* **Icon** - `win/hollowmere.ico` / `win/spire.ico`, 16 through 256 px, compiled
  in with `windres`. Shows in Explorer, the taskbar and Alt-Tab.
* **Version block** - right-click the exe, *Properties -> Details* shows the
  product name, version, description and the KayKit credit.
* **DPI manifest** - the window is not bitmap-stretched on a scaled display.
* **GUI subsystem** (`-mwindows`), so double-clicking does not open a console.
  Diagnostic flags (`--help`, `--headless`, `--smoke`, `--shot`) attach to the
  parent console so the log is still visible.
* **Static link** - the only imports are `KERNEL32`, `USER32`, `GDI32`,
  `XINPUT1_3` and `msvcrt`.

The binaries are unsigned, so SmartScreen shows *"Windows protected your PC"* →
*More info* → *Run anyway* on first launch.

### Building on Windows with MSVC

`build.bat` (from an *x64 Native Tools Command Prompt*) produces a development
`kaykit.exe` that reads assets from disk, and now also compiles the icon and
version resources. For the portable embedded-asset `.exe`, use `build_exe.sh`
(mingw-w64); it needs no Windows machine to produce a Windows binary.

Run a dev build with an assets folder beside it:

```bat
kaykit.exe --game hollowmere --data ..\game\public\assets
kaykit.exe --game spire      --data ..\spire\public\assets
```

## Why the native port looked worse than the WebGL build (fixed)

The port was not a different scene. The models, textures and lighting rig are
identical - the loss was all in the rasterizer. Measured differences and what
was done about each:

| | WebGL build | native, before | now |
|---|---|---|---|
| texture filtering | trilinear + anisotropic | **nearest, no mipmaps** | bilinear + box-filtered mip chain, per-triangle LOD |
| internal resolution | up to 2x device pixel ratio | **640x360, point-scaled to the window** | 1:1 with the window, adaptive only if slow |
| tone mapping | ACES filmic | **none** (linear, clipped highlights) | ACES-ish curve |
| window upscale | GPU bilinear | **nearest** | bilinear (and area-averaged when downscaling) |

A software rasterizer is fill-rate bound, which is why the old default was so
low: at 720p on 2 slow cores this build manages ~12 fps, so 640x360 was picked
to keep it playable. The result was a frame that was *both* point-sampled and
then point-scaled - hence "downgraded".

The fixes, in the order they matter:

1. **Mipmaps + bilinear** (`Image::buildMips` / `Image::sampleBilinear` in
   `base.h`). A 1024x1024 atlas point-sampled at a few hundred pixels aliases
   badly; a per-triangle LOD estimate (`log2(texels per pixel)`) picks the right
   mip and blends between two levels.
2. **1:1 rendering by default**, with `--render-width/height` still able to pin a
   size for benchmarking.
3. **Adaptive resolution** for the windowed game: if the frame rate drops under
   26 fps the internal scale steps down (to a floor of 50%); above 55 fps it
   steps back up to 1:1. Crisp when the machine can afford it, softer rather than
   stuttering when it cannot. Disabled when a render size is pinned or in
   `--headless`.
4. **ACES-ish tone curve** applied after fog, matching what three.js does with
   `ACESFilmicToneMapping`.
5. **HUD scaling bug**: `textWidth()` was called without the HUD scale factor, so
   right-aligned and centred text overflowed the screen at anything above
   640x360. Those call sites now pass it.

### Still different from the WebGL build (by design)

* **No soft shadows.** three.js uses `PCFSoftShadowMap`; the software renderer has
  no shadow map. Ground objects are lit by the directional wrap term only.
* **No anti-aliasing.** Geometry edges are aliased; the WebGL build runs MSAA.
  Supersampling with `--render-width 2560 --render-height 1440` works but is slow.
* The HUD is the engine's own 5x7 bitmap font, not the browser's webfont.

### Build-system bug found while doing this

The Makefile had no header dependencies, so editing `base.h` left `build/*.o`
compiled against the old struct layout. The link succeeded and then corrupted the
heap at exit (`SIGSEGV` in `Assets::~Assets`). `-MMD -MP` + `-include *.d` added;
`make clean` after editing a header if in doubt.

## Windows warnings: SmartScreen, UAC and Defender

An unsigned binary always trips Windows' heuristics. Two of the three warnings
were ours to fix; the third needs a code-signing certificate.

**Fixed: the UAC "unknown publisher wants to make changes" prompt.** The
application manifest had no `requestedExecutionLevel`, and when that element is
missing Windows' installer-detection heuristic may assume the exe wants
elevation. `win/app.manifest` now declares `asInvoker`, plus the supported-OS
list and per-monitor v2 DPI awareness. This build never needs admin rights.

**Fixed: the antivirus-shaped binary.** The exe was linking a statically
extracted `libwinpthread` (GCC's posix thread model). A large static pthread
inside a mingw binary is a well-known Defender false-positive signature. The
Windows build now uses the **win32 thread model**, where `std::thread` maps
straight onto Win32 threads, so that blob is gone. Imports are now only
`KERNEL32`, `USER32`, `GDI32`, `XINPUT1_3`, `msvcrt`.

**Not fixable here: SmartScreen.** "Windows protected your PC" appears for any
executable without a *reputation* in Microsoft's cloud, which comes from either
an Authenticode signature with an EV certificate or enough download volume.
Neither is something I can produce. What the user does about it:

* *More info -> Run anyway* on the SmartScreen dialog, or
* right-click the file -> *Properties* -> tick **Unblock** -> *Apply* (PowerShell:
  `Unblock-File .\Hollowmere.exe`), or
* Defender quarantine: *Protection history* -> *Actions* -> **Allow on device**,
  then exclude the folder.
* **Launching from a command prompt skips SmartScreen entirely** - it only
  filters Explorer/double-click launches.

These instructions are in the `README.txt` shipped inside each zip.

### What it would take to make the warning disappear

1. An Authenticode code-signing certificate. An OV certificate (~USD 200-400/yr)
   still shows the warning until reputation accumulates; an **EV** certificate
   (~USD 300-600/yr, requires a hardware token and identity verification) clears
   it within days.
2. Sign both the exe and the zip:

```bat
signtool sign /fd sha256 /tr http://timestamp.digicert.com /td sha256 ^
  /f mycert.pfx /p PASSWORD Hollowmere.exe
```

3. Submit the signed binary to Microsoft's *Submit a file for malware analysis*
   page once, to have it scanned rather than silently blocked.

## The Platformer pack was removed

`public/assets/platform/` (741 files) was deleted from the Spire. It had been
used for the spiral stair, the terraces, the bell column, the gate arch and the
landmark markers, so `worlds.cpp` now points at packs that were already loaded
(`dungeon/floor_tile_small`, `dungeon/floor_tile_large`, `dungeon/column`,
`props/arch_gate`, `props/arch`). `platform/` is gone from the model search path
in `game.cpp` as well. Prop and collider counts are unchanged (412 / 140).

Removing the pack dropped the Spire exe from 26.1 MB to 20.6 MB and its
standalone HTML from 28.0 MB to 27.3 MB.
