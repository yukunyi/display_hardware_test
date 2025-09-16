# display_hardware_test（中文）

用于在高刷新率与高色深环境下测试显示器与显示链路（GPU/线缆/协议）的稳定性与画质边界，可用于复现黑屏、花屏、掉帧、MTF 降级等问题。

English version: see `README.md`.

## 功能
- 实时叠加层：FPS、帧时间（ms）、目标 FPS。
- 系统信息：GPU 厂商/型号、OpenGL 版本、分辨率与刷新率。
- 压力图样：带宽与时序压力；静态图样用于几何/色彩/均匀性检查。

## 模式
- 固定帧率（Fixed FPS）：恒定目标（如 120 FPS）。
- 帧率抖动（Jitter FPS）：在最小/最大范围内随机跳变（如 30–144 FPS）。
- 震荡帧率（Oscillation FPS）：按正弦波平滑变化。

## 图样分组
- 静态图样：彩条、灰阶渐变、16 阶灰条、细/粗棋盘格、32px/8px 网格、RGB 竖条、十字+三分线、纯黑/白/红/绿/蓝、50% 灰、Siemens Star、水平/垂直楔形、同心圆环、点栅格、Gamma Checker。
- 动态压力：多种高熵内容变体、RGBW 轮播、移动亮条、UFO 风格运动、1px 棋盘反相闪烁、Zone Plate、位平面闪烁（10-bit）、彩色棋盘轮换、蓝噪声滚动、径向相位扫频、旋转楔形线。

## 技术要点
- 面向 10-bit 色深（每通道 0–1023），避免可压缩内容，最大化带宽利用。
- 便于暴露 handshake 重协商、DSC/VRR/G-Sync 等边界问题。

## 构建
- Linux（Debug）：`cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug && cmake --build build-linux -j`
- Linux（Release）：`cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release && cmake --build build-linux -j`
- Windows 跨平台（在 Linux 上）：`cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/windows-cross.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-windows -j`
- 发布打包：运行 `./build_release.sh`，输出到 `dist/linux-x64/` 与/或 `dist/windows-x64/`。
- 说明：Windows 依赖不再自动下载。如需获取/更新，可运行 `scripts/fetch_windows_deps.sh` 将预编译库放入 `deps/windows/`。

## 运行
- Linux：`build-linux/display_hardware_test`
- Windows：`build-windows/display_hardware_test.exe`

## 控制按键
- `ESC`：退出
- `P`：暂停/继续
- `SPACE`：切换分组（静态/动态）
- `←/→`：上一/下一图样
- `V`：垂直同步开关
- `A`：自动轮播开关；`[`/`]`：轮播间隔 -/+ 1s
- `K`：截图保存（PPM，保存至 `dist/screenshots/`）
- `T`：日志开关（CSV，保存至 `dist/logs/`）

## 依赖
- 操作系统：Linux 或 Windows（Windows 版本可在 Linux 上交叉编译）
- 显卡：OpenGL 3.3+
- 建议显示器：高刷新率（如 4K@120Hz）
- Linux 构建依赖示例（Ubuntu）：`sudo apt install libglfw3-dev libglew-dev libfreetype6-dev libfontconfig1-dev`

## 故障排除
- 构建问题：确认 CMake（3.16+）、GLFW、GLEW、FreeType 已安装；Fontconfig 可选。
- 运行问题：更新显卡驱动；检查线缆/接口带宽；优先使用认证的 HDMI 2.1 或 DP 1.4+ 线缆。

## 其他说明
- 文本叠加默认优先使用系统字体（Linux 使用 Fontconfig）。若需中文显示效果更佳，建议安装 CJK 字体。
