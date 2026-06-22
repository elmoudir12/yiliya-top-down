#include "Font.h"
#include <vector>
#include <cstdio>
#include <cstdlib>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace {
    stbtt_fontinfo g_font;
    std::vector<unsigned char> g_fontBuf;
    bool g_loaded = false;
}

bool Font::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_fontBuf.resize(fsz);
    if (fread(g_fontBuf.data(), 1, fsz, f) != (size_t)fsz) { fclose(f); return false; }
    fclose(f);

    if (!stbtt_InitFont(&g_font, g_fontBuf.data(), 0)) return false;
    g_loaded = true;
    return true;
}

void Font::shutdown() {
    g_fontBuf.clear();
    g_loaded = false;
}

bool Font::renderText(uint8_t* pixels, int pw, int ph,
                       const char* text, int x, int y,
                       int pixelHeight,
                       uint8_t r, uint8_t g, uint8_t b) {
    if (!g_loaded) return false;

    float scale = stbtt_ScaleForPixelHeight(&g_font, (float)pixelHeight);
    float posX = (float)x;
    float posY = (float)y;
    bool anyDrawn = false;

    while (*text) {
        unsigned char ch = (unsigned char)*text;
        if (ch == ' ') {
            int adv;
            stbtt_GetCodepointHMetrics(&g_font, ' ', &adv, nullptr);
            posX += adv * scale;
        } else if (ch >= 32) {
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&g_font, ch, &adv, &lsb);
            int cw, chh, xOff, yOff;
            unsigned char* bm = stbtt_GetCodepointBitmap(&g_font, scale, scale, ch, &cw, &chh, &xOff, &yOff);
            if (bm) {
                int bx0 = (int)(posX + lsb * scale + xOff);
                int by0 = (int)(posY + yOff);
                for (int by = 0; by < chh; ++by) {
                    for (int bx = 0; bx < cw; ++bx) {
                        int px = bx0 + bx, py = by0 + by;
                        if (px >= 0 && px < pw && py >= 0 && py < ph) {
                            int a = bm[by * cw + bx];
                            if (a > 0) {
                                float f = a / 255.0f;
                                int i = (py * pw + px) * 4;
                                pixels[i+0] = (uint8_t)(r * f);
                                pixels[i+1] = (uint8_t)(g * f);
                                pixels[i+2] = (uint8_t)(b * f);
                                pixels[i+3] = (uint8_t)(255 * f);
                                anyDrawn = true;
                            }
                        }
                    }
                }
                free(bm);
                posX += adv * scale;
            }
        }
        ++text;
    }
    return anyDrawn;
}

int Font::textWidth(const char* text, int pixelHeight) {
    if (!g_loaded) return 0;
    float scale = stbtt_ScaleForPixelHeight(&g_font, (float)pixelHeight);
    float w = 0;
    for (; *text; ++text) {
        int adv;
        stbtt_GetCodepointHMetrics(&g_font, (unsigned char)*text, &adv, nullptr);
        w += adv * scale;
    }
    return (int)w;
}

int Font::ascent(int pixelHeight) {
    if (!g_loaded) return 0;
    int a, d;
    stbtt_GetFontVMetrics(&g_font, &a, &d, nullptr);
    return (int)(a * stbtt_ScaleForPixelHeight(&g_font, (float)pixelHeight));
}

int Font::descent(int pixelHeight) {
    if (!g_loaded) return 0;
    int a, d;
    stbtt_GetFontVMetrics(&g_font, &a, &d, nullptr);
    return (int)(d * stbtt_ScaleForPixelHeight(&g_font, (float)pixelHeight));
}

int Font::textHeight(int pixelHeight) {
    return ascent(pixelHeight) - descent(pixelHeight);
}