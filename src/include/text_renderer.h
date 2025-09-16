#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <GL/glew.h>
#include "shader.h"

#if __has_include(<ft2build.h>)
  #include <ft2build.h>
#elif __has_include(<freetype2/ft2build.h>)
  #include <freetype2/ft2build.h>
#else
  #error "ft2build.h not found. Please install FreeType development headers (pkg: freetype2)."
#endif
#include FT_FREETYPE_H

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    bool Init(int screenWidth, int screenHeight);
    bool LoadFont(const std::string& fontPath, int pixelHeight);
    void RenderText(const std::string& utf8Text, float x, float y, float scale,
                    float r, float g, float b);
    void SetScreenSize(int screenWidth, int screenHeight);
    float MeasureTextWidth(const std::string& utf8Text, float scale = 1.0f);
    float GetLineHeightPx(float scale = 1.0f) const;
    float GetAscenderPx(float scale = 1.0f) const;
    float GetDescenderPx(float scale = 1.0f) const;
private:
    struct Character {
        GLuint textureId = 0;
        int sizeX = 0;
        int sizeY = 0;
        int bearingX = 0;
        int bearingY = 0;
        long advance = 0;
    };
    static std::u32string Utf8ToUtf32(const std::string& utf8);
    bool EnsureGlyphCached(char32_t codepoint);
    FT_Library ft_ = nullptr;
    FT_Face face_ = nullptr;
    bool ftReady_ = false;
    int fontPixelHeight_ = 0;
    std::unordered_map<char32_t, Character> glyphCache_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::unique_ptr<Shader> shader_;
    int screenW_ = 0;
    int screenH_ = 0;
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
};
