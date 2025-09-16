#!/bin/bash
# 显示器压力测试工具 - 发布脚本（构建与打包）
# 功能:
# - 构建 Linux 可执行文件
# - 在 Linux 下交叉构建 Windows x64 可执行文件（使用已准备好的 deps/windows 依赖）
# - 打包 dist 目录（Linux: tar.gz，Windows: zip）
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
ROOT_DIR="$SCRIPT_DIR"

# 在指定目录中查找库文件（按候选名顺序）
# 参数:
#   $1: 库目录
#   $2...: 候选文件名，空格分隔
# 返回: 输出第一个存在的文件路径；若均不存在，返回空
find_lib() {
  local libdir="$1"; shift
  for name in "$@"; do
    if [ -f "$libdir/$name" ]; then
      echo "$libdir/$name"
      return 0
    fi
  done
  echo ""
  return 1
}

echo "=== 显示器压力测试工具 - 发布构建(增强版) ==="
echo

# 0. 预备输出目录
mkdir -p "$ROOT_DIR/dist/linux-x64"
mkdir -p "$ROOT_DIR/dist/windows-x64"

echo "[字体] 不再自动下载/内置字体。程序将使用系统默认字体 (Fontconfig 或常见系统路径)。"

# 1. 基础依赖检查（Linux 构建）
echo "[检查] 基础构建依赖..."
command -v cmake >/dev/null 2>&1 || { echo "错误: 需要安装 cmake"; exit 1; }
command -v make  >/dev/null 2>&1 || { echo "错误: 需要安装 make"; exit 1; }
command -v g++   >/dev/null 2>&1 || { echo "错误: 需要安装 g++"; exit 1; }
command -v pkg-config >/dev/null 2>&1 || { echo "错误: 需要安装 pkg-config"; exit 1; }

# 2. 构建 Linux 版本
echo
echo "=== 构建 Linux 版本 (Release) ==="
LINUX_BUILD_DIR="$ROOT_DIR/build-linux"
rm -rf "$LINUX_BUILD_DIR"
mkdir -p "$LINUX_BUILD_DIR"
cd "$LINUX_BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

# 复制产物
cp -f "$LINUX_BUILD_DIR/display_hardware_test" "$ROOT_DIR/dist/linux-x64/" || true
cp -f "$ROOT_DIR/README.md" "$ROOT_DIR/dist/linux-x64/" 2>/dev/null || true
# 不再复制内置字体到发布包，依赖系统字体
# strip 可选
if command -v strip >/dev/null 2>&1; then
  strip "$ROOT_DIR/dist/linux-x64/display_hardware_test" || true
fi

echo "Linux 版本构建完成: dist/linux-x64/"
cd "$ROOT_DIR"

# 3. 构建 Windows 版本（交叉编译）
echo
echo "=== 构建 Windows 版本 (x86_64, Release) ==="

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  echo "提示: 未检测到 MinGW-w64 (x86_64-w64-mingw32-g++)，将跳过 Windows 构建"
  echo "Ubuntu/Debian 安装: sudo apt install mingw-w64"
else
  DEPS_ROOT="$ROOT_DIR/deps/windows"
  mkdir -p "$DEPS_ROOT"

  # 依赖不再自动下载。如需更新/拉取，请手动运行 scripts/fetch_windows_deps.sh
  echo "[Windows] 使用已准备好的 deps/windows 依赖 (glfw/glew/freetype 等)"

  # 生成/更新交叉编译 toolchain
  mkdir -p "$ROOT_DIR/cmake"
  cat > "$ROOT_DIR/cmake/windows-cross.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR X86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# 查找根
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# 尽量静态链接标准库（其余依赖使用 dll）
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
EOF

  WIN_BUILD_DIR="$ROOT_DIR/build-windows"
  rm -rf "$WIN_BUILD_DIR"
  mkdir -p "$WIN_BUILD_DIR"
  cd "$WIN_BUILD_DIR"

  # 探测 include/lib
  GLFW_INC="$DEPS_ROOT/glfw/include"
  GLEW_INC="$DEPS_ROOT/glew/include"
  FT_INC_DIR1="$DEPS_ROOT/freetype/include"
  FT_INC_DIR2="$DEPS_ROOT/freetype/include/freetype2"

  GLFW_LIB_DIR="$DEPS_ROOT/glfw/lib"
  GLEW_LIB_DIR="$DEPS_ROOT/glew/lib"
  FT_LIB_DIR="$DEPS_ROOT/freetype/lib"

  # 选择库文件
  GLFW_LIB="$(find_lib "$GLFW_LIB_DIR" "libglfw3.dll.a" "libglfw3.a" )"
  GLEW_LIB="$(find_lib "$GLEW_LIB_DIR" "libglew32.dll.a" "libglew32.a" )"
  FT_LIB="$(find_lib "$FT_LIB_DIR" "libfreetype.dll.a" "libfreetype.a" )"

  # 组织 FreeType include 目录（以 ; 连接给 CMake）
  FT_INCS=""
  if [ -d "$FT_INC_DIR1" ] && [ -d "$FT_INC_DIR2" ]; then
    FT_INCS="$FT_INC_DIR1;$FT_INC_DIR2"
  elif [ -d "$FT_INC_DIR2" ]; then
    FT_INCS="$FT_INC_DIR2"
  else
    FT_INCS="$FT_INC_DIR1"
  fi

  if [ -z "${GLFW_LIB:-}" ] || [ -z "${GLEW_LIB:-}" ] || [ -z "${FT_LIB:-}" ]; then
    echo "警告: Windows 依赖可能不完整。"
    echo "      手动提供目录：deps/windows/{glfw,glew,freetype}/(include|lib)，或稍后手动调整。"
  fi

  cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/windows-cross.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$ROOT_DIR/dist/windows-x64" \
    -DGLFW3_INCLUDE_DIR="$GLFW_INC" \
    -DGLFW3_LIBRARY="$GLFW_LIB" \
    -DGLEW_INCLUDE_DIR="$GLEW_INC" \
    -DGLEW_LIBRARY="$GLEW_LIB" \
    -DFREETYPE_INCLUDE_DIRS="$FT_INCS" \
    -DFREETYPE_LIBRARY="$FT_LIB"

  make -j"$(nproc)"

  # 复制产物
  if [ -f "$WIN_BUILD_DIR/display_hardware_test.exe" ]; then
    cp -f "$WIN_BUILD_DIR/display_hardware_test.exe" "$ROOT_DIR/dist/windows-x64/" || true
    cp -f "$ROOT_DIR/README.md" "$ROOT_DIR/dist/windows-x64/" 2>/dev/null || true
    # 不再复制内置字体，使用系统字体或放置到系统字体目录
    # 复制运行所需 DLL
    if [ -d "$DEPS_ROOT/bin" ]; then
      cp -n "$DEPS_ROOT/bin/"*.dll "$ROOT_DIR/dist/windows-x64/" 2>/dev/null || true
    fi
    # 复制 MinGW 运行时常见 DLL（若存在）
    for d in /usr/x86_64-w64-mingw32/bin /usr/lib/gcc/x86_64-w64-mingw32/*; do
      if [ -d "$d" ]; then
        cp -n "$d"/{libgcc_s_seh-1.dll,libstdc++-6.dll,libwinpthread-1.dll} "$ROOT_DIR/dist/windows-x64/" 2>/dev/null || true
      fi
    done
    echo "Windows 版本构建完成: dist/windows-x64/"
  else
    echo "提示: 未生成 display_hardware_test.exe（请检查上方 CMake/链接日志）"
  fi

  cd "$ROOT_DIR"
fi

# 4. 打包
echo
echo "=== 创建发布包 ==="
cd "$ROOT_DIR/dist"

VERSION="$(date +"%Y%m%d")"
{
  echo "Version: $VERSION"
  echo "Build Date: $(date)"
} > version.txt

# Linux 包
if [ -f "linux-x64/display_hardware_test" ]; then
  echo "打包 Linux 版本..."
  tar -czf "display_hardware_test_linux-x64_v${VERSION}.tar.gz" linux-x64/
  echo "Linux 发布包: display_hardware_test_linux-x64_v${VERSION}.tar.gz"
fi

# Windows 包
if [ -f "windows-x64/display_hardware_test.exe" ]; then
  echo "打包 Windows 版本..."
  if command -v zip >/dev/null 2>&1; then
    zip -r "display_hardware_test_windows-x64_v${VERSION}.zip" windows-x64/
  else
    echo "警告: 未安装 zip，跳过 Windows 压缩包创建"
  fi
  echo "Windows 发布包: display_hardware_test_windows-x64_v${VERSION}.zip"
fi

cd "$ROOT_DIR"

echo
echo "=== 构建完成 ==="
echo "输出目录: dist/"
ls -la dist/ || true
echo
echo "使用说明:"
echo "- Linux版本: ./dist/linux-x64/display_hardware_test"
echo "- Windows版本: dist/windows-x64/display_hardware_test.exe"
echo
echo "备注:"
echo "1) Windows 交叉构建若失败，可手动将预编译依赖放入 deps/windows/{glfw,glew,freetype}/(include|lib)"
echo "   库名示例: libglfw3.a / libglfw3.dll.a, libglew32.dll.a, libfreetype.dll.a"
echo "2) 运行所需 DLL 会自动复制到 dist/windows-x64/（若已从 MSYS2 包获取）"
echo "3) 若需更换字体，请安装到系统字体目录或调整源码选择逻辑（当前优先系统字体）"
