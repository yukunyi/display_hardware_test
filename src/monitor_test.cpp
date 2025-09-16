#include "monitor_test.h"
#include "shader.h"
#include "text_renderer.h"

#include <iostream>
#include <sstream>
#include <cmath>
#include <random>
#include <thread>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <iomanip>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__unix__)
#include <unistd.h>
#include <limits.h>
#endif
#ifdef HAS_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

// 着色器源码定义（主背景/内容）
const std::string MonitorTest::vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;       // NDC 顶点坐标 [-1,1]
layout (location = 1) in vec2 aTexCoord;  // 纹理坐标 [0,1]

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const std::string MonitorTest::fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform float uTime;
uniform vec2 uResolution;
uniform int uColorVariation;
uniform int uContentMode;
uniform int uCategory; // 0: STATIC, 1: DYNAMIC

// 生成高复杂度、不可压缩的颜色内容
vec3 generateComplexColor(vec2 uv, float time, int variation) {
    // 特殊值-1用于半透明背景
    if (variation == -1) {
        return vec3(0.0, 0.0, 0.0); // 黑色半透明背景
    }
    
    // 基础颜色计算
    float r = uv.x;
    float g = uv.y;
    float b = sin(uv.x * 10.0 + uv.y * 10.0 + time) * 0.5 + 0.5;
    
    // 根据变化模式添加不同的扰动
    if (variation == 1) {
        // 高频噪声模式 - 最大化带宽使用
        r = fract(sin(uv.x * 123.456 + time) * 43758.5453);
        g = fract(sin(uv.y * 789.123 + time * 1.3) * 43758.5453);
        b = fract(sin((uv.x + uv.y) * 456.789 + time * 0.7) * 43758.5453);
    } else if (variation == 2) {
        // 波浪干涉模式
        float wave1 = sin(uv.x * 20.0 + time * 2.0);
        float wave2 = sin(uv.y * 15.0 + time * 1.5);
        float wave3 = sin((uv.x + uv.y) * 12.0 + time * 0.8);
        
        r = uv.x + wave1 * 0.3;
        g = uv.y + wave2 * 0.3;
        b = (wave3 + 1.0) * 0.5;
    } else if (variation == 3) {
        // 螺旋模式 - 创建复杂的梯度
        vec2 center = vec2(0.5, 0.5);
        vec2 pos = uv - center;
        float angle = atan(pos.y, pos.x);
        float radius = length(pos);
        
        r = fract(angle * 3.0 / 6.28318 + time * 0.5);
        g = fract(radius * 8.0 + time * 0.3);
        b = fract(sin(radius * 20.0 + angle * 5.0 + time) * 0.5 + 0.5);
    } else if (variation == 4) {
        // 棋盘格模式 - 高对比、易触发传输极限
        float s = 16.0 + 12.0 * sin(time * 0.7);
        vec2 gcell = floor(uv * s);
        float cb = mod(gcell.x + gcell.y, 2.0);
        r = cb;
        g = 1.0 - cb * 0.6;
        b = cb * 0.2 + 0.3;
    } else if (variation == 5) {
        // 彩虹辐射模式 - 角度/半径双通道
        vec2 c = uv - vec2(0.5);
        float ang = atan(c.y, c.x);
        float rad = length(c);
        r = 0.5 + 0.5 * sin(ang * 3.0 + time * 1.2);
        g = 0.5 + 0.5 * sin(ang * 5.0 - time * 0.9);
        b = 0.5 + 0.5 * sin(rad * 50.0 - time * 3.0);
        r = mix(r, sin(rad * 20.0 + time) * 0.5 + 0.5, 0.4);
    } else if (variation == 6) {
        // 斜向干涉条纹 - 方向性强（替代交错条纹，避免重复）
        float s1 = sin((uv.x + uv.y) * 40.0 + time * 2.5);
        float s2 = sin((uv.x - uv.y) * 60.0 - time * 1.7);
        r = s1 * 0.5 + 0.5;
        g = s2 * 0.5 + 0.5;
        b = sin((uv.x * 8.0 + uv.y * 6.0) + time) * 0.5 + 0.5;
    } else {
        // 默认模式(0) - 渐变+独立扰动
        r = uv.x + sin(time * 0.7) * 0.1;
        g = uv.y + sin(time * 1.1) * 0.1;
        b = sin(uv.x * 8.0 + uv.y * 6.0 + time) * 0.5 + 0.5;
    }
    
    // 添加高频细节，避免相邻像素颜色相同
    vec2 pixelPos = uv * uResolution;
    float noise = fract(sin(dot(pixelPos, vec2(12.9898, 78.233)) + time) * 43758.5453);
    
    // 轻微扰动，确保每个像素都不同
    r += noise * 0.01;
    g += noise * 0.007;
    b += noise * 0.013;
    
    // 确保在[0,1]范围内，同时利用10bit精度
    r = clamp(fract(r), 0.0, 1.0);
    g = clamp(fract(g), 0.0, 1.0);
    b = clamp(fract(b), 0.0, 1.0);
    
    return vec3(r, g, b);
}

// 简化SMPTE彩条（横向8等分）
vec3 colorBars(vec2 uv) {
    float x = uv.x;
    int idx = int(floor(x * 8.0));
    if (idx == 0) return vec3(1.0, 1.0, 1.0);       // 白
    if (idx == 1) return vec3(1.0, 1.0, 0.0);       // 黄
    if (idx == 2) return vec3(0.0, 1.0, 1.0);       // 青
    if (idx == 3) return vec3(0.0, 1.0, 0.0);       // 绿
    if (idx == 4) return vec3(1.0, 0.0, 1.0);       // 品红
    if (idx == 5) return vec3(1.0, 0.0, 0.0);       // 红
    if (idx == 6) return vec3(0.0, 0.0, 1.0);       // 蓝
    return vec3(0.0, 0.0, 0.0);                     // 黑
}

// 渐变与轻微噪声（观察色带/抖动）
vec3 smoothGradient(vec2 uv, float time) {
    float g = uv.x;
    // 轻微蓝噪声，减少条带感
    vec2 pixelPos = uv * uResolution;
    float noise = fract(sin(dot(pixelPos + time, vec2(12.9898, 78.233))) * 43758.5453);
    g = clamp(g + (noise - 0.5) * 0.01, 0.0, 1.0);
    return vec3(g);
}

// 网格（32px间距，1px线宽）
vec3 gridPattern(vec2 uv, float time) {
    vec2 p = uv * uResolution;
    float spacing = 32.0;
    float lw = 1.0;
    float fx = fract(p.x / spacing);
    float fy = fract(p.y / spacing);
    float mask = (fx < lw / spacing || fy < lw / spacing) ? 1.0 : 0.0;
    // 背景微动态，避免静态压缩
    float bg = 0.10 + 0.05 * sin(time * 0.6);
    return mix(vec3(bg), vec3(1.0), mask);
}

// 移动竖向亮条（测试拖影/过冲）
vec3 movingBar(vec2 uv, float time) {
    float speed = 0.25; // 周期约4秒
    float pos = fract(time * speed);
    float dx = abs(uv.x - pos);
    dx = min(dx, 1.0 - dx); // 环绕距离
    float halfWidth = 0.05;
    float bar = step(dx, halfWidth);
    // 前缘加高亮边
    float edge = smoothstep(halfWidth, halfWidth - 0.01, dx);
    vec3 bg = vec3(0.0);
    vec3 barColor = vec3(1.0);
    vec3 edgeColor = vec3(1.0, 1.0, 0.3);
    vec3 col = mix(bg, barColor, bar);
    col = mix(col, edgeColor, edge * 0.6);
    return col;
}

// UFO风格移动目标（多行不同速度，测试运动清晰度）
vec3 ufoPattern(vec2 uv, float time) {
    vec3 bg = vec3(0.02);
    vec3 col = bg;
    int rowCount = 3;
    float sizeScale = 1.0;
    for (int i = 0; i < rowCount; ++i) {
        float y = mix(0.2, 0.8, (float(i) + 0.5) / float(rowCount));
        float baseSpeed = mix(0.6, 2.5, float(i) / max(1.0, float(rowCount) - 1.0));
        float pos = fract(time * baseSpeed);
        vec2 c = vec2(pos, y);
        // 椭圆飞船主体
        vec2 d = (uv - c);
        d.x *= 2.0; // 拉伸
        float baseR = 0.08 * sizeScale;
        float body = smoothstep(baseR, baseR - 0.005, length(d));
        // 圆顶
        vec2 domeD = uv - (c + vec2(0.0, 0.035 * sizeScale));
        float dome = smoothstep(0.05 * sizeScale, 0.045 * sizeScale, length(domeD));
        // 尾焰
        float trail = exp(-abs(uv.x - c.x) * 30.0) * smoothstep(0.02 * sizeScale, 0.0, abs(uv.y - y));
        vec3 ship = mix(vec3(0.1, 0.8, 1.0), vec3(1.0), dome) * 0.9;
        vec3 shipBody = mix(vec3(0.1), vec3(0.9), body);
        vec3 flame = vec3(1.0, 0.8, 0.2) * trail;
        col = max(col, shipBody);
        col = max(col, ship);
        col = max(col, flame);
    }
    return col;
}

// 1px棋盘反相闪烁（时域极限，打满过渡）
vec3 temporalFlip(vec2 uv, float time) {
    vec2 p = uv * uResolution;
    float cb = mod(floor(p.x) + floor(p.y), 2.0);
    float flip = mod(floor(time * 120.0), 2.0); // 120Hz 反相
    float v = abs(cb - flip);
    return vec3(v);
}

// Zone plate（同心高频，覆盖各向频率）
vec3 zonePlate(vec2 uv, float time) {
    vec2 c = uv - vec2(0.5);
    c *= 2.0;
    float r2 = dot(c, c);
    float w = 90.0; // 频率权重
    float v = 0.5 + 0.5 * sin(w * r2 + time * 1.2);
    // 三通道相移，避免等灰
    float r = v;
    float g = 0.5 + 0.5 * sin(w * r2 + time * 1.2 + 2.1);
    float b = 0.5 + 0.5 * sin(w * r2 + time * 1.2 + 4.2);
    return vec3(r, g, b);
}

// 位平面闪烁：在10bit量化上按位翻转（时域抖动）
vec3 bitPlaneFlicker(vec2 uv, float time) {
    float v = clamp(uv.x, 0.0, 1.0);
    int q = int(floor(v * 1023.0 + 0.5));
    int bitIdx = int(mod(floor(time * 2.0), 5.0)); // LSB..bit4 轮换
    int phase = int(mod(floor(time * 120.0), 2.0));
    if (phase == 1) {
        q ^= (1 << bitIdx);
    }
    float outv = clamp(float(q) / 1023.0, 0.0, 1.0);
    // 通道交错不同位平面
    int bitIdxG = (bitIdx + 1) % 5;
    int bitIdxB = (bitIdx + 2) % 5;
    int qg = int(floor(uv.y * 1023.0 + 0.5));
    int qb = int(floor(fract(uv.x + uv.y) * 1023.0 + 0.5));
    if (phase == 1) { qg ^= (1 << bitIdxG); qb ^= (1 << bitIdxB); }
    return vec3(outv, float(qg) / 1023.0, float(qb) / 1023.0);
}

// 彩色棋盘轮换：R/G/B 在棋盘上轮换，时域相位不同
vec3 colorCheckerCycle(vec2 uv, float time) {
    float s = 24.0;
    vec2 cell = floor(uv * s);
    float cb = mod(cell.x + cell.y, 2.0);
    float phase = mod(floor(time * 2.0), 3.0);
    vec3 c;
    if (phase < 0.5) c = vec3(1.0, 0.0, 0.0);
    else if (phase < 1.5) c = vec3(0.0, 1.0, 0.0);
    else c = vec3(0.0, 0.0, 1.0);
    return mix(vec3(0.0), c, cb);
}

// 蓝噪声滚动：高频伪蓝噪声，沿对角方向滚动
vec3 blueNoiseScroll(vec2 uv, float time) {
    vec2 p = uv * uResolution / 2.0 + vec2(time * 60.0, time * 47.0);
    float n = fract(sin(dot(floor(p), vec2(12.9898, 78.233))) * 43758.5453);
    float n2 = fract(sin(dot(floor(p + 23.0), vec2(39.3468, 11.135))) * 24634.6345);
    float v = clamp((n * 0.7 + n2 * 0.3), 0.0, 1.0);
    // 三通道相移 + 轻度时域抖动
    float r = fract(v + 0.33);
    float g = fract(v + 0.66);
    float b = v;
    return vec3(r, g, b);
}

// 径向相位扫频：动态改变径向频率，覆盖不同空间频率
vec3 radialPhaseSweep(vec2 uv, float time) {
    vec2 c = uv - vec2(0.5);
    float r = length(c);
    float k = mix(10.0, 250.0, 0.5 + 0.5 * sin(time * 0.7));
    float v = 0.5 + 0.5 * sin(k * r + time * 2.0);
    return vec3(v);
}

// 旋转楔形线：角向高频条纹，随时间旋转
vec3 wedgeSpin(vec2 uv, float time) {
    vec2 c = uv - vec2(0.5);
    float a = atan(c.y, c.x) + time * 0.8;
    float stripes = sin(a * 120.0);
    float v = stripes > 0.0 ? 1.0 : 0.0;
    return vec3(v);
}
// 棋盘格（高对比）
vec3 checker(vec2 uv, float time) {
    float s = 16.0; // 固定密度
    vec2 gcell = floor(uv * s);
    float cb = mod(gcell.x + gcell.y, 2.0);
    return mix(vec3(0.0), vec3(1.0), cb);
}

// RGBW 全屏轮播
vec3 rgbwCycle(float time) {
    float t = floor(mod(time * 0.5, 4.0));
    if (t < 0.5) return vec3(1.0, 0.0, 0.0);
    else if (t < 1.5) return vec3(0.0, 1.0, 0.0);
    else if (t < 2.5) return vec3(0.0, 0.0, 1.0);
    else return vec3(1.0);
}

// Siemens Star（放射状楔形）
vec3 siemensStar(vec2 uv) {
    vec2 c = uv - vec2(0.5);
    float a = atan(c.y, c.x);
    float stripes = cos(a * 100.0);
    float v = stripes > 0.0 ? 1.0 : 0.0;
    return vec3(v);
}

// 水平分辨率楔形（沿X方向增加竖向条纹密度）
vec3 horizWedge(vec2 uv) {
    float k = 400.0;
    float v = sin(k * uv.x * uv.x);
    return vec3(v > 0.0 ? 1.0 : 0.0);
}

// 垂直分辨率楔形（沿Y方向增加横向条纹密度）
vec3 vertWedge(vec2 uv) {
    float k = 400.0;
    float v = sin(k * uv.y * uv.y);
    return vec3(v > 0.0 ? 1.0 : 0.0);
}

// 同心圆环（静态）
vec3 concentricRings(vec2 uv) {
    vec2 c = uv - vec2(0.5);
    float r2 = dot(c, c);
    float v = sin(120.0 * r2);
    return vec3(v > 0.0 ? 1.0 : 0.0);
}

// 点栅格（网格点)
vec3 dotGrid(vec2 uv) {
    vec2 p = uv * uResolution;
    vec2 g = fract(p / 16.0);
    // 距离格点最近点
    vec2 d = min(g, 1.0 - g);
    float r = length((d - 0.5/16.0) * 16.0);
    float dotv = smoothstep(0.15, 0.05, r);
    return vec3(dotv);
}

// Gamma Checker（步进灰+嵌入棋盘）
vec3 gammaChecker(vec2 uv) {
    int steps = 8;
    int idx = int(floor(uv.x * float(steps)));
    float g = (float(idx) + 0.5) / float(steps);
    // 内嵌棋盘
    float n = 16.0;
    vec2 p = uv * n;
    float cb = mod(floor(p.x) + floor(p.y), 2.0);
    float amp = 0.15; // 对比振幅
    float v = clamp(g + (cb > 0.5 ? amp : -amp) * (1.0 - g) * g, 0.0, 1.0);
    return vec3(v);
}

void main()
{
    vec2 uv = TexCoord;

    // 半透明面板直接返回（避免受内容模式影响）
    if (uColorVariation == -1) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.7);
        return;
    }

    vec3 color;
    if (uCategory == 0) {
        // STATIC_GROUP: 常用静态测试图样
        // 索引定义：
        // 0: 彩条, 1: 灰阶渐变, 2: 16阶灰条, 3: 1px细棋盘, 4: 粗棋盘,
        // 5: 32px网格, 6: 8px网格, 7: RGB竖条, 8: 十字/三分线,
        // 9: 黑, 10: 白, 11: 红, 12: 绿, 13: 蓝, 14: 50%灰,
        // 15: Siemens Star, 16: 水平楔形, 17: 垂直楔形, 18: 同心圆环, 19: 点栅格, 20: Gamma Checker
        int idx = uContentMode;
        if (idx == 0) {
            color = colorBars(uv);
        } else if (idx == 1) {
            // 纯渐变（无抖动）
            float g = clamp(uv.x, 0.0, 1.0);
            color = vec3(g);
        } else if (idx == 2) {
            int steps = 16;
            int bar = int(floor(uv.x * float(steps)));
            float v = (float(bar) + 0.5) / float(steps);
            color = vec3(v);
        } else if (idx == 3) {
            // 1px细棋盘
            vec2 p = uv * uResolution;
            float cb = mod(floor(p.x) + floor(p.y), 2.0);
            color = vec3(cb);
        } else if (idx == 4) {
            color = checker(uv, uTime);
        } else if (idx == 5) {
            color = gridPattern(uv, 0.0);
        } else if (idx == 6) {
            // 8px网格
            vec2 p = uv * uResolution; float spacing = 8.0; float lw = 1.0;
            float fx = fract(p.x / spacing); float fy = fract(p.y / spacing);
            float mask = (fx < lw / spacing || fy < lw / spacing) ? 1.0 : 0.0;
            color = mix(vec3(0.15), vec3(1.0), mask);
        } else if (idx == 7) {
            // RGB 竖条（每3条循环）
            int b = int(floor(uv.x * 90.0));
            int m = b % 3;
            if (m == 0) color = vec3(1.0, 0.0, 0.0);
            else if (m == 1) color = vec3(0.0, 1.0, 0.0);
            else color = vec3(0.0, 0.0, 1.0);
        } else if (idx == 8) {
            // 十字 + 三分线
            vec2 p = uv * uResolution; float lw = 1.0;
            float cx = abs(uv.x - 0.5) * uResolution.x; // 中心竖线
            float cy = abs(uv.y - 0.5) * uResolution.y; // 中心横线
            float t1x = abs(uv.x - 1.0/3.0) * uResolution.x;
            float t2x = abs(uv.x - 2.0/3.0) * uResolution.x;
            float t1y = abs(uv.y - 1.0/3.0) * uResolution.y;
            float t2y = abs(uv.y - 2.0/3.0) * uResolution.y;
            float line = 0.0;
            line += step(cx, lw) + step(cy, lw);
            line += step(t1x, lw) + step(t2x, lw) + step(t1y, lw) + step(t2y, lw);
            color = mix(vec3(0.0), vec3(1.0), clamp(line, 0.0, 1.0));
        } else if (idx == 9) {
            color = vec3(0.0);
        } else if (idx == 10) {
            color = vec3(1.0);
        } else if (idx == 11) {
            color = vec3(1.0, 0.0, 0.0);
        } else if (idx == 12) {
            color = vec3(0.0, 1.0, 0.0);
        } else if (idx == 13) {
            color = vec3(0.0, 0.0, 1.0);
        } else if (idx == 14) {
            color = vec3(0.5);
        } else if (idx == 15) {
            color = siemensStar(uv);
        } else if (idx == 16) {
            color = horizWedge(uv);
        } else if (idx == 17) {
            color = vertWedge(uv);
        } else if (idx == 18) {
            color = concentricRings(uv);
        } else if (idx == 19) {
            color = dotGrid(uv);
        } else if (idx == 20) {
            color = gammaChecker(uv);
        } else {
            color = vec3(0.0);
        }
    } else {
        // DYNAMIC_GROUP: 带宽/帧率压力
        // 0..6: generateComplexColor 各变体（去重后）；7: RGBW轮播；8: 移动亮条
        // 9: UFO风格移动；10: 1px棋盘反相闪烁；11: Zone Plate
        // 12: 位平面闪烁；13: 彩色棋盘轮换；14: 蓝噪声滚动
        // 15: 径向相位扫频；16: 旋转楔形线
        int idx = uContentMode;
        if (idx <= 6) {
            color = generateComplexColor(uv, uTime, idx);
        } else if (idx == 7) {
            color = rgbwCycle(uTime);
        } else if (idx == 8) {
            color = movingBar(uv, uTime);
        } else if (idx == 9) {
            color = ufoPattern(uv, uTime);
        } else if (idx == 10) {
            color = temporalFlip(uv, uTime);
        } else if (idx == 11) {
            color = zonePlate(uv, uTime);
        } else if (idx == 12) {
            color = bitPlaneFlicker(uv, uTime);
        } else if (idx == 13) {
            color = colorCheckerCycle(uv, uTime);
        } else if (idx == 14) {
            color = blueNoiseScroll(uv, uTime);
        } else if (idx == 15) {
            color = radialPhaseSweep(uv, uTime);
        } else if (idx == 16) {
            color = wedgeSpin(uv, uTime);
        } else {
            color = generateComplexColor(uv, uTime, 0);
        }
    }

    FragColor = vec4(color, 1.0);
}
)";

MonitorTest::MonitorTest() 
    : window(nullptr)
    , VAO(0)
    , VBO(0)
    , currentTime(0.0)
    , frameCount(0)
    , currentFps(0.0)
    , targetFrameTime(1.0/120.0)
    , frameTimeMs(0.0)
    , windowWidth(0)
    , windowHeight(0)
{
    startTime = std::chrono::high_resolution_clock::now();
    lastFrameTime = startTime;
    lastFpsReportTime = startTime;
    lastLoopTime = startTime;
}

MonitorTest::~MonitorTest() {
    cleanup();
}

bool MonitorTest::initialize() {
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        std::cerr << "初始化GLFW失败" << std::endl;
        return false;
    }
    
    if (!initializeWindow()) {
        return false;
    }
    
    if (!initializeOpenGL()) {
        return false;
    }
    
    setupQuad();
    setupShaders();
    
    printSystemInfo();
    printControls();
    
    return true;
}

bool MonitorTest::initializeWindow() {
    // 设置GLFW窗口属性
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // 获取主显示器
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    windowWidth = mode->width;
    windowHeight = mode->height;
    
    std::cout << "检测到显示器分辨率: " << windowWidth << "x" << windowHeight 
              << " @" << mode->refreshRate << "Hz" << std::endl;
    
    // 创建全屏窗口
    window = glfwCreateWindow(windowWidth, windowHeight, "Display Hardware Test", monitor, nullptr);
    
    if (!window) {
        std::cerr << "创建GLFW窗口失败" << std::endl;
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(window);
    
    // 设置回调函数
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    // 根据设置启用/禁用垂直同步
    glfwSwapInterval(config.vsyncEnabled ? 1 : 0);
    
    return true;
}

bool MonitorTest::initializeOpenGL() {
    // 初始化GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "初始化GLEW失败" << std::endl;
        return false;
    }
    
    // 设置视口
    glViewport(0, 0, windowWidth, windowHeight);
    
    // OpenGL状态设置
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // 背景清屏色
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    return true;
}

void MonitorTest::setupQuad() {
    // 全屏四边形顶点数据（NDC坐标 + UV）
    float vertices[] = {
        // 位置(x,y)    纹理坐标(u,v)
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 1.0f
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    static GLuint EBO = 0; // 保持EBO生命周期与进程一致，避免被释放
    if (EBO == 0) glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // 位置属性
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void MonitorTest::setupShaders() {
    // 主场景着色器
    shader = std::make_unique<Shader>(vertexShaderSource, fragmentShaderSource);

    // 文本渲染器（FreeType）
    textRenderer = std::make_unique<TextRenderer>();
    if (!textRenderer->Init(windowWidth, windowHeight)) {
        std::cerr << "文本渲染初始化失败（FreeType）" << std::endl;
    } else {
        std::string fontPath = chooseFontPath();
        if (fontPath.empty()) {
            std::cerr << "未找到可用中文字体。请安装 Noto Sans CJK、思源黑体或文泉驿字体。" << std::endl;
        } else {
            if (!textRenderer->LoadFont(fontPath, 24)) {
                std::cerr << "加载字体失败: " << fontPath << std::endl;
            } else {
                std::cout << "已加载字体: " << fontPath << std::endl;
            }
        }
    }
}

static std::string toSafeString(const GLubyte* s) {
    return s ? reinterpret_cast<const char*>(s) : std::string("未知");
}

void MonitorTest::renderStatusOverlay() {
    // 半透明面板背景（使用主shader + 限制视口）
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader->use();
    shader->setFloat("uTime", 0.0f); // 静态背景
    shader->setVec2("uResolution", static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    shader->setInt("uCategory", 0);
    shader->setInt("uContentMode", 0);
    shader->setInt("uColorVariation", -1); // 特殊值用于背景
    
    glBindVertexArray(VAO);

    // 动态计算面板大小（基于字体测量）
    const float margin = 24.0f;      // 面板与屏幕边缘的距离
    const float topMargin = 40.0f;   // 顶部留白
    const float padding = 16.0f;     // 面板内边距（更紧凑）
    const float scale = 1.0f;
    const float lh = textRenderer ? textRenderer->GetLineHeightPx(scale) : 28.0f;
    const float asc = textRenderer ? textRenderer->GetAscenderPx(scale) : lh * 0.8f;
    const float desc = textRenderer ? textRenderer->GetDescenderPx(scale) : lh * 0.2f;

    // 左侧文本内容（按渲染顺序）
    struct Line { std::string txt; float r, g, b; bool extraGap; };
    std::vector<Line> leftLines;
    // 配色：标题高对比、正文近白
    const float tr = 0.92f, tg = 0.94f, tb = 0.96f; // 正文
    leftLines.push_back({"GPU 信息", 0.30f, 0.95f, 0.50f, false});
    std::string glVersion = std::string("OpenGL 版本: ") + toSafeString(glGetString(GL_VERSION));
    std::string glVendor  = std::string("显卡厂商: ") + toSafeString(glGetString(GL_VENDOR));
    std::string glRend    = std::string("显卡型号: ") + toSafeString(glGetString(GL_RENDERER));
    std::string res       = "分辨率: " + std::to_string(windowWidth) + "x" + std::to_string(windowHeight);
    leftLines.push_back({glVersion, tr, tg, tb, false});
    leftLines.push_back({glVendor,  tr, tg, tb, false});
    leftLines.push_back({glRend,    tr, tg, tb, false});
    leftLines.push_back({res,       tr, tg, tb, true}); // 额外间距

    leftLines.push_back({"显示器信息", 0.40f, 0.80f, 1.00f, false});
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    std::string refresh = mode ? ("刷新率: " + std::to_string(mode->refreshRate) + " Hz") : "刷新率: 未知";
    leftLines.push_back({refresh, tr, tg, tb, true});

    leftLines.push_back({"实时测试信息", 1.00f, 0.75f, 0.30f, false});
    std::string modeStr;
    switch (config.mode) {
        case TestMode::FIXED_FPS:       modeStr = "固定帧率"; break;
        case TestMode::JITTER_FPS:      modeStr = "抖动模式"; break;
        case TestMode::OSCILLATION_FPS: modeStr = "震荡模式"; break;
    }
    std::ostringstream oss;
    oss << "FPS: " << static_cast<int>(currentFps);
    float ratio = static_cast<float>(std::min(currentFps / 120.0, 1.0));
    leftLines.push_back({oss.str(), 1.0f - ratio, ratio, 0.2f, false});
    std::ostringstream ft;
    ft << std::fixed << std::setprecision(2) << "帧时间: " << frameTimeMs << " ms  (目标: " << (targetFrameTime * 1000.0) << " ms)";
    leftLines.push_back({ft.str(), tr, tg, tb, false});
    leftLines.push_back({"帧率模式: " + modeStr, tr, tg, tb, false});
    std::string groupStr = (config.category == Category::STATIC_GROUP) ? "静态图样" : "动态压力";
    auto staticName = [&](int idx)->std::string {
        switch (idx) {
            case 0: return "彩条"; case 1: return "灰阶渐变"; case 2: return "16阶灰条";
            case 3: return "细棋盘(1px)"; case 4: return "粗棋盘"; case 5: return "网格32px";
            case 6: return "网格8px"; case 7: return "RGB竖条"; case 8: return "十字+三分线";
            case 9: return "纯黑"; case 10: return "纯白"; case 11: return "纯红";
            case 12: return "纯绿"; case 13: return "纯蓝"; case 14: return "50%灰";
            case 15: return "Siemens Star"; case 16: return "水平楔形"; case 17: return "垂直楔形";
            case 18: return "同心圆环"; case 19: return "点栅格"; case 20: return "Gamma Checker";
        }
        return "静态图样";
    };
    auto dynamicName = [&](int idx)->std::string {
        switch (idx) {
            case 0: return "动态: 渐变+噪声"; case 1: return "动态: 高频噪声";
            case 2: return "动态: 波浪干涉"; case 3: return "动态: 螺旋";
            case 4: return "动态: 棋盘扰动"; case 5: return "动态: 辐射彩虹";
            case 6: return "动态: 斜向干涉";
            case 7: return "动态: RGBW轮播"; case 8: return "动态: 移动亮条";
            case 9: return "动态: UFO移动"; case 10: return "动态: 1px反相闪烁";
            case 11: return "动态: Zone Plate"; case 12: return "动态: 位平面闪烁";
            case 13: return "动态: 彩色棋盘轮换"; case 14: return "动态: 蓝噪声滚动";
            case 15: return "动态: 径向相位扫频"; case 16: return "动态: 旋转楔形线";
        }
        return "动态图样";
    };
    std::string patStr = (config.category == Category::STATIC_GROUP)
        ? staticName(config.staticMode)
        : dynamicName(config.dynamicMode);
    leftLines.push_back({"模式组: " + groupStr, tr, tg, tb, false});
    leftLines.push_back({"图样: " + patStr, tr, tg, tb, false});
    // 垂直同步状态
    leftLines.push_back({std::string("垂直同步: ") + (config.vsyncEnabled ? "开" : "关"), tr, tg, tb, false});
    // 自动轮播/日志状态
    {
        std::ostringstream as;
        as << "自动轮播: " << (autoplayEnabled ? "开 (" + std::to_string(autoplayIntervalSec) + "s)" : "关");
        leftLines.push_back({as.str(), tr, tg, tb, false});
    }
    leftLines.push_back({std::string("日志记录: ") + (loggingEnabled ? "开" : "关"), tr, tg, tb, false});
    leftLines.push_back({"目标帧率: " + std::to_string(config.targetFps), tr, tg, tb, false});
    leftLines.push_back({"范围: " + std::to_string(config.minFps) + "~" + std::to_string(config.maxFps), tr, tg, tb, config.isPaused});
    if (config.isPaused) {
        leftLines.push_back({"状态: 已暂停", 1.0f, 0.2f, 0.2f, false});
    }

    float leftMaxW = 0.0f; float leftTotalH = 0.0f; float leftGaps = 0.0f;
    for (const auto& ln : leftLines) {
        float w = textRenderer ? textRenderer->MeasureTextWidth(ln.txt, scale) : static_cast<float>(ln.txt.size() * 10);
        if (w > leftMaxW) leftMaxW = w;
        leftTotalH += lh;
        if (ln.extraGap) leftGaps += 8.0f;
    }
    // 内容高度：asc + (N-1)*lh + desc + gaps
    const int Nleft = static_cast<int>(leftLines.size());
    float leftContentH = asc + (Nleft > 0 ? (Nleft - 1) * lh : 0.0f) + desc + leftGaps;
    GLint panelW = static_cast<GLint>(std::ceil(leftMaxW + padding * 2));
    GLint panelH = static_cast<GLint>(std::ceil(leftContentH + padding * 2));

    // 右侧控制说明
    std::vector<std::string> rightTexts;
    rightTexts.push_back("控制说明");
    rightTexts.push_back("ESC   - 退出程序");
    rightTexts.push_back("P     - 暂停/继续");
    rightTexts.push_back("SPACE - 切换模式组");
    rightTexts.push_back("←/→   - 上一/下一图样");
    rightTexts.push_back("V     - 垂直同步 开/关");
    rightTexts.push_back("A     - 自动轮播 开/关");
    rightTexts.push_back("[/]   - 轮播间隔 -/+ 1s");
    rightTexts.push_back("K     - 截图保存 (PPM)");
    rightTexts.push_back("T     - 日志 开/关");
    rightTexts.push_back("V     - 垂直同步 开/关");
    float rightMaxW = 0.0f; float rightTotalH = 0.0f;
    for (const auto& s : rightTexts) {
        float w = textRenderer ? textRenderer->MeasureTextWidth(s, scale) : static_cast<float>(s.size() * 10);
        if (w > rightMaxW) rightMaxW = w;
        rightTotalH += lh;
    }
    float rightContentH = asc + (static_cast<int>(rightTexts.size()) > 0 ? (static_cast<int>(rightTexts.size()) - 1) * lh : 0.0f) + desc;
    GLint rightW = static_cast<GLint>(std::ceil(rightMaxW + padding * 2));
    GLint rightH = static_cast<GLint>(std::ceil(rightContentH + padding * 2));
    GLint rightX = windowWidth - (rightW + static_cast<GLint>(margin));
    GLint rightY = windowHeight - (rightH + static_cast<GLint>(topMargin));

    // 绘制左/右背景面板
    glViewport(static_cast<GLint>(margin), windowHeight - (panelH + static_cast<GLint>(topMargin)), panelW, panelH);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glViewport(rightX, rightY, rightW, rightH);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // 恢复完整视口
    glViewport(0, 0, windowWidth, windowHeight);
    glBindVertexArray(0);

    // 文本内容（UTF-8，Windows/Linux中文均可）
    if (textRenderer) {
        float x = margin + padding;
        float y = topMargin + padding + asc; // 顶部保持 padding 到字形顶部
        for (const auto& ln : leftLines) {
            textRenderer->RenderText(ln.txt, x, y, scale, ln.r, ln.g, ln.b);
            y += lh;
            if (ln.extraGap) y += 8.0f;
        }

        float rx = static_cast<float>(rightX) + padding;
        float ry = topMargin + padding + asc;
        // 右侧标题颜色与正文颜色
        for (size_t i = 0; i < rightTexts.size(); ++i) {
            if (i == 0) {
                textRenderer->RenderText(rightTexts[i], rx, ry, scale, 1.0f, 0.90f, 0.40f);
            } else {
                textRenderer->RenderText(rightTexts[i], rx, ry, scale, tr, tg, tb);
            }
            ry += lh;
        }
    }

    glDisable(GL_BLEND);
}

void MonitorTest::run() {
    while (!glfwWindowShouldClose(window)) {
        handleInput();
        
        if (!config.isPaused) {
            update();
            render();
        } else {
            // 暂停时也渲染一次覆盖层，保持提示显示
            render();
        }
        
        // 帧率控制
        if (config.mode == TestMode::FIXED_FPS) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(now - lastFrameTime).count();
            
            if (elapsed < targetFrameTime) {
                double sleepTime = targetFrameTime - elapsed;
                std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
            }
            
            lastFrameTime = std::chrono::high_resolution_clock::now();
        } else {
            lastFrameTime = std::chrono::high_resolution_clock::now();
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        // 更新帧时间（毫秒，指数平滑）
        auto loopEnd = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double, std::milli>(loopEnd - lastLoopTime).count();
        lastLoopTime = loopEnd;
        if (frameTimeMs <= 0.0) frameTimeMs = dt; else frameTimeMs = frameTimeMs * 0.9 + dt * 0.1;

        frameCount++;
        reportFps();
    }
}

void MonitorTest::update() {
    auto now = std::chrono::high_resolution_clock::now();
    currentTime = std::chrono::duration<double>(now - startTime).count();
    
    updateFrameRate();
}

void MonitorTest::render() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    shader->use();
    
    // 更新uniform变量
    shader->setFloat("uTime", static_cast<float>(currentTime));
    shader->setVec2("uResolution", static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    // 设置分类与子模式
    int cat = (config.category == Category::STATIC_GROUP) ? 0 : 1;
    int sub = (cat == 0) ? config.staticMode : config.dynamicMode;
    shader->setInt("uCategory", cat);
    shader->setInt("uContentMode", sub);
    // 动态复杂内容的子变体（用于 generateComplexColor）
    int dynVar = (cat == 1 && sub <= 6) ? sub : 0;
    shader->setInt("uColorVariation", dynVar);
    // 无参数传递（已去掉可调参数）
    
    // 绘制全屏四边形
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // 渲染状态覆盖层
    renderStatusOverlay();
}

void MonitorTest::handleInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void MonitorTest::updateFrameRate() {
    double targetFps = calculateTargetFps();
    targetFrameTime = 1.0 / targetFps;
}

double MonitorTest::calculateTargetFps() {
    switch (config.mode) {
        case TestMode::FIXED_FPS:
            return config.targetFps;
            
        case TestMode::JITTER_FPS: {
            // 随机帧率跳变
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(config.minFps, config.maxFps);
            return dis(gen);
        }
        
        case TestMode::OSCILLATION_FPS: {
            // 正弦波震荡帧率
            double range = (config.maxFps - config.minFps) * 0.5;
            double center = config.minFps + range;
            return center + range * std::sin(currentTime * 0.5);
        }
    }
    
    return config.targetFps;
}

void MonitorTest::reportFps() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(now - lastFpsReportTime).count();
    
    if (elapsed >= 1.0) {  // 每秒报告一次
        currentFps = frameCount / elapsed;
        
        std::string modeStr;
        switch (config.mode) {
            case TestMode::FIXED_FPS: modeStr = "固定帧率"; break;
            case TestMode::JITTER_FPS: modeStr = "抖动模式"; break;
            case TestMode::OSCILLATION_FPS: modeStr = "震荡模式"; break;
        }
        
        auto staticName = [&](int idx)->std::string {
            switch (idx) {
                case 0: return "彩条"; case 1: return "灰阶渐变"; case 2: return "16阶灰条";
                case 3: return "细棋盘(1px)"; case 4: return "粗棋盘"; case 5: return "网格32px";
                case 6: return "网格8px"; case 7: return "RGB竖条"; case 8: return "十字+三分线";
                case 9: return "纯黑"; case 10: return "纯白"; case 11: return "纯红";
                case 12: return "纯绿"; case 13: return "纯蓝"; case 14: return "50%灰";
            }
            return "静态图样";
        };
        auto dynamicName = [&](int idx)->std::string {
            switch (idx) {
                case 0: return "动态: 渐变+噪声"; case 1: return "动态: 高频噪声";
                case 2: return "动态: 波浪干涉"; case 3: return "动态: 螺旋";
                case 4: return "动态: 棋盘扰动"; case 5: return "动态: 辐射彩虹";
                case 6: return "动态: 斜向干涉"; case 7: return "动态: RGBW轮播";
                case 8: return "动态: 移动亮条"; case 9: return "动态: UFO移动";
                case 10: return "动态: 1px反相闪烁"; case 11: return "动态: Zone Plate";
                case 12: return "动态: 位平面闪烁"; case 13: return "动态: 彩色棋盘轮换";
                case 14: return "动态: 蓝噪声滚动"; case 15: return "动态: 径向相位扫频";
                case 16: return "动态: 旋转楔形线";
            }
            return "动态图样";
        };
        std::string groupStr = (config.category == Category::STATIC_GROUP) ? "静态图样" : "动态压力";
        std::string patStr = (config.category == Category::STATIC_GROUP)
            ? staticName(config.staticMode)
            : dynamicName(config.dynamicMode);

        std::cout << "当前帧率: " << static_cast<int>(currentFps) << " FPS | "
                  << "帧时间: " << std::fixed << std::setprecision(2) << frameTimeMs << " ms | "
                  << "帧率模式: " << modeStr << " | "
                  << "模式组: " << groupStr << " | "
                  << "图样: " << patStr << " | "
                  << (config.isPaused ? "已暂停" : "运行中") << std::endl;

        if (loggingEnabled && logStream.is_open()) {
            double tsec = std::chrono::duration<double>(now - startTime).count();
            logStream << std::fixed << std::setprecision(2)
                      << tsec << ',' << currentFps << ',' << frameTimeMs << ','
                      << (config.category == Category::STATIC_GROUP ? "static" : "dynamic") << ','
                      << '"' << patStr << '"' << ','
                      << config.targetFps << ',' << modeStr
                      << '\n';
            logStream.flush();
        }
        
        frameCount = 0;
        lastFpsReportTime = now;
    }
}

void MonitorTest::cleanup() {
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    
    shader.reset();
    textRenderer.reset();
    
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    
    glfwTerminate();
}

// 静态回调函数实现
void MonitorTest::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    MonitorTest* test = static_cast<MonitorTest*>(glfwGetWindowUserPointer(window));
    
    if (!test) return;

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_P:
                test->config.isPaused = !test->config.isPaused;
                std::cout << (test->config.isPaused ? "测试已暂停" : "测试已恢复") << std::endl;
                break;

            case GLFW_KEY_SPACE: {
                // 切换模式组（静态/动态）
                test->config.category = (test->config.category == Category::STATIC_GROUP)
                    ? Category::DYNAMIC_GROUP : Category::STATIC_GROUP;
                std::cout << "模式组: " << (test->config.category == Category::STATIC_GROUP ? "静态图样" : "动态压力") << std::endl;
                break;
            }

            case GLFW_KEY_RIGHT: {
                bool isStatic = (test->config.category == Category::STATIC_GROUP);
                if (isStatic) {
                    test->config.staticMode = (test->config.staticMode + 1) % 21;
                    std::cout << "静态图样索引: " << test->config.staticMode << std::endl;
                } else {
                    test->config.dynamicMode = (test->config.dynamicMode + 1) % 17;
                    std::cout << "动态图样索引: " << test->config.dynamicMode << std::endl;
                }
                break;
            }

            case GLFW_KEY_LEFT: {
                bool isStatic = (test->config.category == Category::STATIC_GROUP);
                if (isStatic) {
                    test->config.staticMode = (test->config.staticMode + 21 - 1) % 21;
                    std::cout << "静态图样索引: " << test->config.staticMode << std::endl;
                } else {
                    test->config.dynamicMode = (test->config.dynamicMode + 17 - 1) % 17;
                    std::cout << "动态图样索引: " << test->config.dynamicMode << std::endl;
                }
                break;
            }

            case GLFW_KEY_A: {
                test->autoplayEnabled = !test->autoplayEnabled;
                std::cout << (test->autoplayEnabled ? "自动轮播: 开" : "自动轮播: 关") << std::endl;
                if (test->autoplayEnabled) test->lastModeSwitchTime = std::chrono::high_resolution_clock::now();
                break;
            }
            case GLFW_KEY_LEFT_BRACKET: {
                test->autoplayIntervalSec = std::max(0.2, test->autoplayIntervalSec - 1.0);
                std::cout << "轮播间隔: " << test->autoplayIntervalSec << "s" << std::endl;
                break;
            }
            case GLFW_KEY_RIGHT_BRACKET: {
                test->autoplayIntervalSec = std::min(60.0, test->autoplayIntervalSec + 1.0);
                std::cout << "轮播间隔: " << test->autoplayIntervalSec << "s" << std::endl;
                break;
            }
            case GLFW_KEY_K: {
                bool ok = test->saveScreenshot();
                std::cout << (ok ? "截图已保存" : "截图失败") << std::endl;
                break;
            }
            case GLFW_KEY_T: {
                test->loggingEnabled = !test->loggingEnabled;
                if (test->loggingEnabled) {
                    // 打开日志
                    namespace fs = std::filesystem;
                    fs::create_directories("dist/logs");
                    auto now = std::chrono::system_clock::now();
                    auto t = std::chrono::system_clock::to_time_t(now);
                    std::tm tm{};
#ifdef _WIN32
                    localtime_s(&tm, &t);
#else
                    localtime_r(&t, &tm);
#endif
                    char buf[64];
                    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
                    test->logFilePath = std::string("dist/logs/log-") + buf + ".csv";
                    test->logStream.close();
                    test->logStream.open(test->logFilePath, std::ios::out | std::ios::trunc);
                    if (test->logStream.is_open()) {
                        test->logStream << "time_s,fps,frame_ms,group,pattern,target_fps,mode\n";
                        std::cout << "日志开启: " << test->logFilePath << std::endl;
                    } else {
                        test->loggingEnabled = false;
                        std::cerr << "打开日志失败: " << test->logFilePath << std::endl;
                    }
                } else {
                    if (test->logStream.is_open()) test->logStream.close();
                    std::cout << "日志关闭" << std::endl;
                }
                break;
            }

            case GLFW_KEY_V: {
                test->config.vsyncEnabled = !test->config.vsyncEnabled;
                glfwMakeContextCurrent(window);
                glfwSwapInterval(test->config.vsyncEnabled ? 1 : 0);
                std::cout << (test->config.vsyncEnabled ? "垂直同步: 开" : "垂直同步: 关") << std::endl;
                break;
            }

            default:
                break;
        }
    }
    // 取消参数微调（已移除）
}

void MonitorTest::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    MonitorTest* test = static_cast<MonitorTest*>(glfwGetWindowUserPointer(window));
    if (!test) return;
    test->windowWidth = width;
    test->windowHeight = height;
    if (test->textRenderer) test->textRenderer->SetScreenSize(width, height);
}

void MonitorTest::errorCallback(int error, const char* description) {
    std::cerr << "GLFW错误 " << error << ": " << description << std::endl;
}

bool MonitorTest::saveScreenshot() {
    namespace fs = std::filesystem;
    fs::create_directories("dist/screenshots");
    std::vector<unsigned char> pixels;
    pixels.resize(static_cast<size_t>(windowWidth) * static_cast<size_t>(windowHeight) * 3u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    // 写 PPM (P6)
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    std::string fname = std::string("dist/screenshots/shot-") + buf + ".ppm";
    std::ofstream out(fname, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << windowWidth << " " << windowHeight << "\n255\n";
    // 翻转Y方向（OpenGL原点在左下）
    const size_t rowBytes = static_cast<size_t>(windowWidth) * 3u;
    for (int y = windowHeight - 1; y >= 0; --y) {
        out.write(reinterpret_cast<const char*>(pixels.data() + static_cast<size_t>(y) * rowBytes), rowBytes);
    }
    out.close();
    std::cout << "已保存截图: " << fname << std::endl;
    return true;
}

void MonitorTest::printControls() const {
    std::cout << "\n=== 控制说明 ===" << std::endl;
    std::cout << "ESC    - 退出程序" << std::endl;
    std::cout << "P      - 暂停/继续" << std::endl;
    std::cout << "SPACE  - 切换模式组(静态/动态)" << std::endl;
    std::cout << "←/→     - 上一/下一图样" << std::endl;
    std::cout << "V      - 垂直同步 开/关" << std::endl;
    std::cout << "A      - 自动轮播 开/关" << std::endl;
    std::cout << "[ / ]  - 轮播间隔 -/+ 1s" << std::endl;
    std::cout << "K      - 截图保存 (PPM)" << std::endl;
    std::cout << "T      - 日志 开/关 (CSV)" << std::endl;
    std::cout << "===============\n" << std::endl;
}

void MonitorTest::printSystemInfo() const {
    std::cout << "\n=== 系统信息 ===" << std::endl;
    std::cout << "OpenGL版本: " << toSafeString(glGetString(GL_VERSION)) << std::endl;
    std::cout << "显卡厂商: " << toSafeString(glGetString(GL_VENDOR)) << std::endl;
    std::cout << "显卡型号: " << toSafeString(glGetString(GL_RENDERER)) << std::endl;
    std::cout << "分辨率: " << windowWidth << "x" << windowHeight << std::endl;
    std::cout << "目标: 10bit色深全带宽压力测试" << std::endl;
    std::cout << "================\n" << std::endl;
}

std::string MonitorTest::chooseFontPath() const {
    // 改为始终优先使用“系统默认字体”，不再读取工程内置/下载字体
    namespace fs = std::filesystem;

#ifdef HAS_FONTCONFIG
    // 使用 Fontconfig 查找适用于中文环境的系统无衬线字体
    if (FcInit()) {
        // 语义等同于 "sans:lang=zh-cn:scalable=true"
        FcPattern* pat = FcNameParse((const FcChar8*)"sans:lang=zh-cn:scalable=true");
        if (pat) {
            FcConfigSubstitute(nullptr, pat, FcMatchPattern);
            FcDefaultSubstitute(pat);
            FcResult result;
            FcPattern* font = FcFontMatch(nullptr, pat, &result);
            if (font) {
                FcChar8* file = nullptr;
                if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file) {
                    std::string path(reinterpret_cast<const char*>(file));
                    FcPatternDestroy(font);
                    FcPatternDestroy(pat);
                    FcFini();
                    if (!path.empty()) return path;
                }
                FcPatternDestroy(font);
            }
            FcPatternDestroy(pat);
        }
        FcFini();
    }
#endif

    // 若未启用 Fontconfig，则从常见系统路径中挑选一款字体（以系统默认字体为主）
#ifdef _WIN32
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\Microsoft YaHei UI.ttf",
        "C:\\Windows\\Fonts\\Microsoft YaHei.ttf",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc"
    };
#else
    const char* candidates[] = {
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" // 兜底（英文字形）
    };
#endif
    for (const char* s : candidates) {
        std::error_code ec;
        fs::path p = fs::u8path(s);
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
            return p.string();
        }
    }
    return std::string();
}
