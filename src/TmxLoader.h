#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct TmxMapData {
    int width = 0;
    int height = 0;
    int tileSize = 32;
    std::vector<int> groundTiles;
    std::vector<int> collisionTiles;
};

bool loadTmx(const std::string& filepath, TmxMapData& out);
