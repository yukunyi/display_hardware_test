#!/bin/bash
# Fetch prebuilt Windows (x86_64) dependencies from MSYS2 mirrors into deps/windows/
# Components: glfw, glew, freetype, plus common runtime deps for FreeType/harfbuzz.
#
# Usage:
#   ./scripts/fetch_windows_deps.sh
#
# Result layout:
#   deps/windows/
#     glfw/{include,lib}
#     glew/{include,lib}
#     freetype/{include,lib}
#     ... other components ...
#     bin/*.dll  (runtime DLLs)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
ROOT_DIR="${SCRIPT_DIR%/scripts}"
DEPS_ROOT="$ROOT_DIR/deps/windows"
mkdir -p "$DEPS_ROOT"

fetch_file() {
  local url="$1"
  local out="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -L --retry 3 --connect-timeout 15 -o "$out" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$out" "$url"
  else
    echo "ERROR: Need curl or wget to download: $url" >&2
    return 1
  fi
}

fetch_html() {
  local url="$1"
  if command -v curl >/dev/null 2>&1; then
    curl -L --retry 3 --connect-timeout 15 -s "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O - "$url"
  else
    return 1
  fi
}

msys2_find_latest_pkg() {
  local base="$1"
  local idx_url_primary="https://repo.msys2.org/mingw/x86_64/"
  local idx_url_backup="https://mirror.msys2.org/mingw/x86_64/"
  local html
  html="$(fetch_html "$idx_url_primary" || true)"
  if [ -z "$html" ]; then
    html="$(fetch_html "$idx_url_backup" || true)"
  fi
  if [ -z "$html" ]; then
    echo ""; return 1
  fi
  echo "$html" | grep -oE "${base}-[0-9][^\"<>]*\.pkg\.tar\.zst" | sort -V | tail -n 1
}

extract_pkg() {
  local pkg_file="$1"
  local dst_dir="$2"
  mkdir -p "$dst_dir"
  if tar --help 2>/dev/null | grep -qi zstd; then
    tar --zstd -xvf "$pkg_file" -C "$dst_dir"
  else
    if command -v unzstd >/dev/null 2>&1; then
      unzstd -c "$pkg_file" | tar -xvf - -C "$dst_dir"
    else
      echo "ERROR: Need tar --zstd or unzstd to extract $pkg_file" >&2
      return 1
    fi
  fi
}

msys2_install_pkg() {
  local base="$1"
  local comp="$2"
  local idx_url_primary="https://repo.msys2.org/mingw/x86_64/"
  local idx_url_backup="https://mirror.msys2.org/mingw/x86_64/"

  local latest
  latest="$(msys2_find_latest_pkg "$base" || true)"
  if [ -z "$latest" ]; then
    echo "ERROR: Cannot find $base in MSYS2 repo" >&2
    return 1
  fi

  local url="$idx_url_primary$latest"
  local tmp_pkg="$ROOT_DIR/$latest"
  echo "[MSYS2] Download $base: $latest"
  if ! fetch_file "$url" "$tmp_pkg"; then
    echo "[MSYS2] Primary failed, try mirror..."
    url="$idx_url_backup$latest"
    fetch_file "$url" "$tmp_pkg"
  fi

  local tmp_dir="$ROOT_DIR/.msys2_extract_${comp}"
  rm -rf "$tmp_dir"
  mkdir -p "$tmp_dir"
  extract_pkg "$tmp_pkg" "$tmp_dir"

  mkdir -p "$DEPS_ROOT/$comp/include" "$DEPS_ROOT/$comp/lib" "$DEPS_ROOT/bin"
  if [ -d "$tmp_dir/mingw64/include" ]; then
    cp -R "$tmp_dir/mingw64/include/." "$DEPS_ROOT/$comp/include/" || true
  fi
  if [ -d "$tmp_dir/mingw64/lib" ]; then
    cp -R "$tmp_dir/mingw64/lib/." "$DEPS_ROOT/$comp/lib/" || true
  fi
  if [ -d "$tmp_dir/mingw64/bin" ]; then
    cp -R "$tmp_dir/mingw64/bin/"*.dll "$DEPS_ROOT/bin/" 2>/dev/null || true
  fi

  rm -rf "$tmp_dir" "$tmp_pkg"
}

echo "=== Fetch Windows deps into deps/windows ==="

msys2_install_pkg "mingw-w64-x86_64-glfw"     "glfw" || true
msys2_install_pkg "mingw-w64-x86_64-glew"     "glew" || true
msys2_install_pkg "mingw-w64-x86_64-freetype" "freetype" || true

# FreeType deps
msys2_install_pkg "mingw-w64-x86_64-libpng"    "libpng"    || true
msys2_install_pkg "mingw-w64-x86_64-zlib"      "zlib"      || true
msys2_install_pkg "mingw-w64-x86_64-bzip2"     "bzip2"     || true
msys2_install_pkg "mingw-w64-x86_64-brotli"    "brotli"    || true
msys2_install_pkg "mingw-w64-x86_64-harfbuzz"  "harfbuzz"  || true
msys2_install_pkg "mingw-w64-x86_64-graphite2" "graphite2" || true

# runtime deps
msys2_install_pkg "mingw-w64-x86_64-glib2"     "glib2"     || true
msys2_install_pkg "mingw-w64-x86_64-pcre2"     "pcre2"     || true
msys2_install_pkg "mingw-w64-x86_64-libiconv"  "libiconv"  || true
msys2_install_pkg "mingw-w64-x86_64-gettext"   "gettext"   || true
msys2_install_pkg "mingw-w64-x86_64-fribidi"   "fribidi"   || true

echo "=== Done ==="

