#include "Map.h"
#include "Engine.h"
#include "Renderer.h"
#include "Texture.h"
#include "TmxLoader.h"

#include <cstring>
#include <stdexcept>
#include <unordered_map>

Texture* Map::s_tilesetTexture = nullptr;
int Map::s_tilesetRefCount = 0;

static const int TILE_PX = 32;

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
    std::vector<Map::Transition> transitions;
    int spawnTileX, spawnTileY;
};

static const std::unordered_map<std::string, MapMeta>& getMapMeta() {
    static const std::unordered_map<std::string, MapMeta> meta = {
        {"player_house", {
            .transitions = {{4, 10, 2, 1, "town_center", 12, 2}},
            .spawnTileX = 4,
            .spawnTileY = 4,
        }},
        {"town_center", {
            .transitions = {{8, 17, 5, 1, "player_house", 5, 8}},
            .spawnTileX = 12,
            .spawnTileY = 8,
        }},
    };
    return meta;
}

Map::Map(Engine* engine, Renderer* renderer)
    : m_engine(engine), m_renderer(renderer) {
    if (!s_tilesetTexture) {
        s_tilesetTexture = new Texture(m_engine, "assets/tiles/tileset.png");
        s_tilesetTexture->setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    }
    ++s_tilesetRefCount;
    m_tilesetTexture = s_tilesetTexture;
}

Map::~Map() {
    unload();
    --s_tilesetRefCount;
    if (s_tilesetRefCount <= 0 && s_tilesetTexture) {
        delete s_tilesetTexture;
        s_tilesetTexture = nullptr;
    }
}

void Map::load(const std::string& mapName) {
    m_tilesetTexture = s_tilesetTexture;

    std::string tmxPath = "maps/" + mapName + ".tmx";
    TmxMapData tmx;
    if (!loadTmx(tmxPath, tmx)) {
        throw std::runtime_error("Failed to load TMX: " + tmxPath);
    }

    m_mapName = mapName;
    m_width = tmx.width;
    m_height = tmx.height;
    m_tileSize = tmx.tileSize;
    m_tilesetCols = 8;
    m_groundTiles = std::move(tmx.groundTiles);
    m_collisionTiles = std::move(tmx.collisionTiles);
    m_tileLayers = std::move(tmx.tileLayers);
    m_collisionRects = std::move(tmx.collisionRects);

    const auto& meta = getMapMeta();
    auto it = meta.find(mapName);
    if (it != meta.end()) {
        m_transitions = it->second.transitions;
        m_spawnTileX = it->second.spawnTileX;
        m_spawnTileY = it->second.spawnTileY;
    } else {
        m_spawnTileX = m_width / 2;
        m_spawnTileY = m_height / 2;
    }

    if (!m_wallTexture) {
        m_wallTexture = createTimberTexture(m_engine);
    }
    if (!m_floorTexture) {
        m_floorTexture = createWoodFloorTexture(m_engine);
    }

    buildFloorTop();
    buildFloorBottom();
    buildWalls();
}

void Map::unload() {
    destroyMesh(m_wallMesh);
    destroyMesh(m_floorTopMesh);
    destroyMesh(m_floorBottomMesh);
    if (m_floorTexture) {
        delete m_floorTexture;
        m_floorTexture = nullptr;
    }
    if (m_wallTexture) {
        delete m_wallTexture;
        m_wallTexture = nullptr;
    }
    m_groundTiles.clear();
    m_collisionTiles.clear();
    m_tileLayers.clear();
    m_collisionRects.clear();
    m_wallCollisionRects.clear();
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
        idxs.push_back(base + 0); idxs.push_back(base + 1); idxs.push_back(base + 2);
        idxs.push_back(base + 2); idxs.push_back(base + 3); idxs.push_back(base + 0);
    };

    // Each wall is a solid box (5 faces, no bottom since floor covers it).
    // Walls overlap at corners by the full thickness so there are no gaps.
    // Box definition: x0,x1 = X range, z0,z1 = Z range, y0=0, y1=wh.
    // Faces: +X, -X, +Z, -Z, top.

    struct WallBox { float x0, x1, z0, z1; };
    WallBox walls[4] = {
        // north: extends past east and west edges by t
        {-hw - t, hw + t, -hh - t, -hh},
        // south
        {-hw - t, hw + t,  hh,      hh + t},
        // west
        {-hw - t, -hw,     -hh - t, hh + t},
        // east
        { hw,     hw + t,  -hh - t, hh + t},
    };

    // Collision rects for the 3D wall boxes
    m_wallCollisionRects.clear();
    for (auto& b : walls) {
        m_wallCollisionRects.push_back({b.x0 + hw, b.z0 + hh, b.x1 - b.x0, b.z1 - b.z0});
        m_collisionRects.push_back(m_wallCollisionRects.back());
    }

    for (int i = 0; i < 4; ++i) {
        auto& w = walls[i];
        float x0 = w.x0, x1 = w.x1, z0 = w.z0, z1 = w.z1;

        // +X face
        addWallQuad({x1, 0, z0}, {x1, 0, z1}, {x1, wh, z1}, {x1, wh, z0});
        // -X face
        addWallQuad({x0, 0, z1}, {x0, 0, z0}, {x0, wh, z0}, {x0, wh, z1});
        // +Z face
        addWallQuad({x1, 0, z1}, {x0, 0, z1}, {x0, wh, z1}, {x1, wh, z1});
        // -Z face
        addWallQuad({x0, 0, z0}, {x1, 0, z0}, {x1, wh, z0}, {x0, wh, z0});
        // top and bottom faces — same wood texture as the wainscoting
        addWallQuad({x0, wh, z1}, {x1, wh, z1}, {x1, wh, z0}, {x0, wh, z0}, 0.5f, 0.9375f);
        addWallQuad({x0, 0, z0}, {x1, 0, z0}, {x1, 0, z1}, {x0, 0, z1}, 0.5f, 0.9375f);
    }

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

bool Map::isTileBlocked(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= m_width || tileY < 0 || tileY >= m_height) {
        return true;
    }
    return m_collisionTiles[tileY * m_width + tileX] != 0;
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

void Map::render() {
    if (!m_tilesetTexture) return;
    if (m_floorTopMesh.indexCount > 0 && m_floorTexture) {
        m_renderer->drawTilemap(m_floorTexture->descriptorSet(),
            m_floorTopMesh.vertexBuffer, m_floorTopMesh.indexBuffer,
            m_floorTopMesh.indexCount);
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
}
