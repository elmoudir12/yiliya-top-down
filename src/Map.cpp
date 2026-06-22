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

    // Build mesh for each layer
    m_layerMeshes.resize(m_tileLayers.size());
    for (size_t i = 0; i < m_tileLayers.size(); ++i) {
        buildMeshForLayer(m_tileLayers[i], m_layerMeshes[i]);
    }

    buildWalls();
}

void Map::unload() {
    for (auto& mesh : m_layerMeshes) {
        destroyMesh(mesh);
    }
    m_layerMeshes.clear();
    destroyMesh(m_wallMesh);
    m_groundTiles.clear();
    m_collisionTiles.clear();
    m_tileLayers.clear();
    m_collisionRects.clear();
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

void Map::buildMeshForLayer(const std::vector<int>& tiles, LayerMesh& mesh) {
    int numTiles = m_width * m_height;
    if (numTiles == 0) return;

    glm::vec2 texSize = m_tilesetTexture->size();
    float texW = texSize.x;
    float texH = texSize.y;

    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;

    std::vector<QuadVertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(numTiles * 4);
    indices.reserve(numTiles * 6);

    for (int ty = 0; ty < m_height; ++ty) {
        for (int tx = 0; tx < m_width; ++tx) {
            int tileIdx = tiles[ty * m_width + tx];
            if (tileIdx == 324) continue;

            int srcCol = tileIdx % m_tilesetCols;
            int srcRow = tileIdx / m_tilesetCols;

            float u0 = (srcCol * TILE_PX) / texW;
            float v0 = (srcRow * TILE_PX) / texH;
            float u1 = ((srcCol + 1) * TILE_PX) / texW;
            float v1 = ((srcRow + 1) * TILE_PX) / texH;

            float x0 = static_cast<float>(tx * m_tileSize) - hw;
            float z0 = static_cast<float>(ty * m_tileSize) - hh;
            float x1 = x0 + m_tileSize;
            float z1 = z0 + m_tileSize;

            uint32_t base = static_cast<uint32_t>(vertices.size());
            vertices.push_back({{x0, 0.0f, z0}, {u0, v0}});
            vertices.push_back({{x1, 0.0f, z0}, {u1, v0}});
            vertices.push_back({{x1, 0.0f, z1}, {u1, v1}});
            vertices.push_back({{x0, 0.0f, z1}, {u0, v1}});
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
            indices.push_back(base + 0);
        }
    }

    mesh.indexCount = static_cast<uint32_t>(indices.size());
    if (mesh.indexCount == 0) return;

    VkDeviceSize vertexSize = sizeof(QuadVertex) * vertices.size();
    VkDeviceSize indexSize = sizeof(uint16_t) * indices.size();

    m_engine->createBuffer(vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        mesh.vertexBuffer, mesh.vertexBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), mesh.vertexBufferMemory, 0, vertexSize, 0, &data);
    memcpy(data, vertices.data(), vertexSize);
    vkUnmapMemory(m_engine->device(), mesh.vertexBufferMemory);

    m_engine->createBuffer(indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        mesh.indexBuffer, mesh.indexBufferMemory);

    vkMapMemory(m_engine->device(), mesh.indexBufferMemory, 0, indexSize, 0, &data);
    memcpy(data, indices.data(), indexSize);
    vkUnmapMemory(m_engine->device(), mesh.indexBufferMemory);
}

void Map::buildWalls() {
    glm::vec2 texSize = m_tilesetTexture->size();
    float texH = texSize.y;

    int wallTile = 307;
    int srcRow = wallTile / m_tilesetCols;
    float v0 = (srcRow * TILE_PX) / texH;
    float v1 = ((srcRow + 1) * TILE_PX) / texH;

    float hw = m_width * m_tileSize * 0.5f;
    float hh = m_height * m_tileSize * 0.5f;
    float wh = WALL_HEIGHT;
    float t = WALL_THICK;

    std::vector<QuadVertex> verts;
    std::vector<uint16_t> idxs;

    auto addQuad = [&](const glm::vec3& a, const glm::vec3& b,
                        const glm::vec3& c, const glm::vec3& d,
                        float uA, float uB, float uC, float uD,
                        float vA, float vB, float vC, float vD) {
        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back({a, {uA, vA}});
        verts.push_back({b, {uB, vB}});
        verts.push_back({c, {uC, vC}});
        verts.push_back({d, {uD, vD}});
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

    for (int i = 0; i < 4; ++i) {
        auto& w = walls[i];
        float x0 = w.x0, x1 = w.x1, z0 = w.z0, z1 = w.z1;

        // +X face (if wall extends in +X direction — east face of box)
        addQuad({x1, 0, z0}, {x1, 0, z1}, {x1, wh, z1}, {x1, wh, z0},
                0, 1, 1, 0, v0, v0, v1, v1);
        // -X face
        addQuad({x0, 0, z1}, {x0, 0, z0}, {x0, wh, z0}, {x0, wh, z1},
                0, 1, 1, 0, v0, v0, v1, v1);
        // +Z face
        addQuad({x1, 0, z1}, {x0, 0, z1}, {x0, wh, z1}, {x1, wh, z1},
                0, 1, 1, 0, v0, v0, v1, v1);
        // -Z face
        addQuad({x0, 0, z0}, {x1, 0, z0}, {x1, wh, z0}, {x0, wh, z0},
                0, 1, 1, 0, v0, v0, v1, v1);
        // top face (+Y)
        addQuad({x0, wh, z1}, {x1, wh, z1}, {x1, wh, z0}, {x0, wh, z0},
                0, 1, 1, 0, v0, v0, v1, v1);
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
    for (auto& mesh : m_layerMeshes) {
        if (mesh.indexCount > 0) {
            m_renderer->drawTilemap(m_tilesetTexture->descriptorSet(),
                mesh.vertexBuffer, mesh.indexBuffer, mesh.indexCount);
        }
    }
    if (m_wallMesh.indexCount > 0) {
        m_renderer->drawTilemap(m_tilesetTexture->descriptorSet(),
            m_wallMesh.vertexBuffer, m_wallMesh.indexBuffer, m_wallMesh.indexCount);
    }
}
