#include "text_renderer.h"
#include <vector>
#include <stdexcept>
#include <cstring>

namespace {
const char* kTextVertexShader = R"(#version 330 core
layout (location = 0) in vec4 aPosUV; // xy = pos(px), zw = uv

uniform vec2 uScreenSize;

out vec2 vUV;

void main() {
    vec2 pos = aPosUV.xy;
    // 像素坐标转NDC，y向下为正 -> 需要翻转
    vec2 ndc = (pos / uScreenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = vec2(aPosUV.z, 1.0 - aPosUV.w);
}
)";

const char* kTextFragmentShader = R"(#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uText;
uniform vec3 uTextColor;

void main() {
    float alpha = texture(uText, vUV).r;
    FragColor = vec4(uTextColor, alpha);
}
)";
}

static std::u32string Utf8ToUtf32Impl(const std::string& s) {
    std::u32string out;
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out.push_back(c);
            ++i;
        } else if ((c >> 5) == 0x6 && i + 1 < n) {
            char32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i+1]) & 0x3F);
            out.push_back(cp);
            i += 2;
        } else if ((c >> 4) == 0xE && i + 2 < n) {
            char32_t cp = ((c & 0x0F) << 12)
                        | ((static_cast<unsigned char>(s[i+1]) & 0x3F) << 6)
                        | (static_cast<unsigned char>(s[i+2]) & 0x3F);
            out.push_back(cp);
            i += 3;
        } else if ((c >> 3) == 0x1E && i + 3 < n) {
            char32_t cp = ((c & 0x07) << 18)
                        | ((static_cast<unsigned char>(s[i+1]) & 0x3F) << 12)
                        | ((static_cast<unsigned char>(s[i+2]) & 0x3F) << 6)
                        | (static_cast<unsigned char>(s[i+3]) & 0x3F);
            out.push_back(cp);
            i += 4;
        } else {
            out.push_back(0xFFFD);
            ++i;
        }
    }
    return out;
}

TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer() {
    for (auto& kv : glyphCache_) {
        if (kv.second.textureId) {
            glDeleteTextures(1, &kv.second.textureId);
        }
    }
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (face_) { FT_Done_Face(face_); face_ = nullptr; }
    if (ft_)   { FT_Done_FreeType(ft_); ft_ = nullptr; }
}

bool TextRenderer::Init(int screenWidth, int screenHeight) {
    screenW_ = screenWidth;
    screenH_ = screenHeight;

    if (FT_Init_FreeType(&ft_) != 0) {
        std::cerr << "FreeType init failed" << std::endl;
        return false;
    }
    ftReady_ = true;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    shader_ = std::make_unique<Shader>(kTextVertexShader, kTextFragmentShader);
    return true;
}

bool TextRenderer::LoadFont(const std::string& fontPath, int pixelHeight) {
    if (!ftReady_) return false;

    if (face_) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
    if (FT_New_Face(ft_, fontPath.c_str(), 0, &face_) != 0) {
        std::cerr << "加载字体失败: " << fontPath << std::endl;
        return false;
    }
    FT_Set_Pixel_Sizes(face_, 0, pixelHeight);
    fontPixelHeight_ = pixelHeight;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glyphCache_.clear();
    return true;
}

void TextRenderer::RenderText(const std::string& utf8Text, float x, float y, float scale,
                              float r, float g, float b) {
    if (!face_) return;

    shader_->use();
    shader_->setVec2("uScreenSize", static_cast<float>(screenW_), static_cast<float>(screenH_));
    shader_->setVec3("uTextColor", r, g, b);

    glActiveTexture(GL_TEXTURE0);
    GLint loc = glGetUniformLocation(shader_->getProgram(), "uText");
    if (loc != -1) glUniform1i(loc, 0);

    glBindVertexArray(vao_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const std::u32string text32 = Utf8ToUtf32(utf8Text);
    float penX = x;
    float baseY = y;

    for (char32_t cp : text32) {
        if (!EnsureGlyphCached(cp)) continue;
        const Character& ch = glyphCache_[cp];

        float xpos = penX + static_cast<float>(ch.bearingX) * scale;
        float ypos = baseY - static_cast<float>(ch.bearingY) * scale;

        float w = static_cast<float>(ch.sizeX) * scale;
        float h = static_cast<float>(ch.sizeY) * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h, 0.0f, 0.0f },
            { xpos,     ypos,     0.0f, 1.0f },
            { xpos + w, ypos,     1.0f, 1.0f },

            { xpos,     ypos + h, 0.0f, 0.0f },
            { xpos + w, ypos,     1.0f, 1.0f },
            { xpos + w, ypos + h, 1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureId);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        penX += static_cast<float>(ch.advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

void TextRenderer::SetScreenSize(int screenWidth, int screenHeight) {
    screenW_ = screenWidth;
    screenH_ = screenHeight;
}

std::u32string TextRenderer::Utf8ToUtf32(const std::string& utf8) {
    return Utf8ToUtf32Impl(utf8);
}

float TextRenderer::GetLineHeightPx(float scale) const {
    if (face_ && face_->size) {
        // FreeType metrics are in 26.6 fixed point (1/64th pixel)
        const long h26_6 = face_->size->metrics.height;
        if (h26_6 > 0) {
            return static_cast<float>(h26_6 / 64.0) * scale;
        }
    }
    return static_cast<float>(fontPixelHeight_ > 0 ? fontPixelHeight_ * 1.2 : 24.0 * 1.2) * scale;
}

float TextRenderer::MeasureTextWidth(const std::string& utf8Text, float scale) {
    const std::u32string text32 = Utf8ToUtf32(utf8Text);
    float widthPx = 0.0f;
    if (!face_) {
        // Rough estimate without font: 0.6em per character
        widthPx = static_cast<float>(text32.size()) * (fontPixelHeight_ > 0 ? fontPixelHeight_ * 0.6f : 12.0f);
        return widthPx * scale;
    }
    for (char32_t cp : text32) {
        if (!EnsureGlyphCached(cp)) {
            widthPx += (fontPixelHeight_ > 0 ? fontPixelHeight_ * 0.5f : 10.0f);
            continue;
        }
        const Character& ch = glyphCache_[cp];
        widthPx += static_cast<float>(ch.advance >> 6);
    }
    return widthPx * scale;
}

float TextRenderer::GetAscenderPx(float scale) const {
    if (face_ && face_->size) {
        const long a26_6 = face_->size->metrics.ascender;
        return static_cast<float>(a26_6 / 64.0) * scale;
    }
    return static_cast<float>(fontPixelHeight_ > 0 ? fontPixelHeight_ * 0.8 : 19.0) * scale;
}

float TextRenderer::GetDescenderPx(float scale) const {
    if (face_ && face_->size) {
        const long d26_6 = face_->size->metrics.descender;
        float d = static_cast<float>(std::abs(d26_6) / 64.0);
        return d * scale;
    }
    return static_cast<float>(fontPixelHeight_ > 0 ? fontPixelHeight_ * 0.2 : 5.0) * scale;
}

bool TextRenderer::EnsureGlyphCached(char32_t codepoint) {
    if (glyphCache_.find(codepoint) != glyphCache_.end()) return true;
    if (!face_) return false;

    FT_UInt glyph_index = FT_Get_Char_Index(face_, codepoint);
    if (FT_Load_Char(face_, codepoint, FT_LOAD_RENDER)) {
        if (codepoint != U'\u25A1' && EnsureGlyphCached(U'\u25A1')) {
            glyphCache_[codepoint] = glyphCache_[U'\u25A1'];
            return true;
        }
        return false;
    }

    FT_GlyphSlot g = face_->glyph;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 g->bitmap.width, g->bitmap.rows,
                 0, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Character ch;
    ch.textureId = tex;
    ch.sizeX = g->bitmap.width;
    ch.sizeY = g->bitmap.rows;
    ch.bearingX = g->bitmap_left;
    ch.bearingY = g->bitmap_top;
    ch.advance = g->advance.x;

    glyphCache_[codepoint] = ch;

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}
