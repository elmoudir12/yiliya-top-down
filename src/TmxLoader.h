#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct TmxObject {
    float x = 0, y = 0, width = 0, height = 0;
    std::string name;
    std::string type;
    std::unordered_map<std::string, std::string> properties;
};

struct TmxTileset {
    int firstGid = 1;
    std::string imagePath;
    int tileWidth = 32;
    int tileHeight = 32;
    int tileCount = 0;
    int columns = 0;
    bool valid = false;
};

struct TmxMapData {
    int width = 0;
    int height = 0;
    int tileSize = 32;
    std::vector<TmxTileset> tilesets;
    std::vector<int> groundTiles;
    std::vector<int> wallTiles;
    std::vector<TmxObject> transitions;
    std::vector<TmxObject> trees;
    std::vector<TmxObject> fenceRects;
    TmxObject spawn;
};

bool loadTmx(const std::string& filepath, TmxMapData& out);
