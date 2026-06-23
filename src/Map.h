#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct CollisionRect {
    float x, y, w, h;
};

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
    const std::vector<CollisionRect>& wallCollisionRects() const { return m_wallCollisionRects; }

    static constexpr float wallHeight() { return WALL_HEIGHT; }
    static constexpr float wallThick() { return WALL_THICK; }

    static constexpr int mapPixPerTile() { return 10; }

    struct TextLabel {
        Texture* texture = nullptr;
        int texW = 0, texH = 0;
        float worldCenterX = 0, worldCenterY = 0;
    };

    Texture* mapOverlayTexture() const { return m_mapOverlayTexture; }
    const std::vector<TextLabel>& textLabels() const { return m_textLabels; }
    int globalOriginX() const { return m_globalOriginX; }
    int globalOriginY() const { return m_globalOriginY; }
    int globalPixW() const { return m_globalPixW; }
    int globalPixH() const { return m_globalPixH; }
    int curWorldX() const { return m_curWorldX; }
    int curWorldY() const { return m_curWorldY; }

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

    std::string m_mapName;
    int m_width = 0, m_height = 0;
    int m_tileSize = 32;

    std::vector<uint8_t> m_collisionGrid;
    std::vector<Transition> m_transitions;
    std::vector<CollisionRect> m_collisionRects;
    std::vector<CollisionRect> m_wallCollisionRects;
    struct DoorGap { int side; float gapMin, gapMax; };
    std::vector<DoorGap> m_doorGaps;
    LayerMesh m_wallMesh;
    LayerMesh m_floorTopMesh;
    LayerMesh m_floorBottomMesh;
    int m_spawnTileX = 0, m_spawnTileY = 0;

    Texture* m_wallTexture = nullptr;
    Texture* m_floorTexture = nullptr;
    Texture* m_treeTexture = nullptr;
    Texture* m_decorationTexture = nullptr;
    glm::vec3 m_decorationPos{ 0.0f, 0.0f, 0.0f };
    float m_decorationScale = 1.0f;
    struct Tree { float x, z; float scale; };
    std::vector<Tree> m_trees;

    static constexpr float WALL_HEIGHT = 64.0f;
    static constexpr float WALL_THICK = 32.0f;

    void buildFloorTop();
    void buildFloorBottom();
    void buildWalls();
    void buildBoundaryFence();
    void destroyMesh(LayerMesh& mesh);

private:
    Texture* m_mapOverlayTexture = nullptr;
    std::vector<TextLabel> m_textLabels;
    int m_globalOriginX = 0, m_globalOriginY = 0;
    int m_globalPixW = 0, m_globalPixH = 0;
    int m_curWorldX = 0, m_curWorldY = 0;
    void generateMapTexture();
};
