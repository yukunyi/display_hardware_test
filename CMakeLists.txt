cmake_minimum_required(VERSION 3.16)
project(display_hardware_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包
find_package(PkgConfig REQUIRED)
# OpenGL：Linux 用系统 OpenGL；Windows 交叉编译用 opengl32
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(OPENGL_LIBRARIES opengl32)
else()
    find_package(OpenGL REQUIRED)
endif()
# Freetype：Windows 交叉编译不用 find_package，改用手动传入/脚本准备的路径
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(Freetype QUIET)
endif()
# 仅在非 Windows 平台使用 Fontconfig（避免交叉编译混入本机头文件）
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(Fontconfig QUIET)
    if(NOT Fontconfig_FOUND)
        pkg_check_modules(FONTCONFIG QUIET fontconfig)
    endif()
endif()

# 查找GLFW（非 Windows 使用系统包）
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(glfw3 QUIET)
    if(NOT glfw3_FOUND)
        # 如果没有找到系统安装的GLFW，尝试使用pkg-config
        pkg_check_modules(GLFW REQUIRED glfw3)
    endif()
endif()

# 手动覆盖GLFW（交叉编译或自定义路径）
# 支持传入：GLFW3_LIBRARY/GLFW3_LIBRARIES 和 GLFW3_INCLUDE_DIR/GLFW3_INCLUDE_DIRS
if(DEFINED GLFW3_LIBRARY OR DEFINED GLFW3_LIBRARIES)
    if(NOT DEFINED GLFW3_LIBRARIES AND DEFINED GLFW3_LIBRARY)
        set(GLFW_LIBRARIES ${GLFW3_LIBRARY})
    else()
        set(GLFW_LIBRARIES ${GLFW3_LIBRARIES})
    endif()
    if(DEFINED GLFW3_INCLUDE_DIRS)
        set(GLFW_INCLUDE_DIRS ${GLFW3_INCLUDE_DIRS})
    elseif(DEFINED GLFW3_INCLUDE_DIR)
        set(GLFW_INCLUDE_DIRS ${GLFW3_INCLUDE_DIR})
    endif()
    # 强制进入非glfw3_FOUND分支
    set(glfw3_FOUND FALSE)
endif()

# 查找GLEW（非 Windows 使用系统包）
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(GLEW QUIET)
    if(NOT GLEW_FOUND)
        pkg_check_modules(GLEW REQUIRED glew)
    endif()
endif()

# 手动覆盖GLEW（交叉编译或自定义路径）
# 支持传入：GLEW_LIBRARY/GLEW_LIBRARIES 和 GLEW_INCLUDE_DIR/GLEW_INCLUDE_DIRS
if(DEFINED GLEW_LIBRARY OR DEFINED GLEW_LIBRARIES)
    if(NOT DEFINED GLEW_LIBRARIES AND DEFINED GLEW_LIBRARY)
        set(GLEW_LIBRARIES ${GLEW_LIBRARY})
    endif()
    if(DEFINED GLEW_INCLUDE_DIRS)
        set(GLEW_INCLUDE_DIRS ${GLEW_INCLUDE_DIRS})
    elseif(DEFINED GLEW_INCLUDE_DIR)
        set(GLEW_INCLUDE_DIRS ${GLEW_INCLUDE_DIR})
    endif()
    # 强制进入非GLEW_FOUND分支
    set(GLEW_FOUND FALSE)
endif()

# 添加源文件（迁移至 src/）
set(SOURCES
    src/main.cpp
    src/shader.cpp
    src/monitor_test.cpp
    src/text_renderer.cpp
)

set(HEADERS
    src/include/shader.h
    src/include/monitor_test.h
    src/include/text_renderer.h
)

add_executable(display_hardware_test ${SOURCES} ${HEADERS})
# 将项目源目录作为字符串常量注入，运行时用于定位 assets/fonts
target_compile_definitions(display_hardware_test PRIVATE PROJ_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(display_hardware_test PRIVATE ${CMAKE_SOURCE_DIR}/src/include)

# Windows 交叉编译: 为第三方依赖添加库目录（由脚本放入 deps/windows/<name>/lib）
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WIN_DEPS_ROOT "${CMAKE_SOURCE_DIR}/deps/windows" CACHE PATH "Windows deps root")
    set(_win_components glfw glew freetype libpng zlib bzip2 brotli harfbuzz graphite2)
    set(WIN_EXTRA_LIB_DIRS "")
    foreach(_c ${_win_components})
        if(EXISTS "${WIN_DEPS_ROOT}/${_c}/lib")
            list(APPEND WIN_EXTRA_LIB_DIRS "${WIN_DEPS_ROOT}/${_c}/lib")
        endif()
    endforeach()
    if(WIN_EXTRA_LIB_DIRS)
        message(STATUS "Windows extra lib dirs: ${WIN_EXTRA_LIB_DIRS}")
        target_link_directories(display_hardware_test PRIVATE ${WIN_EXTRA_LIB_DIRS})
    endif()
endif()

# 链接库
target_link_libraries(display_hardware_test
    ${OPENGL_LIBRARIES}
)

# 根据平台链接不同的库
if(glfw3_FOUND)
    target_link_libraries(display_hardware_test glfw)
else()
    target_link_libraries(display_hardware_test ${GLFW_LIBRARIES})
    target_include_directories(display_hardware_test PRIVATE ${GLFW_INCLUDE_DIRS})
    if(DEFINED GLFW_CFLAGS_OTHER AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_options(display_hardware_test PRIVATE ${GLFW_CFLAGS_OTHER})
    endif()
endif()

if(GLEW_FOUND)
    target_link_libraries(display_hardware_test GLEW::GLEW)
else()
    target_link_libraries(display_hardware_test ${GLEW_LIBRARIES})
    target_include_directories(display_hardware_test PRIVATE ${GLEW_INCLUDE_DIRS})
    if(DEFINED GLEW_CFLAGS_OTHER AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_options(display_hardware_test PRIVATE ${GLEW_CFLAGS_OTHER})
    endif()
endif()

# Freetype 链接
if(Freetype_FOUND)
    target_link_libraries(display_hardware_test Freetype::Freetype)
else()
    # 优先使用显式传入
    if(DEFINED FREETYPE_LIBRARY AND DEFINED FREETYPE_INCLUDE_DIRS)
        target_link_libraries(display_hardware_test ${FREETYPE_LIBRARY})
        target_include_directories(display_hardware_test PRIVATE ${FREETYPE_INCLUDE_DIRS})
    else()
        # 使用 pkg-config 查找 freetype2（常见开发包名）
        pkg_check_modules(FREETYPE QUIET freetype2)
        if(FREETYPE_FOUND)
            target_link_libraries(display_hardware_test ${FREETYPE_LIBRARIES})
            target_include_directories(display_hardware_test PRIVATE ${FREETYPE_INCLUDE_DIRS})
            target_compile_options(display_hardware_test PRIVATE ${FREETYPE_CFLAGS_OTHER})
        else()
            message(FATAL_ERROR "Freetype not found. Install FreeType dev package (pkg: freetype2) or provide FREETYPE_LIBRARY and FREETYPE_INCLUDE_DIRS")
        endif()
    endif()
endif()

# Fontconfig 链接（仅非 Windows）
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(Fontconfig_FOUND)
        target_link_libraries(display_hardware_test Fontconfig::Fontconfig)
        target_compile_definitions(display_hardware_test PRIVATE HAS_FONTCONFIG=1)
    elseif(DEFINED FONTCONFIG_LIBRARIES AND DEFINED FONTCONFIG_INCLUDE_DIRS)
        target_link_libraries(display_hardware_test ${FONTCONFIG_LIBRARIES})
        target_include_directories(display_hardware_test PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
        target_compile_definitions(display_hardware_test PRIVATE HAS_FONTCONFIG=1)
    endif()
endif()

# Windows 交叉编译: 额外链接 MSYS2 的依赖库（FreeType 的可选依赖）与 Win32 系统库
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # PNG/zlib/bzip2/Brotli/HarfBuzz 对应 MSYS2 的 import libs 名称
    target_link_libraries(display_hardware_test png16 z bz2 brotlidec brotlicommon harfbuzz)
    # Win32 必需系统库（GLFW/GLEW/OpenGL 使用）
    target_link_libraries(display_hardware_test user32 gdi32 shell32 advapi32 ole32 comdlg32 winmm)
endif()

# Windows特定设置
if(WIN32)
    target_link_libraries(display_hardware_test gdi32)
endif()

# 设置编译选项
if(MSVC)
    target_compile_options(display_hardware_test PRIVATE /W4)
else()
    target_compile_options(display_hardware_test PRIVATE -Wall -Wextra -pedantic)
endif()
