#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct CollisionRect {
    float x, y, w, h;
};

struct TmxMapData {
    int width = 0;
    int height = 0;
    int tileSize = 32;
    std::vector<int> groundTiles;
    std::vector<int> collisionTiles;
    std::vector<std::vector<int>> tileLayers;
    std::vector<CollisionRect> collisionRects;
};

bool loadTmx(const std::string& filepath, TmxMapData& out);
