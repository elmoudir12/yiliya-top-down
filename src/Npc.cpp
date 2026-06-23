#include "Npc.h"
#include "Engine.h"
#include "Renderer.h"
#include "Map.h"
#include <string>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

Npc::Npc(Engine* engine, Renderer* renderer, const std::string& folder)
    : m_engine(engine), m_renderer(renderer) {
    loadTextures(folder);
}

void Npc::loadTextures(const std::string& folder) {
    static const char* dirNames[] = { "front", "back", "left", "right" };
    for (int d = 0; d < 4; ++d) {
        for (int f = 0; f < 4; ++f) {
            std::string path = folder + "/walk " + std::string(dirNames[d]) + " " + std::to_string(f + 1) + ".png";
            m_textures[d][f] = std::make_unique<Texture>(m_engine, path);
        }
    }
}

int Npc::directionIndex(const std::string& dir) const {
    if (dir == "front") return 0;
    if (dir == "back") return 1;
    if (dir == "left") return 2;
    if (dir == "right") return 3;
    return 0;
}

static bool npcAabbOverlap(float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

void Npc::getBounds(float x, float z, const Map* map,
                    float& outLeft, float& outTop,
                    float& outRight, float& outBottom) const {
    float hw = map->width() * map->tileSize() * 0.5f;
    float hh = map->height() * map->tileSize() * 0.5f;
    float scale = 64.0f;
    float halfWorld = scale * 0.5f;
    float tx = x + hw;
    float ty = z + hh;
    outLeft   = tx - halfWorld;
    outTop    = ty - halfWorld;
    outRight  = tx + halfWorld;
    outBottom = ty + halfWorld;
}

bool Npc::canMoveTo(float x, float z, const Map* map, const glm::vec3& playerPos) const {
    if (!map) return true;

    float tileSize = static_cast<float>(map->tileSize());

    float npcLeft, npcTop, npcRight, npcBottom;
    getBounds(x, z, map, npcLeft, npcTop, npcRight, npcBottom);

    // Tile grid collision
    int tx1 = static_cast<int>(npcLeft) / static_cast<int>(tileSize);
    int ty1 = static_cast<int>(npcTop) / static_cast<int>(tileSize);
    int tx2 = static_cast<int>(npcRight) / static_cast<int>(tileSize);
    int ty2 = static_cast<int>(npcBottom) / static_cast<int>(tileSize);

    for (int tty = ty1; tty <= ty2; ++tty) {
        for (int ttx = tx1; ttx <= tx2; ++ttx) {
            if (map->isTileBlocked(ttx, tty)) return false;
        }
    }

    // Rectangle collision from object layer (walls, fence, etc.)
    const auto& rects = map->collisionRects();
    for (const auto& r : rects) {
        if (npcAabbOverlap(npcLeft, npcTop,
                           npcRight - npcLeft, npcBottom - npcTop,
                           r.x, r.y, r.w, r.h)) {
            return false;
        }
    }

    // Collision with player
    float playerHalf = 24.0f;
    float pLeft = playerPos.x + map->width() * map->tileSize() * 0.5f - playerHalf;
    float pTop  = playerPos.z + map->height() * map->tileSize() * 0.5f - playerHalf;
    if (npcAabbOverlap(npcLeft, npcTop, npcRight - npcLeft, npcBottom - npcTop,
                       pLeft, pTop, playerHalf * 2, playerHalf * 2)) {
        return false;
    }

    return true;
}

void Npc::update(float deltaTime, const Map* currentMap, const glm::vec3& target) {
    m_moving = false;

    glm::vec3 diff = target - m_position;
    float dist = glm::length(diff);

    // Direction based on movement
    if (dist > STOP_DIST) {
        glm::vec3 dir = diff / dist;
        float step = MOVE_SPEED * deltaTime;

        // Always face the player (front sprite toward player)
            float angle = atan2f(-dir.x, -dir.z);
            m_facingAngle = glm::degrees(angle);

        // Try X movement
        float newX = m_position.x + dir.x * step;
        if (canMoveTo(newX, m_position.z, currentMap, target)) {
            m_position.x = newX;
            m_moving = true;
        }

        // Try Z movement
        float newZ = m_position.z + dir.z * step;
        if (canMoveTo(m_position.x, newZ, currentMap, target)) {
            m_position.z = newZ;
            m_moving = true;
        }
    }

    if (m_moving) {
        m_animTimer += deltaTime;
        if (m_animTimer >= ANIM_SPEED) {
            m_animTimer = 0.0f;
            m_frame = (m_frame + 1) % 4;
        }
    } else {
        m_frame = 0;
        m_animTimer = 0.0f;
    }
}

glm::vec4 Npc::visibleBounds3D() const {
    float scale = 64.0f;
    float half = scale * 0.5f;
    return {
        m_position.x - half,
        m_position.z - half,
        m_position.x + half,
        m_position.z + half,
    };
}

void Npc::render() {
    // Shadow blob
    if (m_engine->shadowTexture()) {
        glm::mat4 sm = glm::translate(glm::mat4(1.0f), glm::vec3(m_position.x, 0.05f, m_position.z));
        sm = glm::rotate(sm, -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
        sm = glm::scale(sm, glm::vec3(40.0f, 40.0f, 1.0f));
        m_renderer->drawSprite3D(m_engine->shadowTexture()->descriptorSet(), sm);
    }

    // Choose sprite based on facing direction relative to camera
    glm::vec3 camPos = m_engine->cameraPosition();
    glm::vec3 camDir = glm::normalize(camPos - m_position);
    float camAngle = glm::degrees(atan2f(camDir.x, camDir.z));

    float diff = camAngle - m_facingAngle;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;

    int di;
    if (diff > -45.0f && diff <= 45.0f) di = 1;       // Back
    else if (diff > 45.0f && diff <= 135.0f) di = 3;    // Right
    else if (diff > -135.0f && diff <= -45.0f) di = 2;  // Left
    else di = 0;                                          // Front

    auto& tex = m_textures[di][m_frame];
    glm::vec2 texSize = tex->size();
    float aspect = texSize.x / texSize.y;
    float scaleX = 64.0f;
    float scaleY = scaleX / aspect;

    // Position sprite so feet touch the ground
    glm::vec4 vb = tex->visibleBounds();
    float yOffset = (vb.w / texSize.y) * 64.0f - 32.0f;

    glm::vec3 fwd = glm::normalize(camPos - m_position);
    float angle = atan2f(fwd.x, fwd.z);

    glm::vec3 pos3D = m_position;
    pos3D.y = yOffset;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos3D);
    model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleX, -scaleY, 1.0f));

    m_renderer->drawSprite3D(tex->descriptorSet(), model);
}
