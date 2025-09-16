# Repository Guidelines

## Project Structure & Module Organization
- Core sources in `src/`: `main.cpp`, `display_hardware_test.*` (render loop + input), `shader.*` (GLSL wrapper), `text_renderer.*` (FreeType overlay).
- Public headers in `src/include/`.
- Assets: `assets/fonts/` bundled fonts used by the overlay.
- Tooling: `CMakeLists.txt`, `cmake/windows-cross.cmake`, `build_release.sh`.
- Build outputs: `build-linux/`, `build-windows/` (local builds), packaged artifacts in `dist/`.

## Build, Test, and Development Commands
- Linux (Debug): `cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug && cmake --build build-linux -j`
- Run (Linux): `build-linux/display_hardware_test`
- Release packages: `./build_release.sh` (creates `dist/linux-x64/` and optionally `dist/windows-x64/`).
- Windows cross-build (on Linux): `cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/windows-cross.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-windows -j`

## Coding Style & Naming Conventions
- Language: C++20. Indentation: 4 spaces. Braces on same line. Warnings enabled (`-Wall -Wextra -pedantic`).
- Filenames: `snake_case.*` (e.g., `display_hardware_test.cpp`). Types: `PascalCase` (e.g., `MonitorTest`). Functions/vars: `camelCase`. Macros/constants: `UPPER_CASE`.
- Prefer RAII and smart pointers (`std::unique_ptr`), `const`-correctness, no raw `new/delete`.
- Run formatting before PR: `clang-format -i src/*.cpp src/include/*.h`.

## Testing Guidelines
- No unit tests exist yet. If adding tests:
  - Place in `tests/`, name files `*_test.cpp`.
  - Use GoogleTest integrated with CTest; in CMake: `enable_testing()` and `add_test(...)`.
  - Run: `ctest --test-dir build-linux -j` after building.

## Commit & Pull Request Guidelines
- Use clear messages; Conventional Commits are recommended: `feat: ...`, `fix: ...`, `build: ...`.
- PRs should include: what/why, linked issues, platforms tested (e.g., Linux/Windows), and screenshots or terminal logs when visual output changes.
- Keep diffs focused and small; update `README.md` when behavior or usage changes.

## Security & Configuration Tips
- Avoid committing new large binaries (>10 MB) unless essential; fonts belong under `assets/fonts/`.
- Linux build deps (example Ubuntu): `sudo apt install libglfw3-dev libglew-dev libfreetype6-dev libfontconfig1-dev`.
- For Windows cross-build details and required DLLs, see `build_release.sh`; packaged outputs appear in `dist/windows-x64/`.
