#include "Map.h"
#include "Engine.h"
#include "Renderer.h"
#include "Texture.h"
#include "TmxLoader.h"

#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "stb_truetype.h"

static Texture* createWoodFloorTexture(Engine* engine) {
    const int S = 32;
    std::vector<uint8_t> p(S * S * 4, 255);
    auto clamp8 = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
    auto wr = [](int v) { return v & 31; };
    auto hash = [](int x, int y) -> int {
        unsigned h = (unsigned)(x * 374761393 + y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (int)((h ^ (h >> 16)) & 0xFF);
    };
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            // Plank seam at top and bottom edges for seamless vertical tiling
            bool seam = (y < 2 || y >= 30);
            if (seam) {
                p[(y * S + x) * 4 + 0] = 55;
                p[(y * S + x) * 4 + 1] = 35;
                p[(y * S + x) * 4 + 2] = 12;
                p[(y * S + x) * 4 + 3] = 255;
                continue;
            }
            // Wood grain from hash (wraps at 32 so seamless)
            int g = (hash(x, y) % 11) - 5;
            int streak = hash(x, wr(y & ~3)) % 9 - 4;
            int r = 175 + g * 3 + streak * 4;
            int g_ = 112 + g * 2 + streak * 3;
            int b = 58 + g + streak * 2;
            p[(y * S + x) * 4 + 0] = clamp8(r);
            p[(y * S + x) * 4 + 1] = clamp8(g_);
            p[(y * S + x) * 4 + 2] = clamp8(b);
            p[(y * S + x) * 4 + 3] = 255;
        }
    }
    return new Texture(engine, p.data(), S, S,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

static Texture* createGrassTexture(Engine* engine) {
    const int S = 32;
    std::vector<uint8_t> p(S * S * 4, 255);
    auto clamp8 = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
    auto hash = [](int x, int y) -> int {
        unsigned h = (unsigned)(x * 374761393 + y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (int)((h ^ (h >> 16)) & 0xFF);
    };
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            int h = hash(x, y);
            // Base green with variation
            int r = 50 + (h % 20);
            int g = 130 + (h % 35);
            int b = 35 + (h % 15);
            // Occasional darker/lighter patches
            if ((hash(x/2, y/2) % 5) == 0) { r += 10; g += 25; b += 8; }
            if ((hash(x*3, y) % 7) == 0) { r -= 8; g -= 20; b -= 5; }
            // Tiny flower dots
            if ((hash(x+5, y+7) % 15) == 0) { r = 200; g = 80; b = 120; }
            if ((hash(x+3, y+11) % 20) == 0) { r = 220; g = 200; b = 60; }
            p[(y * S + x) * 4 + 0] = clamp8(r);
            p[(y * S + x) * 4 + 1] = clamp8(g);
            p[(y * S + x) * 4 + 2] = clamp8(b);
            p[(y * S + x) * 4 + 3] = 255;
        }
    }
    return new Texture(engine, p.data(), S, S,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

static Texture* createTreeTexture(Engine* engine) {
    Texture* tex = new Texture(engine, "assets/pine trees.png");
    tex->setFilter(VK_FILTER_NEAREST, VK_FILTER_NEAREST);
    return tex;
}

static Texture* createTimberTexture(Engine* engine) {
    const int W = 128, H = 128;
    std::vector<uint8_t> p(W * H * 4, 255);

    auto px = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        int i = (y * W + x) * 4;
        p[i+0] = r; p[i+1] = g; p[i+2] = b; p[i+3] = 255;
    };

    // Colors
    uint8_t DW[3] = {60, 35, 8};     // dark wood (beams)
    uint8_t LW[3] = {90, 58, 28};    // light wood (wainscot panels)
    uint8_t PL[3] = {225, 205, 175}; // plaster

    // Fill plaster
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            px(x, y, PL[0], PL[1], PL[2]);

    // Top rail: y = 0..7
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < W; ++x) px(x, y, DW[0], DW[1], DW[2]);

    // Mid rail: y = 56..63
    for (int y = 56; y < 64; ++y)
        for (int x = 0; x < W; ++x) px(x, y, DW[0], DW[1], DW[2]);

    // Bottom rail: y = 120..127
    for (int y = 120; y < 128; ++y)
        for (int x = 0; x < W; ++x) px(x, y, DW[0], DW[1], DW[2]);

    // Vertical studs (full height)
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 6; ++x) px(x, y, DW[0], DW[1], DW[2]);
        for (int x = 40; x < 46; ++x) px(x, y, DW[0], DW[1], DW[2]);
        for (int x = 80; x < 86; ++x) px(x, y, DW[0], DW[1], DW[2]);
        for (int x = 122; x < 128; ++x) px(x, y, DW[0], DW[1], DW[2]);
    }

    // Diagonal braces in plaster section (y = 8..55)
    for (int i = 0; i < 42; ++i) {
        int bx = 8 + i, by = 10 + i;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                if (by+dy >= 8 && by+dy < 56 && bx+dx >= 6 && bx+dx < 80)
                    px(bx+dx, by+dy, DW[0], DW[1], DW[2]);
    }
    for (int i = 0; i < 42; ++i) {
        int bx = 84 + i, by = 50 - i;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                if (by+dy >= 8 && by+dy < 56 && bx+dx >= 80 && bx+dx < 122)
                    px(bx+dx, by+dy, DW[0], DW[1], DW[2]);
    }

    // Wainscoting area (y = 64..119) with raised panels
    for (int y = 64; y < 120; ++y) {
        for (int x = 0; x < W; ++x) {
            bool divider = (x % 20) < 4;
            if (divider) {
                px(x, y, LW[0], LW[1], LW[2]);
            } else {
                int grain = ((x * 5 + y * 7) % 7) - 3;
                px(x, y, LW[0] + 10 + grain, LW[1] + 8 + grain, LW[2] + 5 + grain);
            }
        }
    }

    return new Texture(engine, p.data(), W, H,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

struct MapMeta {
    int width, height;
    int worldX = 0, worldY = 0;
    struct MiniTransition { int tileX, tileY, tileW, tileH; std::string targetMap; };
    std::vector<MiniTransition> transitions;
};

static const std::unordered_map<std::string, MapMeta>& getMapMeta() {
    static const std::unordered_map<std::string, MapMeta> meta = {
        {"player_house", {14, 11, 0, 0, {{4, 11, 2, 1, "front_yard"}}}},
        {"front_yard",   {27, 20, 0, 13, {{10, -1, 1, 1, "player_house"}}}},
    };
    return meta;
}

Map::Map(Engine* engine, Renderer* renderer)
    : m_engine(engine), m_renderer(renderer) {
}

Map::~Map() {
    unload();
}

void Map::load(const std::string& mapName) {
    m_mapName = mapName;

    TmxMapData tmx;
    if (!loadTmx("maps/" + mapName + ".tmx", tmx))
        throw std::runtime_error("Failed to load TMX map: " + mapName);

    m_width = tmx.width;
    m_height = tmx.height;
    m_tileSize = tmx.tileSize;
    m_curWorldX = 0;
    m_curWorldY = 0;

    // Determine floor type from ground tiles
    bool isGrass = false;
    for (int t : tmx.groundTiles) {
        if (t == 0) { isGrass = false; break; } // wood (first tile of local tileset)
        if (t > 0) { isGrass = true; break; }  // grass (including external tilesets)
    }

    // Build collision grid from wall tiles and ground tiles
    m_collisionGrid.resize(m_width * m_height);
    for (int i = 0; i < m_width * m_height; ++i) {
        m_collisionGrid[i] = 0;
        if (!tmx.wallTiles.empty() && tmx.wallTiles[i] > 0)
            m_collisionGrid[i] = 1;
        if (tmx.groundTiles[i] == -1)
            m_collisionGrid[i] = 1;
    }

    // Parse transitions from TMX objects
    m_transitions.clear();
    for (auto& obj : tmx.transitions) {
        // TMX coords are pixel-based, origin top-left
        // Convert to tile coords
        int tileX = (int)(obj.x / m_tileSize);
        int tileY = (int)(obj.y / m_tileSize);
        int tileW = (int)(obj.width / m_tileSize);
        int tileH = (int)(obj.height / m_tileSize);
        if (tileW < 1) tileW = 1;
        if (tileH < 1) tileH = 1;
        std::string targetMap = "player_house";
        int spawnX = 5, spawnY = 5;
        auto it = obj.properties.find("targetMap");
        if (it != obj.properties.end()) targetMap = it->second;
        it = obj.properties.find("spawnX");
        if (it != obj.properties.end()) spawnX = std::stoi(it->second);
        it = obj.properties.find("spawnY");
        if (it != obj.properties.end()) spawnY = std::stoi(it->second);
        m_transitions.push_back({tileX, tileY, tileW, tileH, targetMap, spawnX, spawnY});
    }

    // Spawn
    m_spawnTileX = (int)(tmx.spawn.x / m_tileSize);
    m_spawnTileY = (int)(tmx.spawn.y / m_tileSize);
    if (m_spawnTileX == 0 && m_spawnTileY == 0) { m_spawnTileX = 4; m_spawnTileY = 4; }

    // Load tileset textures from TMX
    for (auto& ts : m_tilesetTextures) {
        if (ts.texture) delete ts.texture;
    }
    m_tilesetTextures.clear();

    for (auto& tm : tmx.tilesets) {
        if (!tm.valid || tm.imagePath.empty()) continue;
        TilesetSlot slot;
        slot.firstGid = tm.firstGid;
        slot.tileCount = tm.tileCount;
        slot.columns = tm.columns;
        // Check if file exists before loading
        std::ifstream f(tm.imagePath);
        if (f.good()) {
            f.close();
            slot.texture = new Texture(m_engine, tm.imagePath);
            // Fix columns if not set from TSX but we have texture size
            if (slot.columns == 0 && slot.texture) {
                glm::vec2 sz = slot.texture->size();
                if (sz.x > 0) slot.columns = (int)(sz.x / m_tileSize);
            }
        }
        m_tilesetTextures.push_back(slot);
    }

    if (!m_wallTexture)
        m_wallTexture = createTimberTexture(m_engine);
    if (!m_floorTexture)
        m_floorTexture = isGrass ? createGrassTexture(m_engine) : createWoodFloorTexture(m_engine);

    // Door gaps from transitions
    m_doorGaps.clear();
    {
        float hw2 = m_width * m_tileSize * 0.5f;
        float hh2 = m_height * m_tileSize * 0.5f;
        for (auto& t : m_transitions) {
            float x0 = t.tileX * m_tileSize - hw2;
            float x1 = (t.tileX + t.tileW) * m_tileSize - hw2;
            float z0 = t.tileY * m_tileSize - hh2;
            float z1 = (t.tileY + t.tileH) * m_tileSize - hh2;
            float pad = (t.tileY < 0) ? 16.0f : 0.0f;
            if (t.tileY + t.tileH >= m_height)
                m_doorGaps.push_back({1, x0, x1});
            if (t.tileY <= 0)
                m_doorGaps.push_back({0, x0 - pad, x1 + pad});
            if (t.tileX + t.tileW >= m_width)
                m_doorGaps.push_back({3, z0, z1});
            if (t.tileX <= 0)
                m_doorGaps.push_back({2, z0, z1});
        }
    }

    // Use tiled floor if tileset textures are available
    bool hasTiles = false;
    for (auto& ts : m_tilesetTextures) { if (ts.texture) { hasTiles = true; break; } }
    if (hasTiles) {
        buildTiledFloor(tmx.groundTiles);
    }
    buildFloorTop();
    buildFloorBottom();
    if (m_mapName == "player_house") {
        buildWalls();
    }
    if (!tmx.fenceRects.empty())
        buildBoundaryFence();

    generateMapTexture();

    // Build trees from TMX objects
    m_trees.clear();
    if (m_treeTexture) { delete m_treeTexture; m_treeTexture = nullptr; }
    m_treeCollisionStart = -1;
    if (!tmx.trees.empty()) {
        m_treeTexture = createTreeTexture(m_engine);
        float hw3 = m_width * m_tileSize * 0.5f;
        float hh3 = m_height * m_tileSize * 0.5f;
        m_treeCollisionStart = (int)m_collisionRects.size();
        for (auto& obj : tmx.trees) {
            // TMX coords: pixel-based, origin top-left
            // Convert to world coords: x = pixelX - hw, z = pixelY - hh
            float wx = obj.x - hw3;
            float wz = obj.y - hh3;
            m_trees.push_back({wx, wz, 1.0f});
            m_collisionRects.push_back({obj.x - 16, obj.y - 16, 32, 32});
        }
    }

    // Static decoration billboard
    if (m_decorationTexture) { delete m_decorationTexture; m_decorationTexture = nullptr; }
    if (mapName == "front_yard") {
        m_decorationTexture = new Texture(m_engine, "assets/front house of the player.png");
        float hw3 = m_width * m_tileSize * 0.5f;
        float hh3 = m_height * m_tileSize * 0.5f;
        m_decorationPos = glm::vec3(
            10 * m_tileSize + m_tileSize * 0.5f - hw3,
            0.0f,
            2 * m_tileSize + m_tileSize * 0.5f - hh3 - 80.0f
        );
        m_decorationScale = 6.0f;
    }
    m_selectedBillboard = -1;

    loadBillboards("billboards.txt");
}

int Map::billboardCount() const {
    int count = 0;
    if (m_decorationTexture) ++count;
    count += (int)m_trees.size();
    return count;
}

glm::vec3 Map::billboardPosition(int index) const {
    int decoOffset = (m_decorationTexture ? 1 : 0);
    if (m_decorationTexture && index == 0)
        return m_decorationPos;
    int treeIdx = index - decoOffset;
    if (treeIdx >= 0 && treeIdx < (int)m_trees.size())
        return glm::vec3(m_trees[treeIdx].x, 0.0f, m_trees[treeIdx].z);
    return glm::vec3(0.0f);
}

void Map::setBillboardPosition(int index, const glm::vec3& pos) {
    int decoOffset = (m_decorationTexture ? 1 : 0);
    if (m_decorationTexture && index == 0) {
        m_decorationPos = pos;
        return;
    }
    int treeIdx = index - decoOffset;
    if (treeIdx >= 0 && treeIdx < (int)m_trees.size()) {
        m_trees[treeIdx].x = pos.x;
        m_trees[treeIdx].z = pos.z;
        if (m_treeCollisionStart >= 0) {
            int ci = m_treeCollisionStart + treeIdx;
            if (ci < (int)m_collisionRects.size()) {
                float hw = m_width * m_tileSize * 0.5f;
                float hh = m_height * m_tileSize * 0.5f;
                m_collisionRects[ci] = {pos.x + hw - 16, pos.z + hh - 16, 32, 32};
            }
        }
    }
}

std::string Map::billboardName(int index) const {
    int decoOffset = (m_decorationTexture ? 1 : 0);
    if (m_decorationTexture && index == 0) return "front house";
    int treeIdx = index - decoOffset;
    if (treeIdx >= 0 && treeIdx < (int)m_trees.size())
        return "tree " + std::to_string(treeIdx);
    return "?";
}

static std::string xmlAttr(const std::string& xml, size_t pos, const std::string& attr) {
    std::string search = attr + "=\"";
    size_t p = xml.find(search, pos);
    if (p == std::string::npos) return {};
    p += search.size();
    size_t end = xml.find('"', p);
    if (end == std::string::npos) return {};
    return xml.substr(p, end - p);
}

void Map::saveBillboards(const std::string& path) const {
    int n = billboardCount();
    // Save decoration position (index 0) to billboards.txt. Trees go to TMX.
    if (n > 0) {
        FILE* f = fopen(path.c_str(), "w");
        if (f) {
            glm::vec3 p = billboardPosition(0);
            fprintf(f, "%d %f %f %f  # %s\n", 0, p.x, p.y, p.z, billboardName(0).c_str());
            fclose(f);
        }
    }

    // Also save tree positions back to the TMX file
    std::string tmxPath = "maps/" + m_mapName + ".tmx";
    std::ifstream tmxFile(tmxPath);
    if (!tmxFile) return;
    std::string data((std::istreambuf_iterator<char>(tmxFile)),
                      std::istreambuf_iterator<char>());

    size_t ogStart = 0;
    int treeWriteIdx = 0;
    int decoOffset = (m_decorationTexture ? 1 : 0);
    while ((ogStart = data.find("<objectgroup", ogStart)) != std::string::npos) {
        std::string ogName = xmlAttr(data, ogStart, "name");
        size_t ogEnd = data.find("</objectgroup>", ogStart);
        if (ogEnd == std::string::npos) break;

        if (ogName == "trees") {
            size_t objPos = ogStart;
            while ((objPos = data.find("<object ", objPos)) != std::string::npos && objPos < ogEnd) {
                size_t tagEnd = data.find("/>", objPos);
                if (tagEnd == std::string::npos || tagEnd >= ogEnd) break;

                int billIdx = decoOffset + treeWriteIdx;
                if (billIdx < n) {
                    glm::vec3 pos = billboardPosition(billIdx);
                    float hw3 = m_width * m_tileSize * 0.5f;
                    float hh3 = m_height * m_tileSize * 0.5f;
                    int pixelX = (int)(pos.x + hw3);
                    int pixelY = (int)(pos.z + hh3);

                    auto replaceAttr = [&](const std::string& attr, int val) {
                        size_t aPos = data.find(" " + attr + "=\"", objPos);
                        if (aPos == std::string::npos || aPos >= tagEnd) return;
                        size_t vStart = aPos + attr.size() + 3;
                        size_t vEnd = data.find('"', vStart);
                        if (vEnd == std::string::npos || vEnd >= tagEnd) return;
                        std::string newVal = std::to_string(val);
                        int diff = (int)newVal.size() - (int)(vEnd - vStart);
                        data.replace(vStart, vEnd - vStart, newVal);
                        tagEnd = (size_t)((int)tagEnd + diff);
                        ogEnd = (size_t)((int)ogEnd + diff);
                    };
                    replaceAttr("x", pixelX);
                    replaceAttr("y", pixelY);
                    ++treeWriteIdx;
                }
                objPos = tagEnd + 2;
            }
            break;
        }
        ogStart = ogEnd + 14;
    }

    if (treeWriteIdx > 0) {
        std::ofstream outFile(tmxPath);
        outFile.write(data.data(), data.size());
    }
}

void Map::loadBillboards(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;
    int idx; float x, y, z;
    while (fscanf(f, "%d %f %f %f", &idx, &x, &y, &z) == 4) {
        // Only restore decoration (index 0). Trees come from TMX.
        if (idx == 0 && idx < billboardCount())
            setBillboardPosition(idx, glm::vec3(x, y, z));
        int ch;
        while ((ch = fgetc(f)) != EOF && ch != '\n');
    }
    fclose(f);
}

void Map::unload() {
    destroyMesh(m_wallMesh);
    destroyMesh(m_floorTopMesh);
    destroyMesh(m_floorBottomMesh);
    for (auto& mesh : m_tileFloorMeshes)
        destroyMesh(mesh);
    m_tileFloorMeshes.clear();
    if (m_floorTexture) {
        delete m_floorTexture;
        m_floorTexture = nullptr;
    }
    if (m_wallTexture) {
        delete m_wallTexture;
        m_wallTexture = nullptr;
    }
    destroyMesh(m_dirtPathMesh);
    if (m_dirtTexture) {
        delete m_dirtTexture;
        m_dirtTexture = nullptr;
    }
    if (m_treeTexture) {
        delete m_treeTexture;
        m_treeTexture = nullptr;
    }
    if (m_decorationTexture) {
        delete m_decorationTexture;
        m_decorationTexture = nullptr;
    }
    if (m_mapOverlayTexture) {
        delete m_mapOverlayTexture;
        m_mapOverlayTexture = nullptr;
    }
    for (auto& ts : m_tilesetTextures) {
        if (ts.texture) delete ts.texture;
    }
    m_tilesetTextures.clear();
    for (auto& lb : m_textLabels) {
        if (lb.texture) delete lb.texture;
    }
    m_textLabels.clear();
    m_collisionGrid.clear();
    m_trees.clear();
    m_collisionRects.clear();
    m_wallCollisionRects.clear();
    m_doorGaps.clear();
    m_transitions.clear();
}

void Map::destroyMesh(LayerMesh& mesh) {
    VkDevice dev = m_engine->device();
    if (mesh.indexBuffer) {
        vkDestroyBuffer(dev, mesh.indexBuffer, nullptr);
        mesh.indexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.indexBufferMemory) {
        vkFreeMemory(dev, mesh.indexBufferMemory, nullptr);
        mesh.indexBufferMemory = VK_NULL_HANDLE;
    }
    if (mesh.vertexBuffer) {
        vkDestroyBuffer(dev, mesh.vertexBuffer, nullptr);
        mesh.vertexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.vertexBufferMemory) {
        vkFreeMemory(dev, mesh.vertexBufferMemory, nullptr);
        mesh.vertexBufferMemory = VK_NULL_HANDLE;
    }
    mesh.indexCount = 0;
}

void Map::buildFloorTop() {
    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;
    QuadVertex verts[4] = {
        {{-hw, 0.0f, -hh}, {0.0f,       0.0f}},
        {{ hw, 0.0f, -hh}, {m_width,    0.0f}},
        {{ hw, 0.0f,  hh}, {m_width,    m_height}},
        {{-hw, 0.0f,  hh}, {0.0f,       m_height}},
    };
    uint16_t idxs[6] = {0, 1, 2, 2, 3, 0};
    VkDeviceSize vsize = sizeof(verts);
    VkDeviceSize isize = sizeof(idxs);
    m_engine->createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_floorTopMesh.vertexBuffer, m_floorTopMesh.vertexBufferMemory);
    void* data;
    vkMapMemory(m_engine->device(), m_floorTopMesh.vertexBufferMemory, 0, vsize, 0, &data);
    memcpy(data, verts, vsize);
    vkUnmapMemory(m_engine->device(), m_floorTopMesh.vertexBufferMemory);
    m_engine->createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_floorTopMesh.indexBuffer, m_floorTopMesh.indexBufferMemory);
    vkMapMemory(m_engine->device(), m_floorTopMesh.indexBufferMemory, 0, isize, 0, &data);
    memcpy(data, idxs, isize);
    vkUnmapMemory(m_engine->device(), m_floorTopMesh.indexBufferMemory);
    m_floorTopMesh.indexCount = 6;
}

void Map::buildFloorBottom() {
    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;
    QuadVertex verts[4] = {
        {{-hw, -1.0f, -hh}, {0.0f,       0.0f}},
        {{ hw, -1.0f, -hh}, {m_width,    0.0f}},
        {{ hw, -1.0f,  hh}, {m_width,    m_height}},
        {{-hw, -1.0f,  hh}, {0.0f,       m_height}},
    };
    uint16_t idxs[6] = {0, 1, 2, 2, 3, 0};
    VkDeviceSize vsize = sizeof(verts);
    VkDeviceSize isize = sizeof(idxs);
    m_engine->createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_floorBottomMesh.vertexBuffer, m_floorBottomMesh.vertexBufferMemory);
    void* data;
    vkMapMemory(m_engine->device(), m_floorBottomMesh.vertexBufferMemory, 0, vsize, 0, &data);
    memcpy(data, verts, vsize);
    vkUnmapMemory(m_engine->device(), m_floorBottomMesh.vertexBufferMemory);
    m_engine->createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_floorBottomMesh.indexBuffer, m_floorBottomMesh.indexBufferMemory);
    vkMapMemory(m_engine->device(), m_floorBottomMesh.indexBufferMemory, 0, isize, 0, &data);
    memcpy(data, idxs, isize);
    vkUnmapMemory(m_engine->device(), m_floorBottomMesh.indexBufferMemory);
    m_floorBottomMesh.indexCount = 6;
}

void Map::buildTiledFloor(const std::vector<int>& groundTiles) {
    if (groundTiles.empty() || m_tilesetTextures.empty()) return;

    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;
    float ts = (float)m_tileSize;

    // Count tiles per tileset to determine which ones are used
    std::vector<bool> used(m_tilesetTextures.size(), false);
    int usedCount = 0;
    for (int g : groundTiles) {
        if (g < 0) continue;
        for (int ti = 0; ti < (int)m_tilesetTextures.size(); ++ti) {
            auto& slot = m_tilesetTextures[ti];
            if (!slot.texture) continue;
            if (g >= slot.firstGid && g < slot.firstGid + slot.tileCount) {
                if (!used[ti]) { used[ti] = true; ++usedCount; }
                break;
            }
        }
    }
    if (usedCount == 0) return;

    // Build separate mesh per tileset
    m_tileFloorMeshes.resize(m_tilesetTextures.size());
    for (int ti = 0; ti < (int)m_tilesetTextures.size(); ++ti) {
        if (!used[ti]) continue;
        auto& slot = m_tilesetTextures[ti];
        glm::vec2 tsize = slot.texture->size();
        int tcols = slot.columns > 0 ? slot.columns : (int)(tsize.x / m_tileSize);
        float invTexW = 1.0f / tsize.x;
        float invTexH = 1.0f / tsize.y;

        std::vector<QuadVertex> verts;
        std::vector<uint16_t> idxs;
        verts.reserve(groundTiles.size() * 4 / usedCount);
        idxs.reserve(groundTiles.size() * 6 / usedCount);

        for (int i = 0; i < (int)groundTiles.size(); ++i) {
            int gid = groundTiles[i];
            if (gid < 0) continue;
            if (!(gid >= slot.firstGid && gid < slot.firstGid + slot.tileCount)) continue;

            int tx = i % m_width;
            int ty = i / m_width;
            float wx = tx * ts - hw;
            float wz = ty * ts - hh;

            int localIdx = gid - slot.firstGid;
            int col = localIdx % tcols;
            int row = localIdx / tcols;
            float u0 = (float)col * m_tileSize * invTexW;
            float v0 = (float)row * m_tileSize * invTexH;
            float u1 = (float)(col + 1) * m_tileSize * invTexW;
            float v1 = (float)(row + 1) * m_tileSize * invTexH;

            uint32_t base = (uint32_t)verts.size();
            verts.push_back({{wx,      0.0f, wz     }, {u0, v0}});
            verts.push_back({{wx + ts, 0.0f, wz     }, {u1, v0}});
            verts.push_back({{wx + ts, 0.0f, wz + ts}, {u1, v1}});
            verts.push_back({{wx,      0.0f, wz + ts}, {u0, v1}});
            idxs.push_back(base + 0); idxs.push_back(base + 1); idxs.push_back(base + 2);
            idxs.push_back(base + 2); idxs.push_back(base + 3); idxs.push_back(base + 0);
        }

        if (verts.empty()) continue;

        auto& mesh = m_tileFloorMeshes[ti];
        VkDeviceSize vsize = verts.size() * sizeof(QuadVertex);
        VkDeviceSize isize = idxs.size() * sizeof(uint16_t);
        m_engine->createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mesh.vertexBuffer, mesh.vertexBufferMemory);
        void* data;
        vkMapMemory(m_engine->device(), mesh.vertexBufferMemory, 0, vsize, 0, &data);
        memcpy(data, verts.data(), vsize);
        vkUnmapMemory(m_engine->device(), mesh.vertexBufferMemory);
        m_engine->createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mesh.indexBuffer, mesh.indexBufferMemory);
        vkMapMemory(m_engine->device(), mesh.indexBufferMemory, 0, isize, 0, &data);
        memcpy(data, idxs.data(), isize);
        vkUnmapMemory(m_engine->device(), mesh.indexBufferMemory);
        mesh.indexCount = (uint32_t)idxs.size();
    }
    m_useTileMesh = true;
}

void Map::buildWalls() {
    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;
    float wh = WALL_HEIGHT;
    float t = WALL_THICK;

    std::vector<QuadVertex> verts;
    std::vector<uint16_t> idxs;

    auto addWallQuad = [&](const glm::vec3& a, const glm::vec3& b,
                            const glm::vec3& c, const glm::vec3& d,
                            float vFloor = 1.0f, float vCeil = 0.0f) {
        float horiz = glm::distance(a, b);
        float uEnd = horiz / 64.0f;
        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back({a, {0.0f, vFloor}});
        verts.push_back({b, {uEnd, vFloor}});
        verts.push_back({c, {uEnd, vCeil}});
        verts.push_back({d, {0.0f, vCeil}});
        idxs.push_back(base+0); idxs.push_back(base+1); idxs.push_back(base+2);
        idxs.push_back(base+2); idxs.push_back(base+3); idxs.push_back(base+0);
    };

    // Build a solid wall box from (x0,z0)-(x1,z1) spanning y=[0,wh]
    auto addWallBox = [&](float x0, float x1, float z0, float z1) {
        addWallQuad({x1,0,z0}, {x1,0,z1}, {x1,wh,z1}, {x1,wh,z0});
        addWallQuad({x0,0,z1}, {x0,0,z0}, {x0,wh,z0}, {x0,wh,z1});
        addWallQuad({x1,0,z1}, {x0,0,z1}, {x0,wh,z1}, {x1,wh,z1});
        addWallQuad({x0,0,z0}, {x1,0,z0}, {x1,wh,z0}, {x0,wh,z0});
        addWallQuad({x0,wh,z1}, {x1,wh,z1}, {x1,wh,z0}, {x0,wh,z0}, 0.5f, 0.9375f);
        addWallQuad({x0,0,z0}, {x1,0,z0}, {x1,0,z1}, {x0,0,z1}, 0.5f, 0.9375f);
        m_wallCollisionRects.push_back({x0+hw, z0+hh, x1-x0, z1-z0});
        m_collisionRects.push_back(m_wallCollisionRects.back());
    };

    // Build a box at arbitrary y range (for door frame etc.)
    auto addBoxY = [&](float x0, float x1, float z0, float z1, float y0, float y1) {
        addWallQuad({x1,y0,z0}, {x1,y0,z1}, {x1,y1,z1}, {x1,y1,z0});
        addWallQuad({x0,y0,z1}, {x0,y0,z0}, {x0,y1,z0}, {x0,y1,z1});
        addWallQuad({x1,y0,z1}, {x0,y0,z1}, {x0,y1,z1}, {x1,y1,z1});
        addWallQuad({x0,y0,z0}, {x1,y0,z0}, {x1,y1,z0}, {x0,y1,z0});
        addWallQuad({x0,y1,z1}, {x1,y1,z1}, {x1,y1,z0}, {x0,y1,z0}, 0.5f, 0.9375f);
        addWallQuad({x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1}, {x0,y0,z1}, 0.5f, 0.9375f);
    };

    m_wallCollisionRects.clear();

    // Collect gaps by side
    struct Gap { float from, to; };
    std::vector<Gap> wallGaps[4];
    for (auto& dg : m_doorGaps) {
        if (dg.side >= 0 && dg.side < 4)
            wallGaps[dg.side].push_back({dg.gapMin, dg.gapMax});
    }
    for (int i = 0; i < 4; ++i)
        std::sort(wallGaps[i].begin(), wallGaps[i].end(),
            [](auto& a, auto& b) { return a.from < b.from; });

    // Default wall definitions: for N/S (side 0/1) a0..a1 = X, b0..b1 = Z
    // For E/W (side 2/3) a0..a1 = Z, b0..b1 = X (swapped)
    struct { float a0, a1, b0, b1; int side; } base[4] = {
        {-hw - t, hw + t, -hh - t, -hh, 0}, // north
        {-hw - t, hw + t,  hh,      hh + t, 1}, // south
        {-hh - t, hh + t, -hw - t, -hw, 2}, // west (a=Z, b=X)
        {-hh - t, hh + t,  hw,      hw + t, 3}, // east (a=Z, b=X)
    };

    for (auto& w : base) {
        auto& gv = wallGaps[w.side];
        if (gv.empty()) {
            if (w.side <= 1)
                addWallBox(w.a0, w.a1, w.b0, w.b1);
            else
                addWallBox(w.b0, w.b1, w.a0, w.a1);
            continue;
        }
        float cur = w.a0;
        for (auto& gap : gv) {
            float cut0 = std::max(cur, gap.from);
            float cut1 = std::min(w.a1, gap.to);
            if (cut0 > cur + 0.1f) {
                if (w.side <= 1)
                    addWallBox(cur, cut0, w.b0, w.b1);
                else
                    addWallBox(w.b0, w.b1, cur, cut0);
            }
            cur = std::max(cur, cut1);
        }
        if (cur < w.a1 - 0.1f) {
            if (w.side <= 1)
                addWallBox(cur, w.a1, w.b0, w.b1);
            else
                addWallBox(w.b0, w.b1, cur, w.a1);
        }
    }

    // Door frame
    const float pw = 4.0f;
    const float bh = 8.0f;
    for (auto& dg : m_doorGaps) {
        if (dg.side == 0) { // north wall gap
            float x0 = dg.gapMin, x1 = dg.gapMax;
            addBoxY(x0 - pw, x0, -hh - t, -hh, 0, wh);
            addBoxY(x1, x1 + pw, -hh - t, -hh, 0, wh);
            addBoxY(x0 - pw, x1 + pw, -hh - t, -hh, wh - bh, wh);
        }
        if (dg.side == 2) { // west wall gap (z range)
            float z0 = dg.gapMin, z1 = dg.gapMax;
            addBoxY(-hw - t, -hw, z0 - pw, z0, 0, wh);
            addBoxY(-hw - t, -hw, z1, z1 + pw, 0, wh);
            addBoxY(-hw - t, -hw, z0 - pw, z1 + pw, wh - bh, wh);
        }
        if (dg.side == 3) { // east wall gap (z range)
            float z0 = dg.gapMin, z1 = dg.gapMax;
            addBoxY(hw, hw + t, z0 - pw, z0, 0, wh);
            addBoxY(hw, hw + t, z1, z1 + pw, 0, wh);
            addBoxY(hw, hw + t, z0 - pw, z1 + pw, wh - bh, wh);
        }
    }

    m_wallMesh.indexCount = static_cast<uint32_t>(idxs.size());
    if (m_wallMesh.indexCount == 0) return;

    m_wallMesh.indexCount = static_cast<uint32_t>(idxs.size());
    if (m_wallMesh.indexCount == 0) return;

    VkDeviceSize vsize = sizeof(QuadVertex) * verts.size();
    VkDeviceSize isize = sizeof(uint16_t) * idxs.size();

    m_engine->createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_wallMesh.vertexBuffer, m_wallMesh.vertexBufferMemory);
    void* data;
    vkMapMemory(m_engine->device(), m_wallMesh.vertexBufferMemory, 0, vsize, 0, &data);
    memcpy(data, verts.data(), vsize);
    vkUnmapMemory(m_engine->device(), m_wallMesh.vertexBufferMemory);

    m_engine->createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_wallMesh.indexBuffer, m_wallMesh.indexBufferMemory);
    vkMapMemory(m_engine->device(), m_wallMesh.indexBufferMemory, 0, isize, 0, &data);
    memcpy(data, idxs.data(), isize);
    vkUnmapMemory(m_engine->device(), m_wallMesh.indexBufferMemory);
}

void Map::buildBoundaryFence() {
    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;
    const float t = 8.0f;
    // North fence with door gaps
    {
        float cur = 0.0f;
        for (auto& dg : m_doorGaps) {
            if (dg.side != 0) continue;
            float g0 = dg.gapMin + hw, g1 = dg.gapMax + hw;
            if (cur < g0) m_collisionRects.push_back({cur, 0.0f, g0 - cur, t});
            cur = g1;
        }
        if (cur < 2 * hw) m_collisionRects.push_back({cur, 0.0f, 2 * hw - cur, t});
    }
    // South fence with door gaps
    {
        float cur = 0.0f;
        for (auto& dg : m_doorGaps) {
            if (dg.side != 1) continue;
            float g0 = dg.gapMin + hw, g1 = dg.gapMax + hw;
            if (cur < g0) m_collisionRects.push_back({cur, 2 * hh - t, g0 - cur, t});
            cur = g1;
        }
        if (cur < 2 * hw) m_collisionRects.push_back({cur, 2 * hh - t, 2 * hw - cur, t});
    }
    m_collisionRects.push_back({0.0f, 0.0f, t, 2 * hh}); // west
    m_collisionRects.push_back({2 * hw - t, 0.0f, t, 2 * hh}); // east
}

static std::string fmtRoomName(const std::string& raw) {
    std::string out;
    bool cap = true;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '_') { out += ' '; cap = true; }
        else if (cap) { out += (char)toupper(raw[i]); cap = false; }
        else { out += raw[i]; }
    }
    // "Player S House" → "Player's House"
    size_t s = out.find(" S ");
    if (s != std::string::npos) out.replace(s, 3, "'s ");
    return out;
}

static void drawText(uint8_t* pixels, int pw, int ph, stbtt_fontinfo* font,
                     const char* text, int x, int y, float size,
                     uint8_t r, uint8_t g, uint8_t b) {
    float scale = stbtt_ScaleForPixelHeight(font, size);
    float posX = (float)x;
    float posY = (float)y;

    while (*text) {
        unsigned char ch = (unsigned char)*text;
        if (ch == ' ') {
            int adv;
            stbtt_GetCodepointHMetrics(font, ' ', &adv, nullptr);
            posX += adv * scale;
        } else if (ch >= 32) {
            int adv, lsb;
            stbtt_GetCodepointHMetrics(font, ch, &adv, &lsb);
            int cw, chh, xOff, yOff;
            unsigned char* bm = stbtt_GetCodepointBitmap(font, scale, scale, ch, &cw, &chh, &xOff, &yOff);
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
                                pixels[i+0] = (uint8_t)(r * f + pixels[i+0] * (1.0f - f));
                                pixels[i+1] = (uint8_t)(g * f + pixels[i+1] * (1.0f - f));
                                pixels[i+2] = (uint8_t)(b * f + pixels[i+2] * (1.0f - f));
                                pixels[i+3] = (uint8_t)(255 * f + pixels[i+3] * (1.0f - f));
                            }
                        }
                    }
                }
                free(bm);
                posX += adv * scale;
            }
        } else {
            ++text;
            continue;
        }
        ++text;
    }
}

void Map::generateMapTexture() {
    const int PIX_PER_TILE = mapPixPerTile();
    const auto& allMaps = getMapMeta();

    // Compute global bounding box of all rooms
    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool first = true;
    for (auto& [name, meta] : allMaps) {
        if (first) { minX = meta.worldX; minY = meta.worldY; maxX = meta.worldX + meta.width; maxY = meta.worldY + meta.height; first = false; }
        else {
            minX = std::min(minX, meta.worldX);
            minY = std::min(minY, meta.worldY);
            maxX = std::max(maxX, meta.worldX + meta.width);
            maxY = std::max(maxY, meta.worldY + meta.height);
        }
    }

    int gW = maxX - minX, gH = maxY - minY;
    int texW = gW * PIX_PER_TILE, texH = gH * PIX_PER_TILE;
    m_globalOriginX = minX; m_globalOriginY = minY;
    m_globalPixW = texW; m_globalPixH = texH;

    std::vector<uint8_t> pixels(texW * texH * 4, 0);
    auto px = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        if (x < 0 || x >= texW || y < 0 || y >= texH) return;
        int i = (y * texW + x) * 4;
        pixels[i+0] = r; pixels[i+1] = g; pixels[i+2] = b; pixels[i+3] = a;
    };

    // Pure black background (Hollow Knight parchment-style darkness)
    for (int y = 0; y < texH; ++y)
        for (int x = 0; x < texW; ++x)
            px(x, y, 0, 0, 0);

    // Sort rooms by area descending so interior rooms render on top
    std::vector<std::pair<std::string, MapMeta>> sorted;
    for (auto& [name, meta] : allMaps) sorted.push_back({name, meta});
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) {
        return a.second.width * a.second.height > b.second.width * b.second.height;
    });

    // Render each room at its world position (HK style: white edges, brown interior)
    for (auto& [name, meta] : sorted) {
        int ox = (meta.worldX - minX) * PIX_PER_TILE;
        int oy = (meta.worldY - minY) * PIX_PER_TILE;
        bool isCurrent = (name == m_mapName);

        // Room outer border (1px white outline)
        for (int x = ox; x < ox + meta.width * PIX_PER_TILE; ++x) {
            px(x, oy, 180, 170, 150);
            px(x, oy + meta.height * PIX_PER_TILE - 1, 180, 170, 150);
        }
        for (int y = oy; y < oy + meta.height * PIX_PER_TILE; ++y) {
            px(ox, y, 180, 170, 150);
            px(ox + meta.width * PIX_PER_TILE - 1, y, 180, 170, 150);
        }

        for (int ty = 0; ty < meta.height; ++ty) {
            for (int tx = 0; tx < meta.width; ++tx) {
                uint8_t r = 110, g = 82, b = 50;
                if (isCurrent) { r += 20; g += 15; b += 10; }
                for (int dy = 0; dy < PIX_PER_TILE; ++dy) {
                    for (int dx = 0; dx < PIX_PER_TILE; ++dx) {
                        px(ox + tx * PIX_PER_TILE + dx, oy + ty * PIX_PER_TILE + dy, r, g, b);
                    }
                }
            }
        }
        // Transition markers (gold diamonds)
        for (auto& t : meta.transitions) {
            int cx = ox + (t.tileX + t.tileW / 2) * PIX_PER_TILE + PIX_PER_TILE / 2;
            int cy = oy + (t.tileY + t.tileH / 2) * PIX_PER_TILE + PIX_PER_TILE / 2;
            for (int dy = -3; dy <= 3; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    if (abs(dx) + abs(dy) <= 3) {
                        int xx = cx + dx, yy = cy + dy;
                        float dist = (abs(dx) + abs(dy)) / 3.0f;
                        int bright = 200 + (int)(55 * (1.0f - dist));
                        px(xx, yy, bright, bright * 160 / 200, 30 + (int)(30 * (1.0f - dist)));
                    }
                }
            }
        }
    }

    // Dashed connection lines between matching transitions
    for (auto& [nameA, metaA] : sorted) {
        for (auto& t : metaA.transitions) {
            auto itB = allMaps.find(t.targetMap);
            if (itB == allMaps.end()) continue;
            auto& metaB = itB->second;
            for (auto& tb : metaB.transitions) {
                if (tb.targetMap != nameA) continue;
                (void)tb;
                float ax = (metaA.worldX + t.tileX + t.tileW * 0.5f - minX) * PIX_PER_TILE;
                float ay = (metaA.worldY + t.tileY + t.tileH * 0.5f - minY) * PIX_PER_TILE;
                float bx = (metaB.worldX + tb.tileX + tb.tileW * 0.5f - minX) * PIX_PER_TILE;
                float by = (metaB.worldY + tb.tileY + tb.tileH * 0.5f - minY) * PIX_PER_TILE;
                int steps = std::max(abs((int)(bx - ax)), abs((int)(by - ay)));
                for (int i = 0; i <= steps; ++i) {
                    float frac = (float)i / steps;
                    int lx = (int)(ax + frac * (bx - ax) + 0.5f);
                    int ly = (int)(ay + frac * (by - ay) + 0.5f);
                    if ((i / 2) % 2 == 0) px(lx, ly, 180, 160, 70);
                }
                break;
            }
        }
    }

    // Yellow highlight border around current room
    auto& curMeta = allMaps.at(m_mapName);
    int cx0 = (curMeta.worldX - minX) * PIX_PER_TILE;
    int cy0 = (curMeta.worldY - minY) * PIX_PER_TILE;
    int cx1 = (curMeta.worldX + curMeta.width - minX) * PIX_PER_TILE - 1;
    int cy1 = (curMeta.worldY + curMeta.height - minY) * PIX_PER_TILE - 1;
    for (int x = cx0; x <= cx1; ++x) { px(x, cy0, 255, 220, 100); px(x, cy1, 255, 220, 100); }
    m_mapOverlayTexture = new Texture(m_engine, pixels.data(), texW, texH);

    // Generate high-resolution room name label textures (with proper alpha for anti-aliasing)
    FILE* f = fopen("fonts/alagard.ttf", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> fbuf(fsz);
    if (fread(fbuf.data(), 1, fsz, f) != (size_t)fsz) { fclose(f); return; }
    fclose(f);

    stbtt_fontinfo fi;
    if (!stbtt_InitFont(&fi, fbuf.data(), 0)) return;

    int totalAscent, totalDescent;
    stbtt_GetFontVMetrics(&fi, &totalAscent, &totalDescent, nullptr);
    float labelFontSize = 56.0f;
    float labelScale = stbtt_ScaleForPixelHeight(&fi, labelFontSize);
    float textPixelH = (totalAscent - totalDescent) * labelScale;

    for (auto& [name, meta] : allMaps) {
        std::string dn = fmtRoomName(name);

        float tw = 0;
        for (size_t i = 0; i < dn.size(); ++i) {
            int adv;
            stbtt_GetCodepointHMetrics(&fi, (unsigned char)dn[i], &adv, nullptr);
            tw += adv * labelScale;
        }

        int pad = 4;
        int lw = (int)tw + pad * 2;
        int lh = (int)textPixelH + pad * 2;
        std::vector<uint8_t> lp(lw * lh * 4, 0); // fully transparent (alpha=0)

        int tx = pad;
        int ty = pad + (int)(totalAscent * labelScale);

        // Background starts fully transparent, drawText properly sets alpha for anti-aliased edges
        drawText(lp.data(), lw, lh, &fi, dn.c_str(), tx+1, ty+1, labelFontSize, 40, 30, 10);
        drawText(lp.data(), lw, lh, &fi, dn.c_str(), tx, ty, labelFontSize, 255, 255, 255);

        Texture* tex = new Texture(m_engine, lp.data(), lw, lh);
        tex->setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR);

        TextLabel label;
        label.texture = tex;
        label.texW = lw;
        label.texH = lh;
        label.worldCenterX = meta.worldX + meta.width * 0.5f;
        label.worldCenterY = meta.worldY + meta.height * 0.5f;
        m_textLabels.push_back(label);
    }
}

bool Map::isTileBlocked(int tileX, int tileY) const {
    // Allow tiles within transition zones even slightly past the map edge
    // so the player can walk through door gaps and trigger transitions.
    for (auto& t : m_transitions) {
        if (tileX >= t.tileX && tileX < t.tileX + t.tileW &&
            tileY >= t.tileY && tileY < t.tileY + t.tileH)
            return false;
    }
    if (tileX < 0 || tileX >= m_width || tileY < 0 || tileY >= m_height) {
        return true;
    }
    return m_collisionGrid[tileY * m_width + tileX] != 0;
}

Map::Transition* Map::checkTransition(int tileX, int tileY) {
    for (auto& t : m_transitions) {
        if (tileX >= t.tileX && tileX < t.tileX + t.tileW &&
            tileY >= t.tileY && tileY < t.tileY + t.tileH) {
            return &t;
        }
    }
    return nullptr;
}

void Map::buildDirtPath() {
    if (!m_dirtTexture) {
        // Pixel art dirt road texture (32x32, seamless)
        const int S = 32;
        std::vector<uint8_t> p(S * S * 4, 255);
        auto hash = [](int x, int y) -> unsigned {
            unsigned h = (unsigned)(x * 374761393 + y * 668265263);
            h = (h ^ (h >> 13)) * 1274126177u;
            return (h ^ (h >> 16)) & 0xFF;
        };
        // 5-shade earth palette
        uint8_t c[5][3] = {
            {145, 105, 60},  // base
            {160, 118, 68},  // light
            {125, 88, 48},   // dark
            {100, 68, 35},   // shadow/edge
            {75,  50, 22},   // deep
        };
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                unsigned h2 = hash(x+3, y+7);
                unsigned h3 = hash(x*5+11, y*3+13);
                int idx = 0;
                // Large patches of lighter/darker
                int patch = hash(x/4, y/4) % 7;
                if (patch == 0) idx = 1;
                else if (patch == 1) idx = 2;
                else if (patch == 2) idx = 3;
                // Small pebbles (single pixel)
                if ((h2 % 13) == 0) idx = 3;
                if ((h3 % 19) == 0) idx = 4;
                // Wheel rut bands (subtle horizontal dark strips)
                int wy = (y + 6) % 32;
                if (wy >= 10 && wy <= 14 && (hash(x, y>>1) % 5) < 2) idx = 2;
                int wy2 = (y + 20) % 32;
                if (wy2 >= 10 && wy2 <= 14 && (hash(x+5, y>>1) % 5) < 2) idx = 2;
                // Edge wear (slightly darker near edges)
                if (x < 2 || x >= S-2 || y < 2 || y >= S-2) {
                    if ((hash(x+y, x-y) % 3) == 0) idx = idx < 2 ? 2 : idx;
                }
                p[(y * S + x) * 4 + 0] = c[idx][0];
                p[(y * S + x) * 4 + 1] = c[idx][1];
                p[(y * S + x) * 4 + 2] = c[idx][2];
                p[(y * S + x) * 4 + 3] = 255;
            }
        }
        m_dirtTexture = new Texture(m_engine, p.data(), S, S,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    }

    float ts = (float)m_tileSize;
    float hw = m_width * ts * 0.5f;
    float hh = m_height * ts * 0.5f;

    // Spine points in tile coords
    struct Pt { float x, z; };
    std::vector<Pt> spine;
    auto sp = [&](float tx, float tz) { spine.push_back({tx * ts - hw, tz * ts - hh}); };
    sp(10.0f, 0.0f); sp(10.5f, 1.5f); sp(10.0f, 3.0f);
    sp(10.5f, 4.5f); sp(10.0f, 6.0f); sp(9.5f, 7.5f);
    sp(10.0f, 9.0f); sp(10.5f, 10.5f); sp(10.0f, 12.0f);
    sp(10.0f, 13.5f); sp(10.0f, 15.0f); sp(10.0f, 16.5f);
    sp(10.0f, 18.0f); sp(10.0f, 20.0f);

    size_t n = spine.size();
    const float halfW = ts * 1.0f;

    // Compute averaged perpendicular at each joint
    struct { glm::vec2 perp; } joint[32];
    for (size_t i = 0; i < n; ++i) {
        glm::vec2 dir(0.0f);
        if (i > 0) { glm::vec2 d = {spine[i].x - spine[i-1].x, spine[i].z - spine[i-1].z}; dir += glm::normalize(d); }
        if (i+1 < n) { glm::vec2 d = {spine[i+1].x - spine[i].x, spine[i+1].z - spine[i].z}; dir += glm::normalize(d); }
        if (glm::length(dir) < 0.001f) dir = {0.0f, 1.0f};
        dir = glm::normalize(dir);
        joint[i].perp = {-dir.y, dir.x};
    }

    float totalLen = 0.0f;
    for (size_t i = 0; i+1 < n; ++i)
        totalLen += sqrt((spine[i+1].x - spine[i].x) * (spine[i+1].x - spine[i].x) +
                         (spine[i+1].z - spine[i].z) * (spine[i+1].z - spine[i].z));

    std::vector<QuadVertex> verts;
    std::vector<uint16_t> idxs;
    float acc = 0.0f;

    for (size_t i = 0; i+1 < n; ++i) {
        float segLen = sqrt((spine[i+1].x - spine[i].x) * (spine[i+1].x - spine[i].x) +
                            (spine[i+1].z - spine[i].z) * (spine[i+1].z - spine[i].z));
        float uv0 = acc / totalLen;
        float uv1 = (acc + segLen) / totalLen;

        glm::vec2 p0 = {spine[i].x - joint[i].perp.x * halfW, spine[i].z - joint[i].perp.y * halfW};
        glm::vec2 p1 = {spine[i].x + joint[i].perp.x * halfW, spine[i].z + joint[i].perp.y * halfW};
        glm::vec2 p2 = {spine[i+1].x + joint[i+1].perp.x * halfW, spine[i+1].z + joint[i+1].perp.y * halfW};
        glm::vec2 p3 = {spine[i+1].x - joint[i+1].perp.x * halfW, spine[i+1].z - joint[i+1].perp.y * halfW};

        uint32_t base = (uint32_t)verts.size();
        verts.push_back({{p0.x, 0.01f, p0.y}, {0.0f, uv0}});
        verts.push_back({{p1.x, 0.01f, p1.y}, {1.0f, uv0}});
        verts.push_back({{p2.x, 0.01f, p2.y}, {1.0f, uv1}});
        verts.push_back({{p3.x, 0.01f, p3.y}, {0.0f, uv1}});
        idxs.push_back(base+0); idxs.push_back(base+1); idxs.push_back(base+2);
        idxs.push_back(base+2); idxs.push_back(base+3); idxs.push_back(base+0);

        acc += segLen;
    }

    m_dirtPathMesh.indexCount = (uint32_t)idxs.size();
    VkDeviceSize vsize = sizeof(QuadVertex) * verts.size();
    VkDeviceSize isize = sizeof(uint16_t) * idxs.size();
    m_engine->createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_dirtPathMesh.vertexBuffer, m_dirtPathMesh.vertexBufferMemory);
    void* data;
    vkMapMemory(m_engine->device(), m_dirtPathMesh.vertexBufferMemory, 0, vsize, 0, &data);
    memcpy(data, verts.data(), vsize);
    vkUnmapMemory(m_engine->device(), m_dirtPathMesh.vertexBufferMemory);
    m_engine->createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_dirtPathMesh.indexBuffer, m_dirtPathMesh.indexBufferMemory);
    vkMapMemory(m_engine->device(), m_dirtPathMesh.indexBufferMemory, 0, isize, 0, &data);
    memcpy(data, idxs.data(), isize);
    vkUnmapMemory(m_engine->device(), m_dirtPathMesh.indexBufferMemory);
}

void Map::render() {
    // Tiled floor from actual tileset textures
    if (m_useTileMesh) {
        for (int ti = 0; ti < (int)m_tilesetTextures.size(); ++ti) {
            auto& mesh = m_tileFloorMeshes[ti];
            auto& slot = m_tilesetTextures[ti];
            if (mesh.indexCount > 0 && slot.texture)
                m_renderer->drawTilemap(slot.texture->descriptorSet(),
                    mesh.vertexBuffer, mesh.indexBuffer, mesh.indexCount);
        }
    } else {
        if (m_floorTopMesh.indexCount > 0 && m_floorTexture) {
            m_renderer->drawTilemap(m_floorTexture->descriptorSet(),
                m_floorTopMesh.vertexBuffer, m_floorTopMesh.indexBuffer,
                m_floorTopMesh.indexCount);
        }
    }
    if (m_floorBottomMesh.indexCount > 0 && m_floorTexture) {
        m_renderer->drawTilemap(m_floorTexture->descriptorSet(),
            m_floorBottomMesh.vertexBuffer, m_floorBottomMesh.indexBuffer,
            m_floorBottomMesh.indexCount);
    }
    if (m_wallMesh.indexCount > 0 && m_wallTexture) {
        m_renderer->drawTilemap(m_wallTexture->descriptorSet(),
            m_wallMesh.vertexBuffer, m_wallMesh.indexBuffer, m_wallMesh.indexCount);
    }
    // Tree shadows (projected onto floor)
    if (!m_trees.empty() && m_engine->shadowTexture()) {
        VkDescriptorSet shadowDS = m_engine->shadowTexture()->descriptorSet();
        for (auto& tree : m_trees) {
            glm::mat4 sm = glm::translate(glm::mat4(1.0f), glm::vec3(tree.x, 0.05f, tree.z));
            sm = glm::rotate(sm, -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            sm = glm::scale(sm, glm::vec3(64.0f, 64.0f, 1.0f));
            m_renderer->drawSprite3D(shadowDS, sm);
        }
    }

    // Billboarded trees
    if (!m_trees.empty() && m_treeTexture) {
        glm::vec3 camPos = m_engine->cameraPosition();
        glm::vec2 texSize = m_treeTexture->size();
        float aspect = texSize.x / texSize.y;
        for (auto& tree : m_trees) {
            glm::vec3 fwd = glm::normalize(camPos - glm::vec3(tree.x, 0, tree.z));
            float angle = atan2f(fwd.x, fwd.z);
            float s = tree.scale * 256.0f;
            glm::vec4 vb = m_treeTexture->visibleBounds();
            float drawH = s / aspect;
            float yOff = (vb.w / texSize.y) * drawH - drawH * 0.5f;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(tree.x, yOff, tree.z));
            model = glm::rotate(model, angle, glm::vec3(0, 1, 0));
            model = glm::scale(model, glm::vec3(s, -drawH, 1));
            m_renderer->drawSprite3D(m_treeTexture->descriptorSet(), model);
        }
    }

    // Static decoration sprite (fixed orientation, does NOT rotate with camera)
    if (m_decorationTexture) {
        glm::vec2 texSize = m_decorationTexture->size();
        float aspect = texSize.x / texSize.y;
        float scaleX = 64.0f * m_decorationScale;
        float scaleY = scaleX / aspect;
        // Position sprite so visible feet touch the ground (y=0)
        glm::vec4 vb = m_decorationTexture->visibleBounds();
        float yOffset = (vb.w / texSize.y) * scaleY - scaleY * 0.5f;
        glm::vec3 pos3D = m_decorationPos;
        pos3D.y = yOffset - 10.0f;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos3D);
        // No rotation — sprite stays fixed
        model = glm::scale(model, glm::vec3(scaleX, -scaleY, 1.0f));
        m_renderer->drawSprite3D(m_decorationTexture->descriptorSet(), model);
    }
}
