#include "MapManager.h"
#include "Map.h"
#include "Engine.h"
#include "Renderer.h"
#include "Player.h"

MapManager::MapManager(Engine* engine, Renderer* renderer, Player* player)
    : m_engine(engine), m_renderer(renderer), m_player(player) {
}

MapManager::~MapManager() = default;

void MapManager::loadMap(const std::string& mapId) {
    m_engine->waitIdle();
    m_currentMap = std::make_unique<Map>(m_engine, m_renderer);
    m_currentMap->load(mapId);

    float spawnX = m_currentMap->spawnTileX() * m_currentMap->tileSize() + m_currentMap->tileSize() / 2.0f;
    float spawnY = m_currentMap->spawnTileY() * m_currentMap->tileSize() + m_currentMap->tileSize() / 2.0f;
    m_player->setPosition(glm::vec2(spawnX, spawnY));

    m_engine->setMapBounds(
        m_currentMap->worldWidth(),
        m_currentMap->worldHeight()
    );
}

void MapManager::startTransition(const std::string& mapId, int spawnTileX, int spawnTileY) {
    m_transitioning = true;
    m_fadeTimer = 0.0f;
    m_fadingOut = true;
    m_targetMap = mapId;
    m_targetSpawnX = spawnTileX;
    m_targetSpawnY = spawnTileY;
}

void MapManager::update(float deltaTime) {
    if (!m_currentMap) return;

    if (m_transitioning) {
        m_fadeTimer += deltaTime;
        if (m_fadingOut && m_fadeTimer >= m_fadeDuration) {
            m_fadingOut = false;
            m_fadeTimer = 0.0f;

            m_engine->waitIdle();
            m_currentMap = std::make_unique<Map>(m_engine, m_renderer);
            m_currentMap->load(m_targetMap);
            float spawnX = m_targetSpawnX * m_currentMap->tileSize() + m_currentMap->tileSize() / 2.0f;
            float spawnY = m_targetSpawnY * m_currentMap->tileSize() + m_currentMap->tileSize() / 2.0f;
            m_player->setPosition(glm::vec2(spawnX, spawnY));
            m_engine->setMapBounds(m_currentMap->worldWidth(), m_currentMap->worldHeight());
        }
        if (!m_fadingOut && m_fadeTimer >= m_fadeDuration) {
            m_transitioning = false;
            m_cooldownFrames = 5;
        }
        return;
    }

    if (m_cooldownFrames > 0) {
        --m_cooldownFrames;
        return;
    }

    if (!m_currentMap) return;

    glm::vec2 playerPos = m_player->position();
    int tileX = static_cast<int>(playerPos.x) / m_currentMap->tileSize();
    int tileY = static_cast<int>(playerPos.y) / m_currentMap->tileSize();

    Map::Transition* transition = m_currentMap->checkTransition(tileX, tileY);
    if (transition) {
        startTransition(transition->targetMap, transition->spawnTileX, transition->spawnTileY);
    }
}

void MapManager::render() {
    if (m_currentMap) {
        m_currentMap->render();
    }
}
