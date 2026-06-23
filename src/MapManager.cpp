#include "MapManager.h"
#include "Map.h"
#include "Engine.h"
#include "Renderer.h"
#include "Player.h"
#include "Npc.h"
#include <cmath>

MapManager::MapManager(Engine* engine, Renderer* renderer, Player* player)
    : m_engine(engine), m_renderer(renderer), m_player(player) {
}

MapManager::~MapManager() = default;

static glm::vec3 tileToWorld(int tileX, int tileY, int tileSize, int mapW, int mapH) {
    float hw = mapW * tileSize * 0.5f;
    float hh = mapH * tileSize * 0.5f;
    float x = tileX * tileSize + tileSize * 0.5f - hw;
    float z = tileY * tileSize + tileSize * 0.5f - hh;
    return glm::vec3(x, 0.0f, z);
}

void MapManager::loadMap(const std::string& mapId) {
    m_engine->waitIdle();
    m_currentMap = std::make_unique<Map>(m_engine, m_renderer);
    m_currentMap->load(mapId);

    glm::vec3 spawnPos = tileToWorld(
        m_currentMap->spawnTileX(), m_currentMap->spawnTileY(),
        m_currentMap->tileSize(), m_currentMap->width(), m_currentMap->height());
    m_player->setPosition(spawnPos);
    if (m_engine->npc()) {
        m_engine->npc()->setPosition(spawnPos + glm::vec3(0.0f, 0.0f, -64.0f));
    }
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
            glm::vec3 spawnPos = tileToWorld(
                m_targetSpawnX, m_targetSpawnY,
                m_currentMap->tileSize(), m_currentMap->width(), m_currentMap->height());
            m_player->setPosition(spawnPos);
            if (m_engine->npc()) {
                // Offset NPC away from the map edge the player entered from
                int w = m_currentMap->width();
                int h = m_currentMap->height();
                glm::vec3 npcOff(0.0f, 0.0f, 0.0f);
                if (m_targetSpawnY <= 1)
                    npcOff.z =  64.0f; // entered from north, place south
                else if (m_targetSpawnY >= h - 2)
                    npcOff.z = -64.0f; // entered from south, place north
                else if (m_targetSpawnX <= 1)
                    npcOff.x =  64.0f; // entered from west, place east
                else if (m_targetSpawnX >= w - 2)
                    npcOff.x = -64.0f; // entered from east, place west
                else
                    npcOff.z = -64.0f; // fallback: place north
                m_engine->npc()->setPosition(spawnPos + npcOff);
            }
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

    glm::vec3 playerPos = m_player->position();
    float hw = m_currentMap->width() * m_currentMap->tileSize() * 0.5f;
    float hh = m_currentMap->height() * m_currentMap->tileSize() * 0.5f;
    int tileX = static_cast<int>(std::floor((playerPos.x + hw) / m_currentMap->tileSize()));
    int tileY = static_cast<int>(std::floor((playerPos.z + hh) / m_currentMap->tileSize()));

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
