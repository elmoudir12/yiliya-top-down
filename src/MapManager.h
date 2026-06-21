#pragma once

#include <string>
#include <memory>

class Engine;
class Renderer;
class Map;
class Player;

class MapManager {
public:
    MapManager(Engine* engine, Renderer* renderer, Player* player);
    ~MapManager();

    void loadMap(const std::string& mapId);
    void update(float deltaTime);
    void render();

    Map* currentMap() const { return m_currentMap.get(); }
    bool isTransitioning() const { return m_transitioning; }

private:
    Engine* m_engine;
    Renderer* m_renderer;
    Player* m_player;

    std::unique_ptr<Map> m_currentMap;

    bool m_transitioning = false;
    float m_fadeTimer = 0.0f;
    float m_fadeDuration = 0.5f;
    std::string m_targetMap;
    int m_targetSpawnX = 0;
    int m_targetSpawnY = 0;
    bool m_fadingOut = true;
    int m_cooldownFrames = 0;

    void startTransition(const std::string& mapId, int spawnTileX, int spawnTileY);
};
