#include "display_hardware_test.h"
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

// 10-bit 量化（0..1023）
float q10(float v) { return clamp(floor(clamp(v,0.0,1.0) * 1023.0 + 0.5) / 1023.0, 0.0, 1.0); }

vec3 hsv2rgb(vec3 c){
    vec3 p = abs(fract(vec3(c.x,c.x,c.x) + vec3(0.0,2.0/3.0,1.0/3.0)) * 6.0 - 3.0);
    vec3 rgb = clamp(p - 1.0, 0.0, 1.0);
    return c.z * mix(vec3(1.0), rgb, c.y);
}

// 高熵颜色场（避免大块重复色）
vec3 generateComplexColor(vec2 uv, float time, int variation) {
    if (variation == -1) return vec3(0.0);
    vec2 p = uv * uResolution;
    float t = time;

    // 基础哈希（避免使用纹理）
    float h1 = fract(sin(dot(floor(p), vec2(12.9898, 78.233)) + t * 19.19) * 43758.5453);
    float h2 = fract(sin(dot(floor(p)+13.0, vec2(39.3468, 11.135)) + t * 23.17) * 24634.6345);
    float h3 = fract(sin(dot(floor(p)+71.0, vec2(9.154, 27.983)) + t * 29.41) * 17431.3711);

    vec3 c;
    if (variation == 0) {
        // 独立通道哈希
        c = vec3(h1, h2, h3);
    } else if (variation == 1) {
        // 多尺度哈希混合
        vec2 p2 = p * 0.5; vec2 p3 = p * 2.7;
        float m1 = fract(sin(dot(floor(p2), vec2(15.7, 47.3)) + t * 13.3) * 31871.1);
        float m2 = fract(sin(dot(floor(p3), vec2(61.3, 21.9)) + t * 31.7) * 55147.3);
        c = vec3(mix(h1, m1, 0.5), mix(h2, m2, 0.5), mix(h3, h1, 0.5));
    } else if (variation == 2) {
        // 频谱混合（不公倍频率）
        float r = fract(sin(uv.x * 123.0 + uv.y * 173.0 + t * 2.17) * 43758.3);
        float g = fract(sin(uv.x * 231.0 + uv.y * 119.0 - t * 1.93) * 31871.7);
        float b = fract(sin(uv.x * 199.0 + uv.y * 157.0 + t * 2.71) * 27493.9);
        c = vec3(r,g,b);
    } else if (variation == 3) {
        // 蓝噪声滚动
        vec2 pp = p / 2.0 + vec2(t * 60.0, t * 47.0);
        float n1 = fract(sin(dot(floor(pp), vec2(12.9898, 78.233))) * 43758.5453);
        float n2 = fract(sin(dot(floor(pp + 23.0), vec2(39.3468, 11.135))) * 24634.6345);
        float v = clamp((n1 * 0.7 + n2 * 0.3), 0.0, 1.0);
        c = vec3(fract(v + 0.33), fract(v + 0.66), v);
    } else if (variation == 4) {
        // 径向扰动 + 相位扫频
        vec2 d = (uv - 0.5) * 2.0;
        float r = length(d);
        float a = atan(d.y, d.x);
        float v = fract(sin(r * 333.0 + a * 177.0 + t * 3.0) * 32768.0);
        c = vec3(v, fract(v + 0.37), fract(v + 0.73));
    } else if (variation == 5) {
        // 区域板动态（避免等灰）
        vec2 d = (uv - 0.5) * 2.0;
        float r2 = dot(d,d);
        float base = 0.5 + 0.5 * sin(90.0 * r2 + t * 1.8);
        c = vec3(base, fract(base + 0.31), fract(base + 0.62));
    } else if (variation == 6) {
        // 混合场（通道交错不同相位/尺度）
        float r = fract(sin(dot(p, vec2(0.251, 0.391)) + t * 2.3) * 51413.0);
        float g = fract(sin(dot(p, vec2(0.173, 0.613)) - t * 1.7) * 37199.0);
        float b = fract(sin(dot(p, vec2(0.421, 0.287)) + t * 3.1) * 29761.0);
        c = vec3(r,g,b);
    } else if (variation == 7) {
        // HSV 全色域覆盖（平滑，无抖动）
        float h = fract(uv.x + uv.y + t*0.05);
        float s = 0.9;
        float v = 0.9;
        c = hsv2rgb(vec3(h,s,v));
    } else if (variation == 8) {
        // 谱梯度混合（平滑，无抖动）
        float w = fract(uv.x*0.37 + uv.y*0.41 + t*0.10);
        vec3 a = vec3(1.0, 0.0, 0.5);
        vec3 b2 = vec3(0.0, 1.0, 1.0);
        c = mix(a, b2, w);
    } else if (variation == 9) {
        // Lissajous 色域轨迹（叠加噪声防止重复块）
        float r = sin(uv.x*157.0 + t*2.31) * sin(uv.y*133.0 - t*1.77) * 0.5 + 0.5;
        float g = sin(uv.x*141.0 - t*2.07) * sin(uv.y*149.0 + t*1.61) * 0.5 + 0.5;
        float b = sin(uv.x*163.0 + t*2.83) * sin(uv.y*127.0 - t*1.29) * 0.5 + 0.5;
        c = vec3(r,g,b);
    } else if (variation == 10) {
        // HSV 色轮（平滑）：角度->色相，半径->亮度
        vec2 d = (uv - 0.5) * 2.0;
        float h = fract((atan(d.y,d.x) / 6.2831853) + 1.0);
        float v2 = clamp(length(d), 0.0, 1.0);
        c = hsv2rgb(vec3(h, 0.9, 1.0 - v2*0.2));
    } else if (variation == 11) {
        // 时域色相扫动（平滑）：hue 随时间线性变化
        float h = fract(uv.x + t*0.05);
        c = hsv2rgb(vec3(h, 0.85, 0.95));
    } else if (variation == 12) {
        // 三正弦全色域（平滑）：相位错开 120 度
        float ph = t*0.35;
        float r = 0.5 + 0.5*sin(6.28318*(uv.x*0.23 + uv.y*0.31) + ph);
        float g = 0.5 + 0.5*sin(6.28318*(uv.x*0.29 + uv.y*0.17) + ph + 2.094);
        float b = 0.5 + 0.5*sin(6.28318*(uv.x*0.19 + uv.y*0.27) + ph + 4.188);
        c = vec3(r,g,b);
    } else if (variation == 13) {
        // 伪 YUV->RGB 扫动（平滑）：Y 固定、UV 扫动
        float Y = 0.7;
        float U = sin(uv.x*3.0 + t*0.4)*0.5;
        float V = sin(uv.y*3.0 - t*0.5)*0.5;
        float R = clamp(Y + 1.13983*V, 0.0, 1.0);
        float G = clamp(Y - 0.39465*U - 0.58060*V, 0.0, 1.0);
        float B = clamp(Y + 2.03211*U, 0.0, 1.0);
        c = vec3(R,G,B);
    } else {
        // 混合场（通道交错不同相位/尺度）
        float r = fract(sin(dot(p, vec2(0.251, 0.391)) + t * 2.3) * 51413.0);
        float g = fract(sin(dot(p, vec2(0.173, 0.613)) - t * 1.7) * 37199.0);
        float b = fract(sin(dot(p, vec2(0.421, 0.287)) + t * 3.1) * 29761.0);
        c = vec3(r,g,b);
    }

    // 10-bit 量化，降低压缩可预测性同时确保位深覆盖
    c = vec3(q10(c.r), q10(c.g), q10(c.b));
    return c;
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
    } else if (uCategory == 1) {
        // DYNAMIC_GROUP: 高熵带宽压力（避免重复色块，低可压缩性，10-bit 覆盖）
        int idx = clamp(uContentMode, 0, 13);
        color = generateComplexColor(uv, uTime, idx);
    } else {
        // AUX_GROUP: test‑ufo 对标
        color = ufoPattern(uv, uTime);
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
    language = detectLanguage();
}

MonitorTest::~MonitorTest() {
    cleanup();
}

bool MonitorTest::initialize() {
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        std::cerr << tr("初始化GLFW失败", "Failed to initialize GLFW") << std::endl;
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
    
    std::cout << tr("检测到显示器分辨率: ", "Detected resolution: ")
              << windowWidth << "x" << windowHeight << " @" << mode->refreshRate << "Hz" << std::endl;
    
    // 创建全屏窗口
    window = glfwCreateWindow(windowWidth, windowHeight, "Display Hardware Test", monitor, nullptr);
    
    if (!window) {
        std::cerr << tr("创建GLFW窗口失败", "Failed to create GLFW window") << std::endl;
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
        std::cerr << tr("初始化GLEW失败", "Failed to initialize GLEW") << std::endl;
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
        std::cerr << tr("文本渲染初始化失败（FreeType）", "Text renderer init failed (FreeType)") << std::endl;
    } else {
        std::string fontPath = chooseFontPath();
        if (fontPath.empty()) {
            std::cerr << tr("未找到可用字体，请安装常见 CJK 或西文字体。",
                           "No suitable system font found; please install common CJK or Western fonts.")
                      << std::endl;
        } else {
            int px = std::clamp(windowHeight / 90, 16, 40);
            if (!textRenderer->LoadFont(fontPath, px)) {
                std::cerr << tr("加载字体失败: ", "Failed to load font: ") << fontPath << std::endl;
            } else {
                std::cout << tr("已加载字体: ", "Loaded font: ") << fontPath << " (" << px << "px)" << std::endl;
            }
        }
    }
}

static std::string toSafeString(const GLubyte* s) {
    return s ? reinterpret_cast<const char*>(s) : std::string("Unknown");
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
    const float cr = 0.92f, cg = 0.94f, cb = 0.96f; // 正文颜色
    if (minimalOverlay) {
        std::ostringstream oss;
        oss << "FPS: " << static_cast<int>(currentFps);
        leftLines.push_back({oss.str(), 1.0f, 1.0f, 1.0f, false});
    } else {
        leftLines.push_back({tr("GPU 信息", "GPU Info"), 0.30f, 0.95f, 0.50f, false});
    std::string glVersion = std::string(tr("OpenGL 版本: ", "OpenGL: ")) + toSafeString(glGetString(GL_VERSION));
    std::string glVendor  = std::string(tr("显卡厂商: ", "Vendor: ")) + toSafeString(glGetString(GL_VENDOR));
    std::string glRend    = std::string(tr("显卡型号: ", "Renderer: ")) + toSafeString(glGetString(GL_RENDERER));
    std::string res       = std::string(tr("分辨率: ", "Resolution: ")) + std::to_string(windowWidth) + "x" + std::to_string(windowHeight);
    leftLines.push_back({glVersion, cr, cg, cb, false});
    leftLines.push_back({glVendor,  cr, cg, cb, false});
    leftLines.push_back({glRend,    cr, cg, cb, false});
    leftLines.push_back({res,       cr, cg, cb, true}); // 额外间距

    leftLines.push_back({tr("显示器信息", "Monitor"), 0.40f, 0.80f, 1.00f, false});
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    std::string refresh = mode ? (std::string(tr("刷新率: ", "Refresh: ")) + std::to_string(mode->refreshRate) + " Hz")
                               : std::string(tr("刷新率: 未知", "Refresh: Unknown"));
    leftLines.push_back({refresh, cr, cg, cb, true});

    leftLines.push_back({tr("实时测试信息", "Runtime"), 1.00f, 0.75f, 0.30f, false});
    std::string modeStr;
    switch (config.mode) {
        case TestMode::FIXED_FPS:       modeStr = tr("固定帧率", "Fixed FPS"); break;
        case TestMode::JITTER_FPS:      modeStr = tr("抖动模式", "Jitter FPS"); break;
        case TestMode::OSCILLATION_FPS: modeStr = tr("震荡模式", "Oscillation FPS"); break;
    }
    std::ostringstream oss;
    oss << "FPS: " << static_cast<int>(currentFps);
    float ratio = static_cast<float>(std::min(currentFps / 120.0, 1.0));
    leftLines.push_back({oss.str(), 1.0f - ratio, ratio, 0.2f, false});
    std::ostringstream ft;
    ft << std::fixed << std::setprecision(2)
       << (language == Language::ZH ? "帧时间: " : "Frame time: ")
       << frameTimeMs << " ms";
    if (!(useDynamicFrameRange && !config.vsyncEnabled)) {
        ft << (language == Language::ZH ? "  (目标: " : "  (Target: ")
           << (targetFrameTime * 1000.0) << " ms)";
    }
    leftLines.push_back({ft.str(), cr, cg, cb, false});
    std::string pacing;
    if (config.vsyncEnabled) {
        pacing = (language==Language::ZH?"帧率策略: 垂直同步":"Pacing: VSync");
    } else {
        pacing = useDynamicFrameRange ? (language==Language::ZH?"帧率策略: 动态范围":"Pacing: Range")
                                      : (language==Language::ZH?"帧率策略: 固定":"Pacing: Fixed");
    }
    leftLines.push_back({pacing, cr, cg, cb, false});
    std::string groupStr;
    if (config.category == Category::STATIC_GROUP) groupStr = tr("静态图样", "Static");
    else if (config.category == Category::DYNAMIC_GROUP) groupStr = tr("动态高熵", "High-Entropy");
    else groupStr = tr("辅助诊断", "Auxiliary");
    auto staticName = [&](int idx)->std::string {
        switch (idx) {
            case 0: return language==Language::ZH? "彩条" : "Color Bars";
            case 1: return language==Language::ZH? "灰阶渐变" : "Gray Gradient";
            case 2: return language==Language::ZH? "16阶灰条" : "16-step Gray";
            case 3: return language==Language::ZH? "细棋盘(1px)" : "Fine Checker (1px)";
            case 4: return language==Language::ZH? "粗棋盘" : "Coarse Checker";
            case 5: return language==Language::ZH? "网格32px" : "Grid 32px";
            case 6: return language==Language::ZH? "网格8px" : "Grid 8px";
            case 7: return language==Language::ZH? "RGB竖条" : "RGB Stripes";
            case 8: return language==Language::ZH? "十字+三分线" : "Cross + Thirds";
            case 9: return language==Language::ZH? "纯黑" : "Black";
            case 10: return language==Language::ZH? "纯白" : "White";
            case 11: return language==Language::ZH? "纯红" : "Red";
            case 12: return language==Language::ZH? "纯绿" : "Green";
            case 13: return language==Language::ZH? "纯蓝" : "Blue";
            case 14: return language==Language::ZH? "50%灰" : "50% Gray";
            case 15: return "Siemens Star";
            case 16: return language==Language::ZH? "水平楔形" : "Horizontal Wedge";
            case 17: return language==Language::ZH? "垂直楔形" : "Vertical Wedge";
            case 18: return language==Language::ZH? "同心圆环" : "Concentric Rings";
            case 19: return language==Language::ZH? "点栅格" : "Dot Grid";
            case 20: return "Gamma Checker";
        }
        return language==Language::ZH? "静态图样" : "Static";
    };
    auto dynamicName = [&](int idx)->std::string {
        switch (idx) {
            case 0: return language==Language::ZH? "高熵: 通道哈希" : "HE: Channel Hash";
            case 1: return language==Language::ZH? "高熵: 多尺度哈希" : "HE: Multi-Scale Hash";
            case 2: return language==Language::ZH? "高熵: 频谱混合" : "HE: Spectral Mix";
            case 3: return language==Language::ZH? "高熵: 蓝噪声滚动" : "HE: Blue-Noise Scroll";
            case 4: return language==Language::ZH? "高熵: 径向扰动" : "HE: Radial Turbulence";
            case 5: return language==Language::ZH? "高熵: 区域板动态" : "HE: Zoneplate Dynamic";
            case 6: return language==Language::ZH? "高熵: 混合场" : "HE: Mixed Field";
            case 7: return language==Language::ZH? "高熵: HSV 全色域" : "HE: HSV Full-Gamut";
            case 8: return language==Language::ZH? "高熵: 谱梯度混合" : "HE: Spectral Gradient";
            case 9: return language==Language::ZH? "高熵: Lissajous 色域" : "HE: Lissajous Field";
            case 10: return language==Language::ZH? "高熵: HSV 色轮" : "HE: HSV Wheel";
            case 11: return language==Language::ZH? "高熵: 色相扫动" : "HE: Hue Sweep";
            case 12: return language==Language::ZH? "高熵: 三正弦色域" : "HE: Tri-Sine Gamut";
            case 13: return language==Language::ZH? "高熵: YUV 扫动" : "HE: YUV Sweep";
        }
        return language==Language::ZH? "高熵" : "High-Entropy";
    };
    auto auxName = [&](int idx)->std::string {
        switch (idx) {
            case 0: return language==Language::ZH? "辅助: 移动亮条" : "Aux: Moving Bar";
            case 1: return language==Language::ZH? "辅助: UFO 运动" : "Aux: UFO Motion";
            case 2: return language==Language::ZH? "辅助: 1px 反相" : "Aux: 1px Temporal Flip";
            case 3: return language==Language::ZH? "辅助: Zone Plate" : "Aux: Zone Plate";
            case 4: return language==Language::ZH? "辅助: 位平面闪烁" : "Aux: Bit-Plane Flicker";
            case 5: return language==Language::ZH? "辅助: 彩色棋盘轮换" : "Aux: Color Checker Cycle";
            case 6: return language==Language::ZH? "辅助: 蓝噪声滚动" : "Aux: Blue-Noise Scroll";
            case 7: return language==Language::ZH? "辅助: 径向扫频" : "Aux: Radial Sweep";
            case 8: return language==Language::ZH? "辅助: 旋转楔形" : "Aux: Wedge Spin";
            case 9: return language==Language::ZH? "辅助: 粗棋盘" : "Aux: Checker Coarse";
        }
        return language==Language::ZH? "辅助" : "Aux";
    };
    std::string patStr;
    int patIdx = 0;
    char grp = 'S';
    if (config.category == Category::STATIC_GROUP) {
        patIdx = config.staticMode; grp = 'S'; patStr = staticName(config.staticMode);
    } else if (config.category == Category::DYNAMIC_GROUP) {
        patIdx = config.dynamicMode; grp = 'D'; patStr = dynamicName(config.dynamicMode);
    } else {
        patIdx = config.auxMode; grp = 'A'; patStr = auxName(config.auxMode);
    }
    leftLines.push_back({std::string(tr("模式组: ", "Group: ")) + groupStr, cr, cg, cb, false});
    {
        std::ostringstream lab; lab << tr("图样: ", "Pattern: ") << "[" << grp << ":" << patIdx << "] " << patStr;
        leftLines.push_back({lab.str(), cr, cg, cb, false});
    }
    // 垂直同步状态
    leftLines.push_back({std::string(tr("垂直同步: ", "VSync: ")) + onOff(config.vsyncEnabled), cr, cg, cb, false});
    leftLines.push_back({std::string(tr("目标帧率: ", "Target FPS: ")) + std::to_string(config.targetFps), cr, cg, cb, false});
    leftLines.push_back({std::string(tr("范围: ", "Range: ")) + std::to_string(config.minFps) + "~" + std::to_string(config.maxFps), cr, cg, cb, config.isPaused});
    if (config.isPaused) leftLines.push_back({tr("状态: 已暂停", "Status: Paused"), 1.0f, 0.2f, 0.2f, false});
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

    // 绘制左面板背景
    glViewport(static_cast<GLint>(margin), windowHeight - (panelH + static_cast<GLint>(topMargin)), panelW, panelH);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // 可选绘制右面板
    GLint rightX = 0, rightY = 0, rightW = 0, rightH = 0;
    if (!minimalOverlay) {
        // 右侧控制说明（两列对齐渲染）
        struct CtrlItem { std::string key; std::string desc; };
        std::vector<CtrlItem> items;
        items.push_back({"", tr("控制说明", "Controls")});
        items.push_back({"ESC", tr("退出程序", "Exit")});
#ifndef _WIN32
        items.push_back({"P", tr("暂停/继续", "Pause/Resume")});
#endif
        items.push_back({"SPACE", tr("切换模式组", "Toggle group")});
        items.push_back({"←/→", tr("上一/下一图样", "Prev/Next pattern")});
        items.push_back({"V", tr("垂直同步 开/关", "VSync On/Off")});
    items.push_back({"F1", tr("精简显示 开/关", "Minimal overlay On/Off")});
    items.push_back({"F2", tr("帧率策略 固定/动态范围", "Pacing Fixed/Range")});
    items.push_back({"F12", tr("一键极限模式", "Extreme mode toggle")});
    items.push_back({"F5/F6", tr("动态最小帧 -/+", "Range min -/+" )});
    items.push_back({"F7/F8", tr("动态最大帧 -/+", "Range max -/+" )});
        items.push_back({"L", "Toggle language (ZH/EN)"});

        float col1W = 0.0f; float col2W = 0.0f; float rightTotalH = 0.0f;
        for (const auto& it : items) {
            float w1 = textRenderer ? textRenderer->MeasureTextWidth(it.key, scale) : static_cast<float>(it.key.size() * 10);
            float w2 = textRenderer ? textRenderer->MeasureTextWidth(it.desc, scale) : static_cast<float>(it.desc.size() * 10);
            if (w1 > col1W) col1W = w1;
            if (w2 > col2W) col2W = w2;
            rightTotalH += lh;
        }
        float gap = 16.0f;
        float rightMaxW = col1W + (col1W>0?gap:0) + col2W;
        float rightContentH = asc + (static_cast<int>(items.size()) > 0 ? (static_cast<int>(items.size()) - 1) * lh : 0.0f) + desc;
        rightW = static_cast<GLint>(std::ceil(rightMaxW + padding * 2));
        rightH = static_cast<GLint>(std::ceil(rightContentH + padding * 2));
        rightX = windowWidth - (rightW + static_cast<GLint>(margin));
        rightY = windowHeight - (rightH + static_cast<GLint>(topMargin));

        glViewport(rightX, rightY, rightW, rightH);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // 恢复完整视口后绘制右侧文本（确保与左侧相同像素坐标映射与字号）
        glViewport(0, 0, windowWidth, windowHeight);
        glBindVertexArray(0);

        // 文本内容（右侧两列）
        if (textRenderer) {
            float rx = static_cast<float>(rightX) + padding;
            float ry = topMargin + padding + asc;
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& it = items[i];
                if (i == 0) {
                    textRenderer->RenderText(it.desc, rx, ry, scale, 1.0f, 0.90f, 0.40f);
                } else {
                    float x2 = rx;
                    if (!it.key.empty()) {
                        textRenderer->RenderText(it.key, rx, ry, scale, cr, cg, cb);
                        x2 = rx + col1W + gap;
                    }
                    textRenderer->RenderText(it.desc, x2, ry, scale, cr, cg, cb);
                }
                ry += lh;
            }
        }
    }

    // 恢复完整视口（左侧文本绘制所需；右侧已在绘制前重置）
    glViewport(0, 0, windowWidth, windowHeight);
    glBindVertexArray(0);

    // 文本内容（左侧）
    if (textRenderer) {
        float x = margin + padding;
        float y = topMargin + padding + asc; // 顶部保持 padding 到字形顶部
        for (const auto& ln : leftLines) {
            textRenderer->RenderText(ln.txt, x, y, scale, ln.r, ln.g, ln.b);
            y += lh;
            if (ln.extraGap) y += 8.0f;
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
    int cat = (config.category == Category::STATIC_GROUP) ? 0 : ((config.category == Category::DYNAMIC_GROUP) ? 1 : 2);
    int sub = (cat == 0) ? config.staticMode : ((cat==1)? config.dynamicMode : config.auxMode);
    shader->setInt("uCategory", cat);
    shader->setInt("uContentMode", sub);
    // 动态复杂内容的子变体（用于 generateComplexColor）
    int dynVar = (cat == 1) ? sub : 0;
    shader->setInt("uColorVariation", dynVar);
    // 无参数传递（已去掉可调参数）
    
    // 绘制全屏四边形
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // 渲染状态覆盖层（精简显示时减少绘制）
    renderStatusOverlay();
}

void MonitorTest::handleInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void MonitorTest::updateFrameRate() {
    if (!config.vsyncEnabled) {
        config.mode = useDynamicFrameRange ? TestMode::JITTER_FPS : TestMode::FIXED_FPS;
    } else {
        config.mode = TestMode::FIXED_FPS;
    }
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
            case TestMode::FIXED_FPS: modeStr = tr("固定帧率", "Fixed FPS"); break;
            case TestMode::JITTER_FPS: modeStr = tr("抖动模式", "Jitter FPS"); break;
            case TestMode::OSCILLATION_FPS: modeStr = tr("震荡模式", "Oscillation FPS"); break;
        }
        
        auto staticName = [&](int idx)->std::string {
            switch (idx) {
                case 0: return language==Language::ZH? "彩条" : "Color Bars";
                case 1: return language==Language::ZH? "灰阶渐变" : "Gray Gradient";
                case 2: return language==Language::ZH? "16阶灰条" : "16-step Gray";
                case 3: return language==Language::ZH? "细棋盘(1px)" : "Fine Checker (1px)";
                case 4: return language==Language::ZH? "粗棋盘" : "Coarse Checker";
                case 5: return language==Language::ZH? "网格32px" : "Grid 32px";
                case 6: return language==Language::ZH? "网格8px" : "Grid 8px";
                case 7: return language==Language::ZH? "RGB竖条" : "RGB Stripes";
                case 8: return language==Language::ZH? "十字+三分线" : "Cross + Thirds";
                case 9: return language==Language::ZH? "纯黑" : "Black";
                case 10: return language==Language::ZH? "纯白" : "White";
                case 11: return language==Language::ZH? "纯红" : "Red";
                case 12: return language==Language::ZH? "纯绿" : "Green";
                case 13: return language==Language::ZH? "纯蓝" : "Blue";
                case 14: return language==Language::ZH? "50%灰" : "50% Gray";
            }
            return language==Language::ZH? "静态图样" : "Static";
        };
        auto dynamicName = [&](int idx)->std::string {
            switch (idx) {
                case 0: return language==Language::ZH? "动态: 渐变+噪声" : "Dyn: Gradient+Noise";
                case 1: return language==Language::ZH? "动态: 高频噪声" : "Dyn: High-Freq Noise";
                case 2: return language==Language::ZH? "动态: 波浪干涉" : "Dyn: Wave Interference";
                case 3: return language==Language::ZH? "动态: 螺旋" : "Dyn: Spiral";
                case 4: return language==Language::ZH? "动态: 棋盘扰动" : "Dyn: Checker Disturb";
                case 5: return language==Language::ZH? "动态: 辐射彩虹" : "Dyn: Radial Rainbow";
                case 6: return language==Language::ZH? "动态: 斜向干涉" : "Dyn: Diagonal Interf";
                case 7: return language==Language::ZH? "动态: RGBW轮播" : "Dyn: RGBW Cycle";
                case 8: return language==Language::ZH? "动态: 移动亮条" : "Dyn: Moving Bar";
                case 9: return language==Language::ZH? "动态: UFO移动" : "Dyn: UFO Motion";
                case 10: return language==Language::ZH? "动态: 1px反相闪烁" : "Dyn: 1px Temporal Flip";
                case 11: return language==Language::ZH? "动态: Zone Plate" : "Dyn: Zone Plate";
                case 12: return language==Language::ZH? "动态: 位平面闪烁" : "Dyn: Bit-Plane Flicker";
                case 13: return language==Language::ZH? "动态: 彩色棋盘轮换" : "Dyn: Color Checker Cycle";
                case 14: return language==Language::ZH? "动态: 蓝噪声滚动" : "Dyn: Blue-Noise Scroll";
                case 15: return language==Language::ZH? "动态: 径向相位扫频" : "Dyn: Radial Phase Sweep";
                case 16: return language==Language::ZH? "动态: 旋转楔形线" : "Dyn: Wedge Spin";
            }
            return language==Language::ZH? "动态图样" : "Dynamic";
        };
        std::string groupStr = (config.category == Category::STATIC_GROUP) ? tr("静态图样", "Static") : tr("动态压力", "Dynamic");
        std::string patStr = (config.category == Category::STATIC_GROUP)
            ? staticName(config.staticMode)
            : dynamicName(config.dynamicMode);
        if (language == Language::ZH) {
            std::cout << "当前帧率: " << static_cast<int>(currentFps) << " FPS | "
                      << "帧时间: " << std::fixed << std::setprecision(2) << frameTimeMs << " ms | "
                      << "帧率模式: " << modeStr << " | "
                      << "模式组: " << groupStr << " | "
                      << "图样: " << patStr << " | "
                      << (config.isPaused ? "已暂停" : "运行中") << std::endl;
        } else {
            std::cout << "FPS: " << static_cast<int>(currentFps) << " | "
                      << "Frame: " << std::fixed << std::setprecision(2) << frameTimeMs << " ms | "
                      << "Mode: " << modeStr << " | "
                      << "Group: " << groupStr << " | "
                      << "Pattern: " << patStr << " | "
                      << (config.isPaused ? "Paused" : "Running") << std::endl;
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
#ifndef _WIN32
            case GLFW_KEY_P:
                test->config.isPaused = !test->config.isPaused;
                std::cout << (test->language==Language::ZH ? (test->config.isPaused ? "测试已暂停" : "测试已恢复")
                                                                 : (test->config.isPaused ? "Paused" : "Resumed"))
                          << std::endl;
                break;
#endif

            case GLFW_KEY_SPACE: {
                // 轮换模式组（静态 -> 动态 -> 辅助 -> 静态）
                if (test->config.category == Category::STATIC_GROUP) test->config.category = Category::DYNAMIC_GROUP;
                else if (test->config.category == Category::DYNAMIC_GROUP) test->config.category = Category::AUX_GROUP;
                else test->config.category = Category::STATIC_GROUP;
                std::cout << (test->language==Language::ZH ? "模式组: " : "Group: ")
                          << (test->config.category == Category::STATIC_GROUP ? (test->language==Language::ZH?"静态图样":"Static")
                              : (test->config.category == Category::DYNAMIC_GROUP ? (test->language==Language::ZH?"动态高熵":"High-Entropy")
                              : (test->language==Language::ZH?"辅助诊断":"Auxiliary")))
                          << std::endl;
                break;
            }

            case GLFW_KEY_RIGHT: {
                if (test->config.category == Category::STATIC_GROUP) {
                    test->config.staticMode = (test->config.staticMode + 1) % 21;
                    std::cout << (test->language==Language::ZH?"静态图样索引: ":"Static index: ") << test->config.staticMode << std::endl;
                } else if (test->config.category == Category::DYNAMIC_GROUP) {
                    test->config.dynamicMode = (test->config.dynamicMode + 1) % 14;
                    std::cout << (test->language==Language::ZH?"动态图样索引: ":"Dynamic index: ") << test->config.dynamicMode << std::endl;
                } else {
                    test->config.auxMode = 0; // 仅 UFO
                    std::cout << (test->language==Language::ZH?"辅助图样索引: ":"Aux index: ") << test->config.auxMode << std::endl;
                }
                break;
            }

            case GLFW_KEY_LEFT: {
                if (test->config.category == Category::STATIC_GROUP) {
                    test->config.staticMode = (test->config.staticMode + 21 - 1) % 21;
                    std::cout << (test->language==Language::ZH?"静态图样索引: ":"Static index: ") << test->config.staticMode << std::endl;
                } else if (test->config.category == Category::DYNAMIC_GROUP) {
                    test->config.dynamicMode = (test->config.dynamicMode + 14 - 1) % 14;
                    std::cout << (test->language==Language::ZH?"动态图样索引: ":"Dynamic index: ") << test->config.dynamicMode << std::endl;
                } else {
                    test->config.auxMode = 0; // 仅 UFO
                    std::cout << (test->language==Language::ZH?"辅助图样索引: ":"Aux index: ") << test->config.auxMode << std::endl;
                }
                break;
            }


            case GLFW_KEY_V: {
                test->config.vsyncEnabled = !test->config.vsyncEnabled;
                glfwMakeContextCurrent(window);
                glfwSwapInterval(test->config.vsyncEnabled ? 1 : 0);
                std::cout << (test->language==Language::ZH ? (test->config.vsyncEnabled ? "垂直同步: 开" : "垂直同步: 关")
                                                           : (test->config.vsyncEnabled ? "VSync: On" : "VSync: Off"))
                          << std::endl;
                break;
            }

            case GLFW_KEY_L: {
                test->toggleLanguage();
                std::cout << (test->language==Language::EN?"Language: English":"Language: Chinese") << std::endl;
                break;
            }

            case GLFW_KEY_F1: {
                test->minimalOverlay = !test->minimalOverlay;
                break;
            }
            case GLFW_KEY_F2: {
                test->useDynamicFrameRange = !test->useDynamicFrameRange;
                break;
            }
            case GLFW_KEY_F12: {
                test->extremeMode = !test->extremeMode;
                if (test->extremeMode) {
                    test->config.vsyncEnabled = false; glfwSwapInterval(0);
                    test->minimalOverlay = true;
                    test->useDynamicFrameRange = true;
                    test->config.category = Category::DYNAMIC_GROUP;
                    test->config.dynamicMode = 1; // 多尺度哈希
                    test->config.minFps = 30; test->config.maxFps = 240;
                }
                std::cout << (test->extremeMode?"Extreme: ON":"Extreme: OFF") << std::endl;
                break;
            }

            case GLFW_KEY_F5: {
                if (test->config.minFps > 10) test->config.minFps -= 1;
                if (test->config.minFps >= test->config.maxFps) test->config.minFps = test->config.maxFps - 1;
                break;
            }
            case GLFW_KEY_F6: {
                if (test->config.minFps < test->config.maxFps-1) test->config.minFps += 1;
                break;
            }
            case GLFW_KEY_F7: {
                if (test->config.maxFps > test->config.minFps+1) test->config.maxFps -= 1;
                break;
            }
            case GLFW_KEY_F8: {
                if (test->config.maxFps < 360) test->config.maxFps += 1;
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
    if (test->textRenderer) {
        test->textRenderer->SetScreenSize(width, height);
        std::string fontPath = test->chooseFontPath();
        if (!fontPath.empty()) {
            int px = std::clamp(height / 90, 16, 40);
            test->textRenderer->LoadFont(fontPath, px);
        }
    }
}

void MonitorTest::errorCallback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
}

 

void MonitorTest::printControls() const {
    std::cout << (language==Language::ZH?"\n=== 控制说明 ===":"\n=== Controls ===") << std::endl;
    std::cout << (language==Language::ZH?"ESC    - 退出程序":"ESC    - Exit") << std::endl;
    #ifndef _WIN32
    std::cout << (language==Language::ZH?"P      - 暂停/继续":"P      - Pause/Resume") << std::endl;
    #endif
    std::cout << (language==Language::ZH?"SPACE  - 切换模式组(静态/动态)":"SPACE  - Toggle group (static/dynamic)") << std::endl;
    std::cout << (language==Language::ZH?"←/→     - 上一/下一图样":"←/→     - Prev/Next pattern") << std::endl;
    std::cout << (language==Language::ZH?"V      - 垂直同步 开/关":"V      - VSync On/Off") << std::endl;
    std::cout << "F1     - " << (language==Language::ZH?"精简显示 开/关":"Minimal overlay On/Off") << std::endl;
    std::cout << "F2     - " << (language==Language::ZH?"帧率策略 固定/动态":"Pacing Fixed/Range") << std::endl;
    std::cout << "F3     - " << (language==Language::ZH?"动态策略 抖动/震荡":"Range Jitter/Osc") << std::endl;
    std::cout << "F12    - " << (language==Language::ZH?"一键极限模式":"Extreme mode toggle") << std::endl;
    std::cout << "L      - Toggle language (ZH/EN)" << std::endl;
    std::cout << "===============\n" << std::endl;
}

void MonitorTest::printSystemInfo() const {
    std::cout << (language==Language::ZH?"\n=== 系统信息 ===":"\n=== System Info ===") << std::endl;
    std::cout << (language==Language::ZH?"OpenGL版本: ":"OpenGL: ") << toSafeString(glGetString(GL_VERSION)) << std::endl;
    std::cout << (language==Language::ZH?"显卡厂商: ":"Vendor: ") << toSafeString(glGetString(GL_VENDOR)) << std::endl;
    std::cout << (language==Language::ZH?"显卡型号: ":"Renderer: ") << toSafeString(glGetString(GL_RENDERER)) << std::endl;
    std::cout << (language==Language::ZH?"分辨率: ":"Resolution: ") << windowWidth << "x" << windowHeight << std::endl;
    std::cout << (language==Language::ZH?"目标: 10bit色深全带宽压力测试":"Goal: 10-bit deep color bandwidth stress") << std::endl;
    std::cout << "================\n" << std::endl;
}

const char* MonitorTest::tr(const char* zh, const char* en) const {
    return language == Language::ZH ? zh : en;
}

std::string MonitorTest::onOff(bool v) const {
    return language == Language::ZH ? (v ? "开" : "关") : (v ? "On" : "Off");
}

void MonitorTest::toggleLanguage() {
    language = (language == Language::ZH) ? Language::EN : Language::ZH;
}

Language MonitorTest::detectLanguage() {
    const char* env = std::getenv("DISPLAY_HW_LANG");
    if (env) {
        std::string v(env);
        for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (v.find("en") != std::string::npos) return Language::EN;
        if (v.find("zh") != std::string::npos || v.find("cn") != std::string::npos) return Language::ZH;
    }
    const char* sys = std::getenv("LANG");
    if (sys) {
        std::string v(sys);
        for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (v.find("zh") != std::string::npos || v.find("cn") != std::string::npos) return Language::ZH;
    }
    return Language::EN;
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
