# display_hardware_test

Diagnose display pipelines (GPU/cable/protocol/monitor) under high refresh rate and high color depth. Useful for reproducing black screens, artifacts, frame drops, and MTF degradation.

For a Chinese version of this document, see `README_zh.md`.

## Features
- Realtime overlay with FPS, frame time, target FPS.
- System info: GPU vendor/model, OpenGL version, current resolution and refresh rate.
- Stress patterns for bandwidth and timing; static patterns for geometry/color checks.

## Modes
- Fixed FPS: constant target (e.g., 120 FPS).
- Jitter FPS: random jumps between min/max (e.g., 30–144 FPS).
- Oscillation FPS: smooth sine-wave variation.

## Pattern Groups
- Static: color bars, gray gradient, 16-step gray, fine/coarse checkerboards, 32px/8px grids, RGB stripes, cross/thirds, black/white/R/G/B/50% gray, Siemens star, horizontal/vertical wedges, concentric rings, dot grid, gamma checker.
- Dynamic: multiple high-entropy content variants, RGBW cycle, moving highlight bar, UFO motion, 1px temporal inversion, zone plate, bit-plane flicker (10-bit), color checker cycle, blue-noise scroll, radial phase sweep, wedge spin.

## Tech Highlights
- 10-bit depth friendly patterns (0–1023 per channel), designed to avoid compressibility and maximize link utilization.
- Helps provoke handshake renegotiations and exposes DSC/VRR/G-Sync edge cases.

## Build
- Linux (Debug): `cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug && cmake --build build-linux -j`
- Linux (Release): `cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release && cmake --build build-linux -j`
- Windows cross-build (on Linux): `cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/windows-cross.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-windows -j`
- Release packages: `./build_release.sh` → `dist/linux-x64/`, `dist/windows-x64/`
- Note: Windows deps are not auto-downloaded. If needed, run `scripts/fetch_windows_deps.sh` to populate `deps/windows/` (GLFW/GLEW/FreeType and common DLLs).

## Run
- Linux: `build-linux/display_hardware_test`
- Windows: `build-windows/display_hardware_test.exe`

## Controls
- `ESC`: exit
- `P`: pause/resume
- `SPACE`: switch group (static/dynamic)
- `LEFT/RIGHT`: previous/next pattern
- `V`: VSync toggle
- `A`: autoplay toggle; `[`/`]`: interval -/+ 1s
- `K`: save screenshot (PPM to `dist/screenshots/`)
- `T`: logging toggle (CSV to `dist/logs/`)

## Requirements
- OS: Linux or Windows (cross-built on Linux via MinGW)
- GPU: OpenGL 3.3+
- Monitor: high refresh (e.g., 4K@120Hz) recommended
- Linux deps: `libglfw3-dev libglew-dev libfreetype6-dev libfontconfig1-dev`

## Troubleshooting
- Build: ensure CMake 3.16+, GLFW, GLEW, FreeType are installed; Fontconfig optional.
- Run: verify driver is up to date; check cable/port bandwidth; prefer certified HDMI 2.1 or DP 1.4+ cables.

## Notes
- The overlay uses system fonts by default (Fontconfig on Linux, common paths on Windows). Install CJK fonts for best Chinese text rendering.
