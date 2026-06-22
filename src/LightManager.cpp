#include "LightManager.h"
#include "Renderer.h"
#include "Engine.h"
#include "Map.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

LightManager::LightManager(Engine* engine, Renderer* renderer)
    : m_engine(engine), m_renderer(renderer) {
}

LightManager::~LightManager() = default;

void LightManager::setAmbient(const glm::vec3& color, float intensity) {
    m_ambientColor = color;
    m_ambientIntensity = intensity;
}

void LightManager::addLight(const Light& light) {
    m_lights.push_back(light);
}

void LightManager::clearLights() {
    m_lights.clear();
}

void LightManager::renderLights(const Map* currentMap) {
    for (const auto& light : m_lights) {
        float radius = light.radius;
        glm::vec2 scale(radius * 2, radius * 2);
        glm::vec4 color(light.color * light.intensity, light.intensity);
        m_renderer->drawLight(light.position, scale, color);
    }
}

void LightManager::renderShadows(const Map* currentMap) {
    if (!currentMap) return;

    int width = currentMap->width();
    int height = currentMap->height();
    int tileSize = currentMap->tileSize();
    const auto& collisionTiles = currentMap->collisionTiles();

    float shadowExtrude = 96.0f;

    for (const auto& light : m_lights) {
        float lx = light.position.x;
        float ly = light.position.y;
        float lightRadius = light.radius;

        int txMin = std::max(0, static_cast<int>((lx - lightRadius) / tileSize) - 1);
        int tyMin = std::max(0, static_cast<int>((ly - lightRadius) / tileSize) - 1);
        int txMax = std::min(width - 1, static_cast<int>((lx + lightRadius) / tileSize) + 1);
        int tyMax = std::min(height - 1, static_cast<int>((ly + lightRadius) / tileSize) + 1);

        for (int ty = tyMin; ty <= tyMax; ++ty) {
            for (int tx = txMin; tx <= txMax; ++tx) {
                int idx = ty * width + tx;
                if (idx < 0 || idx >= static_cast<int>(collisionTiles.size())) continue;
                if (collisionTiles[idx] == 0) continue;

                float tileCX = tx * tileSize + tileSize / 2.0f;
                float tileCY = ty * tileSize + tileSize / 2.0f;

                float dx = tileCX - lx;
                float dy = tileCY - ly;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > lightRadius + tileSize) continue;

                float nx = 0.0f, ny = 0.0f;
                if (dist > 0.001f) {
                    nx = dx / dist;
                    ny = dy / dist;
                }

                float offX = nx * shadowExtrude;
                float offY = ny * shadowExtrude;

                float cx = tileCX + offX * 0.3f;
                float cy = tileCY + offY * 0.3f;
                float sx = tileSize + std::abs(offX) * 0.6f;
                float sy = tileSize + std::abs(offY) * 0.6f;

                m_renderer->drawShadow(cx, cy, sx, sy);
            }
        }
    }
}
