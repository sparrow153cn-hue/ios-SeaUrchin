# Seaurchin for iOS (port in progress)

An iOS port of [Seaurchin](README.md): the game logic is unchanged, and the Windows-only layers
(DxLib/DirectX, Wacom touch, Win32) are replaced by a DxLib-compatible layer over Metal, UIKit touch/keyboard input
and BASS for iOS.

* **Read [docs/PORTING.md](docs/PORTING.md) first** — it says exactly what has been verified (208 automated checks, run on Linux)
  and what has not (everything that needs Xcode: it has never been compiled for iOS or run on a device).
* `Platform/DxCompat` — DxLib replacement (`DxCore.cpp` is backend-independent and tested)
* `Platform/iOS` — Metal renderer, UIKit host, `Info.plist`
* `Seaurchin/GameHost.*` — replaces `Main.cpp`'s init/run/shutdown
* `docs/game-source-changes.patch` — every change to the original game sources
* `docs/lane-reference-render.png` — the lane drawn by the real draw calls + real skin images through the test renderer
* `Tests/` — `make test`

Quick start (on a Mac): `brew install xcodegen && scripts/fetch_deps.sh`, add BASS for iOS to `ThirdParty/bass/`,
`xcodegen generate`, open the project. Details and the first-build checklist are in `docs/PORTING.md`.

**No Mac?** Push this repo to GitHub: `.github/workflows/ios-build.yml` builds it on a macOS runner (see *Building on GitHub* in `docs/PORTING.md`).
