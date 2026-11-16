# Seaurchin → iOS: porting notes

This document explains how the port is structured, what has actually been verified, and what you should
expect to fix on your first build on a Mac. Read **Verification status** first.

## Verification status

The port was prepared in a Linux sandbox with no Xcode, no Mac and no network. So:

| Part | State |
|---|---|
| DxLib replacement front-end (`Platform/DxCompat/DxCore.cpp`): handle tables, blend/brightness/camera state, 2D+3D geometry, batching, render targets | **Compiled and tested** (80 checks against a CPU reference renderer) |
| Camera / projection (left-handed, 60° vertical FOV) | **Tested**, and the FOV is independently confirmed (see *Calibration*) |
| End-to-end lane scene using the real default-skin images | **Rendered and inspected** (`docs/lane-reference-render.png`) |
| `SeaText` (UTF-8 ↔ `wstring`, both 16- and 32-bit `wchar_t`), `SeaPng`, `SeaTouchRouter`, `SeaKeyMap` | **Compiled and tested** |
| `Controller.cpp`, `Debug.cpp`, `Font.h` (real game sources) | **Compiled and tested** (touch→slider mapping, edge clamp, `.sif` layout vs. the shipped font files) |
| Metal backend, iOS host (`Platform/iOS/*`), `GameHost.cpp`, `project.yml`, `scripts/fetch_deps.sh` | **Written, never compiled** |
| The remaining game-source patches (`docs/game-source-changes.patch`) | **Written, never compiled** — the rest of the engine needs Boost/AngelScript/BASS headers that were not available |
| Running on a device | **Not done** |

In total 208 checks pass (`cd Tests && make test`). Expect a round of ordinary compile errors on the first
Xcode build — the original code was only ever built with MSVC. Section *First-build checklist* lists the
classes of problem I anticipated.

## Approach

The game logic (~10.7k lines: chart parsing, scoring, scripting glue, sprites, effects) is portable.
What is Windows-bound is a thin layer, so the port replaces that layer rather than rewriting the game:

```
 skins / scripts (AngelScript)                          unchanged
 ExecutionManager, ScenePlayer, ScoreProcessor ...      unchanged (+ ~390 lines of patches)
 ─────────────────────────────────────────────────────────────────────────────────────────
 DxLib API subset    -> Platform/DxCompat/DxLib.h + DxCore.cpp   (same signatures, same defaults)
                        └─ DxRenderer interface ─┬─ SeaMetalRenderer.mm  (iOS, Metal)
                                                 └─ Tests/DxSoftRenderer (CPU reference, tests only)
 BASS audio          -> BASS for iOS (same API; UTF-8 paths instead of BASS_UNICODE)
 Wacom touch strip   -> ControlState::TouchBegan/Moved/Ended  <- SeaTouchRouter <- UIKit touches
 Win32 (window, console, IME, fonts, paths) -> GameHost.cpp + UIKit host
```

Why a DxLib *compatibility layer* rather than an engine rewrite: Seaurchin uses only ~30 DxLib
entry points. Re-implementing exactly those keeps every draw call in `ScenePlayer.Draw.cpp`,
`ScriptSprite.cpp`, etc. byte-for-byte as the author wrote and tuned them.

### The renderer contract (`DxRenderer.h`)

`DxCore` turns every DxLib call into triangles plus a small `DxDrawState` and batches compatible
draws. A backend only has to draw indexed triangles. The fragment/blend rules are written down in
`DxRenderer.h` and implemented twice — in `SeaShaders.metal` and in `Tests/DxSoftRenderer.cpp` —
so the tested reference defines the behaviour:

* render targets hold **premultiplied** alpha, loaded images **straight** alpha; every blend mode is expressed in premultiplied terms. A test proves that compositing through an alpha render target equals drawing directly (the case that breaks most naive ports; text sprites depend on it);
* DxLib specifics that are easy to get wrong and are covered by tests: `DrawBox` right/bottom edge exclusive; `DrawRotaGraph3F` angle clockwise on screen; **`DrawRectRotaGraph3F` pivot is relative to the source rectangle** (`DrawTap` tiles 64-px cells of a sprite sheet with `cx=0`); `TurnFlag` mirrors; `LoadDivGraph` cells share one texture; derived-graph UVs in 3D draws; near-plane clipping; top-left fill rule (no double blending on quad diagonals).

## Controls on iOS

The game expects a 16-slider touch strip (gameplay) and a keyboard (menus). `SeaTouchRouter` splits the screen:

* **lower 55 % of the 16:9 play field** — finger x → slider 0‥15 (multi-touch, sliding works);
* **upper 45 %** — gestures become key presses: *tap* = Return, *two-finger tap* = Escape, *drag/swipe* = arrow keys (one press per 56 px of travel), *long press (0.7 s, finger still)* = toggle autoplay;
* a hardware keyboard works as on Windows (A Z S X D C F V G B H N J M K , are the 16 sliders; arrows/Return/Escape in menus).

The play field is a letter-boxed 1280×720 virtual screen, so touch coordinates map 1:1 to the original layout.
**Autoplay.** `ScenePlayer` plays automatically unless `AutoPlay == 0`, and the default skin only sets that with the **M** key on
the song-select screen (**A** turns it back on, with an on-screen "オートプレイ: ON/OFF" message). A touch-only device would therefore
be stuck in autoplay, hence the long-press gesture (the host alternates M/A, starting from the engine default "on"). It is
also a handy way to demo the game before touch play is tuned.

## Build

1. `brew install xcodegen`
2. `scripts/fetch_deps.sh` — downloads Boost, AngelScript, FreeType, libpng, msgpack into `ThirdParty/` (versions are variables at the top; the script was written offline and has not been run).
3. Put **BASS for iOS** (`bass.h`, `bassmix.h`, `libbass.a`, `libbassmix.a`) in `ThirdParty/bass/` (+ `ios/`). Not downloaded automatically: it is proprietary — free for non-commercial use, licensed for commercial/App Store releases.
4. Edit the bundle id / team in `project.yml`, then `xcodegen generate && open Seaurchin.xcodeproj`.

Targets: `SeaurchinCore` (static library: all game code, DxCompat, AngelScript, Boost.Filesystem, FreeType, libpng; the PCH is the MSVC force-included `PrecompiledHeader.h`, now used as an Xcode prefix header) and `Seaurchin` (UIKit/Metal app).

## Building on GitHub (no Mac needed)

`.github/workflows/ios-build.yml` builds the app on a GitHub-hosted macOS runner and uploads an **unsigned IPA**.

1. Create a GitHub repository and push the **contents** of this folder (so `.github/` is at the repo root).
   A public repository gets free Actions minutes; on a private one macOS minutes are billed at a higher rate - check your plan.
   BASS is never committed: the workflow downloads it at build time (`scripts/fetch_bass.sh`).
2. *Optional*: if the default BASS download URLs (my unverified guess at un4seen's naming) fail, set the repository
   variables `BASS_IOS_URL` and `BASSMIX_IOS_URL` (Settings -> Secrets and variables -> Actions -> Variables) to the iOS zip
   links from https://www.un4seen.com/.
3. Push, or open the **Actions** tab -> *iOS build* -> *Run workflow*. Two jobs run:
   * **Portable tests (Linux)** - the 208 checks; this part has been run and should be green.
   * **iOS app (unsigned)** - Xcode 16.4 on `macos-15`; `macos-14` is deprecated and `macos-latest` now means `macos-26`
     (to try Xcode 26, change `runs-on` and `XCODE_VERSION` at the top of the workflow).
4. **Expect the iOS job to fail the first time** (this code has never been compiled for iOS). The job summary lists the first
   compiler errors; the full `build.log` is always attached as an artifact. Fix, push, repeat.
5. When it goes green, download the `Seaurchin-unsigned-ipa` artifact. It is not signed: install it by re-signing with a
   tool such as Sideloadly or AltStore (free Apple ID, apps expire after 7 days), or add signing to the workflow
   (certificate + provisioning profile as secrets; TestFlight/App Store need a paid Apple Developer account).

The IPA statically links BASS: do not publish it publicly without a BASS license. Free use is for non-commercial projects.

## Putting charts on the device

The app's Documents folder is exposed in the **Files** app (`UIFileSharingEnabled`). Layout, as the engine expects it:

```
Documents/
  Music/<Category>/<Song>/…      .sus chart(s), audio, jacket  (same layout as on Windows)
  Data/                          copied from the bundled Resources/ on first launch and after app updates
  config.json, Cache/            created by the engine
```

`Data/` is re-staged from the app bundle when the app version changes (always in Debug builds), overwriting bundled
files; extra skins you add under `Data/Skins/` are left alone. The music scan throws if `Music/` does not exist;
`GameHost` creates it. No charts or music are bundled (copyright).

## What changed in the game sources

26 files (two of them new: `GameHost.*`), ~390 lines added; see `docs/game-source-changes.patch` (made against a UTF-8-normalised copy of the
original so the diff is readable). All sources were re-encoded from Shift-JIS/UTF-16 to UTF-8 with BOM (MSVC and clang
both read that; the Japanese comments and strings are intact). The Windows build keeps working: every change is either
platform-neutral or behind `#if defined(_WIN32)` (add `Platform/SeaText.cpp` to the project — already done in the `.vcxproj`).

| Problem | Fix |
|---|---|
| `GlyphInfo` contained a `wchar_t` and is read straight from the binary `.sif` fonts with `sizeof` — 18 bytes on Windows, 20 on iOS: every font would have been misparsed | `uint16_t letter`, `static_assert`s; **validated against the three shipped `.sif` files** |
| `std::wstring` = UTF-16 on Windows, UTF-32 elsewhere; `MultiByteToWideChar` | portable UTF-8 ⇄ `wstring` (`SeaText.cpp`), glyph lookup guarded for code points > U+FFFF |
| `ifstream::open(std::wstring)` (MSVC extension) in 6 places | `SU_FS_PATH()` |
| DxLib was switched to UTF-16 file names via a `wchar_t*`→`const char*` cast | `SU_DX_PATH()`; the shim takes UTF-8 |
| `char *cp = "0000"; cp[i] = …` in `SusAnalyzer.cpp` — writes into a string literal (MSVC tolerated it; crashes on iOS) | `char cp[5]` |
| `double` in a braced `VERTEX3D` initialiser (hard error in clang) | `float role`; plus `-Wno-c++11-narrowing` as a safety net |
| Wacom touch SDK; slider index `floor(x/width*16)` can reach 16 (out-of-bounds write) | platform-neutral `ControlState::Touch*` API with clamping; Wacom code kept under `_WIN32` |
| BASS: `BASS_UNICODE` + `wchar_t` paths, HWND | UTF-8 paths, `NULL` window outside Windows |
| `Main.cpp`, IME, `WndProc`, console, `EnumFontFamiliesEx`, `PostMessage(WM_CLOSE)` | `GameHost.cpp`, guarded or stubbed (`ExitApplication()` is a no-op on iOS by design) |
| `SceneDebug` uses DxLib's `printfDx` | Windows only |

## Calibration findings

The skin relies on DxLib's *default* projection (the code only sets camera position/target). I could not find the
defaults in DxLib's documentation, so I derived them from the data: the default skin's `Background.png` contains a
perspective trapezoid drawn from the same camera. Inverting the projection for six screen rows gives a constant world
half-width (≈658–669, spread ≈10 units) **only at a 60° vertical FOV**; 50°/55°/65°/70° drift by 70–300. The far end of the
lane then lands at y≈89, hidden behind the 106-px `PlayerTopCover`, and the judge line at y≈682. Near/far clip
planes do not affect the lane; they are `10 / 20000` (`DxSetProjection`).

Another assumption worth knowing: draws use **bilinear** filtering (DxLib's own default is nearest-neighbour; the original calls no `SetDrawMode`). Bilinear is the right choice once the screen is scaled to a device. To compare with Windows, set `drawMode` to `DX_DRAWMODE_NEAREST` in `DxCore.cpp`.

## First-build checklist (most likely problems first)

1. **AngelScript version.** The engine registers its API through native calling conventions, so the library needs ARM64 native-call support (added in releases from roughly 2.33–2.35 — I could not verify the exact version offline; use a current release). The code dates from 2017–18; if a current AngelScript changed an API the engine uses (e.g. `asITypeInfo`/script-builder interfaces in `wscriptbuilder.*`), try an older release first.
2. **Clang-only errors that MSVC accepted**: missing `typename`/two-phase lookup in templates, missing includes that MSVC pulled in transitively (the PCH now lists many extra standard headers), `std::lerp` ambiguities if you raise the language standard above C++17 (the engine defines its own `lerp`).
3. **Boost**: Filesystem is the only compiled library (xpressive/spirit/regex/crc are header-only). `BOOST_FILESYSTEM_VERSION=3` matters; `GameHost` calls `path::imbue(utf8)` so the engine's `std::wstring` paths convert correctly. Pin Boost to a release that still ships v3 as default.
4. **msgpack**: the music cache uses `MSGPACK_DEFINE`; newer *msgpack-cxx* releases may need `MSGPACK_NO_BOOST` or a header-path tweak.
5. **Metal backend**: never compiled. It is ~250 lines mapping the documented contract; if the picture is wrong, compare against `DxSoftRenderer` — it is the reference. Use Xcode's Metal API validation and frame capture.
6. **Audio session.** `BASS_Init` runs inside `ExecutionManager`'s constructor and sets its own session category; the host re-asserts Playback and a 5 ms preferred buffer afterwards. Latency/offset tuning on real devices is expected (`Sound` settings, BASS buffer config).

## Known limitations / follow-ups

* No MSAA, and the `…AA` shape calls are not anti-aliased (`SetFullSceneAntiAliasingMode` was ignored). Easy next step: 4× MSAA on the main target.
* Per-draw buffer allocation in the Metal backend (fine for the tens of batched draws per frame; replace with a ring buffer if profiling says so).
* Frame rate fixed at 60 Hz (`preferredFramesPerSecond`); the engine is time-based, so ProMotion can be enabled.
* The 1024×8192 ground render target is ≈32 MB (it is 1024 × next-power-of-two of 4224 in the original too).
* System-font enumeration and the DxLib IME text-input widget are stubbed (the skin API never uses them).
* Touch zones are fixed; a scene-aware layout (e.g. full-screen sliders only while playing) would need a hook from the script layer.
* The 3D `DrawTriangle3D` outline mode and `DX_BLENDMODE_SUB/MUL` are not implemented (not used by the engine).

## Tests

```
cd Tests && make test     # DxCompatTests (80 checks) + PortableTests (128 checks); needs g++, libpng
make lane                 # replays ScenePlayer::Draw with the real skin images -> /tmp/lane.ppm
```
`Tests/DxSoftRenderer.*` is a slow CPU rasterizer used only for tests; it is not part of the app.
