#pragma once

#include <cstdint>
#include <string>

struct stbtt_fontinfo;

class Font {
public:
    static bool load(const std::string& path);
    static void shutdown();

    // Render text at the given pixel height. pixels must be pw*ph*4 bytes.
    // (x, y) is the top-left of the text box.
    // Returns true if any pixels were drawn.
    static bool renderText(uint8_t* pixels, int pw, int ph,
                           const char* text, int x, int y,
                           int pixelHeight,
                           uint8_t r, uint8_t g, uint8_t b);

    // Width of text at the given pixel height
    static int textWidth(const char* text, int pixelHeight);

    // Vertical distance from the baseline to the top of the highest pixel.
    // The texture should be sized at least textHeight() tall.
    static int ascent(int pixelHeight);
    static int descent(int pixelHeight);
    static int textHeight(int pixelHeight);
};