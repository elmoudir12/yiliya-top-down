#include "Player.h"
#include "Engine.h"
#include "Renderer.h"
#include "Map.h"
#include <GLFW/glfw3.h>
#include <string>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

Player::Player(Engine* engine, Renderer* renderer)
    : m_engine(engine), m_renderer(renderer) {
    loadTextures();
}

void Player::loadTextures() {
    static const char* dirNames[] = { "front", "back", "left", "right" };
    for (int d = 0; d < 4; ++d) {
        for (int f = 0; f < 4; ++f) {
            std::string path = "assets/yir/walk " + std::string(dirNames[d]) + " " + std::to_string(f + 1) + ".png";
            m_textures[d][f] = std::make_unique<Texture>(m_engine, path);
        }
    }
}

int Player::directionIndex(Direction dir) const {
    switch (dir) {
        case Direction::Front: return 0;
        case Direction::Back: return 1;
        case Direction::Left: return 2;
        case Direction::Right: return 3;
    }
    return 0;
}

static bool aabbOverlap(float ax, float ay, float aw, float ah,
                         float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

bool Player::canMoveTo(float x, float z, const Map* map) const {
    if (!map) return true;

    float hw = map->width() * map->tileSize() * 0.5f;
    float hh = map->height() * map->tileSize() * 0.5f;
    float tileSize = static_cast<float>(map->tileSize());

    // Convert 3D (x, z) to 2D tile space (top-left origin)
    float tx = x + hw;
    float ty = z + hh;

    float playerLeft = tx - 10.0f;
    float playerTop = ty + 9.0f;
    float playerRight = tx + 10.0f - 1.0f;
    float playerBottom = ty + 23.0f - 1.0f;

    // Tile grid collision
    int tx1 = static_cast<int>(playerLeft) / static_cast<int>(tileSize);
    int ty1 = static_cast<int>(playerTop) / static_cast<int>(tileSize);
    int tx2 = static_cast<int>(playerRight) / static_cast<int>(tileSize);
    int ty2 = static_cast<int>(playerBottom) / static_cast<int>(tileSize);

    for (int tty = ty1; tty <= ty2; ++tty) {
        for (int ttx = tx1; ttx <= tx2; ++ttx) {
            if (map->isTileBlocked(ttx, tty)) return false;
        }
    }

    // Rectangle collision from object layer
    const auto& rects = map->collisionRects();
    for (const auto& r : rects) {
        if (aabbOverlap(playerLeft, playerTop,
                        playerRight - playerLeft, playerBottom - playerTop,
                        r.x, r.y, r.w, r.h)) {
            return false;
        }
    }

    return true;
}

void Player::update(float deltaTime, const Map* currentMap) {
    m_moving = false;

    bool wPressed = glfwGetKey(m_engine->window(), GLFW_KEY_W) == GLFW_PRESS;
    bool sPressed = glfwGetKey(m_engine->window(), GLFW_KEY_S) == GLFW_PRESS;
    bool aPressed = glfwGetKey(m_engine->window(), GLFW_KEY_A) == GLFW_PRESS;
    bool dPressed = glfwGetKey(m_engine->window(), GLFW_KEY_D) == GLFW_PRESS;

    float step = MOVE_SPEED * deltaTime;

    // Camera-relative movement: W/S forward/back, A/D strafe
    float yawRad = glm::radians(m_engine->cameraYaw());
    float fwdX = -std::sin(yawRad);
    float fwdZ = -std::cos(yawRad);
    float rgtX = std::cos(yawRad);
    float rgtZ = -std::sin(yawRad);

    if (wPressed || sPressed || aPressed || dPressed) {
        float dx = 0.0f, dz = 0.0f;
        int count = 0;
        if (wPressed) { dx += fwdX; dz += fwdZ; ++count; }
        if (sPressed) { dx -= fwdX; dz -= fwdZ; ++count; }
        if (aPressed) { dx -= rgtX; dz -= rgtZ; ++count; }
        if (dPressed) { dx += rgtX; dz += rgtZ; ++count; }
        if (count > 1) {
            float inv = 1.0f / std::sqrt(2.0f);
            dx *= inv; dz *= inv;
        }
        dx *= step; dz *= step;

        // Axis-separated collision for wall sliding
        float newX = m_position.x + dx;
        if (canMoveTo(newX, m_position.z, currentMap)) {
            m_position.x = newX;
            m_moving = true;
        }
        float newZ = m_position.z + dz;
        if (canMoveTo(m_position.x, newZ, currentMap)) {
            m_position.z = newZ;
            m_moving = true;
        }
    }

    if (sPressed && (dPressed || aPressed)) {
        m_direction = Direction::Front;
    } else if (wPressed) {
        m_direction = Direction::Back;
    } else if (sPressed) {
        m_direction = Direction::Front;
    } else if (aPressed) {
        m_direction = Direction::Left;
    } else if (dPressed) {
        m_direction = Direction::Right;
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

void Player::render() {
    int di = directionIndex(m_direction);
    auto& tex = m_textures[di][m_frame];
    glm::vec2 texSize = tex->size();
    float aspect = texSize.x / texSize.y;
    float scaleX = 64.0f;
    float scaleY = scaleX / aspect;

    // Directional billboard: face camera but only rotate around Y axis
    glm::vec3 camPos = m_engine->cameraPosition();
    glm::vec3 dir = glm::normalize(camPos - m_position);
    float angle = atan2f(dir.x, dir.z);

    // Slightly above floor to avoid z-fighting
    // Negate Y scale to un-flip the sprite (projection has Y-flip for 3D room orientation)
    glm::vec3 pos3D = m_position;
    pos3D.y = 32.0f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos3D);
    model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleX, -scaleY, 1.0f));

    m_renderer->drawSprite3D(tex->descriptorSet(), model);
}
