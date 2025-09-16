#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include <GL/glew.h>
#include "shader.h"

/* FreeType */
#if __has_include(<ft2build.h>)
  #include <ft2build.h>
#elif __has_include(<freetype2/ft2build.h>)
  #include <freetype2/ft2build.h>
#else
  #error "ft2build.h not found. Please install FreeType development headers (pkg: freetype2)."
#endif
#include FT_FREETYPE_H

// 简单文本渲染器：使用FreeType按需缓存字形纹理，支持UTF-8（含中文）
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    // 初始化屏幕尺寸（像素坐标系，左上角为(0,0)）
    bool Init(int screenWidth, int screenHeight);

    // 加载字体文件（TrueType/OTF/TTC），像素高度建议20~48
    bool LoadFont(const std::string& fontPath, int pixelHeight);

    // 渲染UTF-8字符串（包含中文）
    // x,y 为屏幕像素坐标，y 为基线到屏幕顶部的距离（与常见2D UI一致）
    void RenderText(const std::string& utf8Text, float x, float y, float scale,
                    float r, float g, float b);

    // 更新屏幕尺寸（当窗口大小变化时调用）
    void SetScreenSize(int screenWidth, int screenHeight);

    // 计算文本度量（像素）
    float MeasureTextWidth(const std::string& utf8Text, float scale = 1.0f);
    float GetLineHeightPx(float scale = 1.0f) const;
    float GetAscenderPx(float scale = 1.0f) const;
    float GetDescenderPx(float scale = 1.0f) const; // 正值

private:
    struct Character {
        GLuint textureId = 0;   // GL_RED 单通道纹理
        int    sizeX = 0;       // 位图宽
        int    sizeY = 0;       // 位图高
        int    bearingX = 0;    // 左侧到字形起笔点的距离
        int    bearingY = 0;    // 基线到字形顶部的距离
        long   advance = 0;     // 水平前进（1/64像素）
    };

    // UTF-8 转 Unicode 码点
    static std::u32string Utf8ToUtf32(const std::string& utf8);

    bool EnsureGlyphCached(char32_t codepoint);

    // 资源
    FT_Library ft_ = nullptr;
    FT_Face    face_ = nullptr;
    bool       ftReady_ = false;
    int        fontPixelHeight_ = 0;

    std::unordered_map<char32_t, Character> glyphCache_;

    // GL 资源
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::unique_ptr<Shader> shader_;

    // 屏幕信息
    int screenW_ = 0;
    int screenH_ = 0;

    // 禁止拷贝
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
};
