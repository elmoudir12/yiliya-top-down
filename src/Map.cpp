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

struct MapMeta {
    std::vector<Map::Transition> transitions;
    int spawnTileX, spawnTileY;
};

static const std::unordered_map<std::string, MapMeta>& getMapMeta() {
    static const std::unordered_map<std::string, MapMeta> meta = {
        {"player_house", {
            .transitions = {{5, 14, 2, 1, "town_center", 12, 2}},
            .spawnTileX = 10,
            .spawnTileY = 7,
        }},
        {"town_center", {
            .transitions = {{8, 17, 5, 1, "player_house", 7, 12}},
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
    m_tilesetCols = 16;
    m_groundTiles = std::move(tmx.groundTiles);
    m_collisionTiles = std::move(tmx.collisionTiles);

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

    buildMesh();
}

void Map::unload() {
    VkDevice dev = m_engine->device();
    if (m_indexBuffer) {
        vkDestroyBuffer(dev, m_indexBuffer, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBufferMemory) {
        vkFreeMemory(dev, m_indexBufferMemory, nullptr);
        m_indexBufferMemory = VK_NULL_HANDLE;
    }
    if (m_vertexBuffer) {
        vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexBufferMemory) {
        vkFreeMemory(dev, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }
    m_indexCount = 0;
    m_groundTiles.clear();
    m_collisionTiles.clear();
    m_transitions.clear();
}

void Map::buildMesh() {
    int numTiles = m_width * m_height;
    if (numTiles == 0) return;

    glm::vec2 texSize = m_tilesetTexture->size();
    float texW = texSize.x;
    float texH = texSize.y;

    std::vector<QuadVertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(numTiles * 4);
    indices.reserve(numTiles * 6);

    for (int ty = 0; ty < m_height; ++ty) {
        for (int tx = 0; tx < m_width; ++tx) {
            int tileIdx = m_groundTiles[ty * m_width + tx];
            int srcCol = tileIdx % m_tilesetCols;
            int srcRow = tileIdx / m_tilesetCols;

            float u0 = (srcCol * TILE_PX) / texW;
            float v0 = (srcRow * TILE_PX) / texH;
            float u1 = ((srcCol + 1) * TILE_PX) / texW;
            float v1 = ((srcRow + 1) * TILE_PX) / texH;

            float x0 = static_cast<float>(tx * m_tileSize);
            float y0 = static_cast<float>(ty * m_tileSize);
            float x1 = x0 + m_tileSize;
            float y1 = y0 + m_tileSize;

            uint32_t base = static_cast<uint32_t>(vertices.size());
            vertices.push_back({{x0, y0}, {u0, v0}});
            vertices.push_back({{x1, y0}, {u1, v0}});
            vertices.push_back({{x1, y1}, {u1, v1}});
            vertices.push_back({{x0, y1}, {u0, v1}});
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
            indices.push_back(base + 0);
        }
    }

    m_indexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize vertexSize = sizeof(QuadVertex) * vertices.size();
    VkDeviceSize indexSize = sizeof(uint16_t) * indices.size();

    m_engine->createBuffer(vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_vertexBuffer, m_vertexBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), m_vertexBufferMemory, 0, vertexSize, 0, &data);
    memcpy(data, vertices.data(), vertexSize);
    vkUnmapMemory(m_engine->device(), m_vertexBufferMemory);

    m_engine->createBuffer(indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_indexBuffer, m_indexBufferMemory);

    vkMapMemory(m_engine->device(), m_indexBufferMemory, 0, indexSize, 0, &data);
    memcpy(data, indices.data(), indexSize);
    vkUnmapMemory(m_engine->device(), m_indexBufferMemory);
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
    if (!m_tilesetTexture || m_indexCount == 0) return;
    m_renderer->drawTilemap(m_tilesetTexture->descriptorSet(),
        m_vertexBuffer, m_indexBuffer, m_indexCount);
}


