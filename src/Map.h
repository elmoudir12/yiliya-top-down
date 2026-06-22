#pragma once

#include "TmxLoader.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Engine;
class Renderer;
class Texture;

class Map {
public:
    struct Transition {
        int tileX, tileY, tileW, tileH;
        std::string targetMap;
        int spawnTileX, spawnTileY;
    };

    Map(Engine* engine, Renderer* renderer);
    ~Map();

    void load(const std::string& mapName);
    void unload();
    void render();

    bool isTileBlocked(int tileX, int tileY) const;
    Transition* checkTransition(int tileX, int tileY);

    int width() const { return m_width; }
    int height() const { return m_height; }
    int tileSize() const { return m_tileSize; }
    int spawnTileX() const { return m_spawnTileX; }
    int spawnTileY() const { return m_spawnTileY; }
    const std::string& mapId() const { return m_mapName; }

    float worldWidth() const { return m_width * m_tileSize; }
    float worldHeight() const { return m_height * m_tileSize; }

    const std::vector<CollisionRect>& collisionRects() const { return m_collisionRects; }
    const std::vector<int>& collisionTiles() const { return m_collisionTiles; }

private:
    struct LayerMesh {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
    };

    Engine* m_engine;
    Renderer* m_renderer;

    static Texture* s_tilesetTexture;
    static int s_tilesetRefCount;

    std::string m_mapName;
    int m_width = 0, m_height = 0;
    int m_tileSize = 32;
    int m_tilesetCols = 8;

    std::vector<int> m_groundTiles;
    std::vector<int> m_collisionTiles;
    std::vector<std::vector<int>> m_tileLayers;
    std::vector<Transition> m_transitions;
    std::vector<CollisionRect> m_collisionRects;
    std::vector<LayerMesh> m_layerMeshes;
    int m_spawnTileX = 0, m_spawnTileY = 0;

    Texture* m_tilesetTexture = nullptr;

    void buildMeshForLayer(const std::vector<int>& tiles, LayerMesh& mesh);
    void destroyMesh(LayerMesh& mesh);
};
