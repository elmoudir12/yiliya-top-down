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

void Player::getBounds(float x, float z, const Map* map,
                       float& outLeft, float& outTop,
                       float& outRight, float& outBottom) const {
    float hw = map->width() * map->tileSize() * 0.5f;
    float hh = map->height() * map->tileSize() * 0.5f;
    auto& tex = m_textures[directionIndex(m_direction)][m_frame];
    glm::vec4 vb = tex->visibleBounds();
    float texW = tex->size().x;
    float scale = 64.0f / texW;
    float halfWorld = texW * scale * 0.5f;

    float tx = x + hw;
    float ty = z + hh;
    outLeft   = tx - halfWorld + vb.x * scale;
    outTop    = ty - halfWorld + vb.y * scale;
    outRight  = tx - halfWorld + vb.z * scale;
    outBottom = ty - halfWorld + vb.w * scale;
}

bool Player::canMoveTo(float x, float z, const Map* map) const {
    if (!map) return true;

    float tileSize = static_cast<float>(map->tileSize());

    float playerLeft, playerTop, playerRight, playerBottom;
    getBounds(x, z, map, playerLeft, playerTop, playerRight, playerBottom);

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

// Push player out of any overlapping collision rect along the shortest axis.
// The player is INSIDE the rect; we find which side they're closest to
// (smallest overlap) and push them out that direction.
void Player::resolveCollisions(const Map* map) {
    if (!map) return;
    const auto& rects = map->collisionRects();
    if (rects.empty()) return;

    for (int iter = 0; iter < 4; ++iter) {
        float playerLeft, playerTop, playerRight, playerBottom;
        getBounds(m_position.x, m_position.z, map,
                  playerLeft, playerTop, playerRight, playerBottom);

        bool anyOverlap = false;
        for (const auto& r : rects) {
            if (!aabbOverlap(playerLeft, playerTop,
                             playerRight - playerLeft, playerBottom - playerTop,
                             r.x, r.y, r.w, r.h)) continue;
            anyOverlap = true;

            // Player is inside the rect. Compute push distance to escape each side.
            // Sign convention: positive delta_x moves player RIGHT (larger x),
            // positive delta_z moves player DOWN (larger z in tile Y space).
            float pushLeft   = r.x - playerRight;          // negative: escape left
            float pushRight  = (r.x + r.w) - playerLeft;   // positive: escape right
            float pushUp     = r.y - playerBottom;         // negative: escape up (smaller z)
            float pushDown   = (r.y + r.h) - playerTop;    // positive: escape down (larger z)

            // Choose smallest magnitude push along each axis
            float axPush = (std::abs(pushLeft) < std::abs(pushRight)) ? pushLeft : pushRight;
            float axPushZ = (std::abs(pushUp) < std::abs(pushDown)) ? pushUp : pushDown;

            // Pick the smaller-magnitude axis
            if (std::abs(axPush) < std::abs(axPushZ)) {
                m_position.x += axPush;
            } else {
                m_position.z += axPushZ;
            }
            break; // restart with updated bounds
        }
        if (!anyOverlap) break;
    }
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

    // Push out of any overlapping collision rect (prevents getting stuck)
    resolveCollisions(currentMap);

    if (sPressed && (dPressed || aPressed)) {
        m_direction = Direction::Front;
        m_facingAngle = m_engine->cameraYaw() + 180.0f;
    } else if (wPressed) {
        m_direction = Direction::Back;
        m_facingAngle = m_engine->cameraYaw();
    } else if (sPressed) {
        m_direction = Direction::Front;
        m_facingAngle = m_engine->cameraYaw() + 180.0f;
    } else if (aPressed) {
        m_direction = Direction::Left;
        m_facingAngle = m_engine->cameraYaw() + 90.0f;
    } else if (dPressed) {
        m_direction = Direction::Right;
        m_facingAngle = m_engine->cameraYaw() - 90.0f;
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

glm::vec4 Player::visibleBounds3D() const {
    auto& tex = m_textures[directionIndex(m_direction)][m_frame];
    glm::vec4 vb = tex->visibleBounds();
    float texW = tex->size().x;
    float scale = 64.0f / texW;
    float half = texW * scale * 0.5f;
    return {
        m_position.x - half + vb.x * scale,
        m_position.z - half + vb.y * scale,
        m_position.x - half + vb.z * scale,
        m_position.z - half + vb.w * scale,
    };
}

void Player::render() {
    // Always pick the sprite that shows the correct side of the character
    // based on camera angle relative to the character's current facing direction.
    Direction dir = idleDirection();
    int di = directionIndex(dir);
    auto& tex = m_textures[di][m_frame];
    glm::vec2 texSize = tex->size();
    float aspect = texSize.x / texSize.y;
    float scaleX = 64.0f;
    float scaleY = scaleX / aspect;

    // Billboard: always face the camera so the sprite is always visible
    glm::vec3 camPos = m_engine->cameraPosition();
    glm::vec3 fwd = glm::normalize(camPos - m_position);
    float angle = atan2f(fwd.x, fwd.z);

    glm::vec3 pos3D = m_position;
    pos3D.y = 32.0f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos3D);
    model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleX, -scaleY, 1.0f));

    m_renderer->drawSprite3D(tex->descriptorSet(), model);
}

Direction Player::idleDirection() const {
    // Camera angle from the character's position
    glm::vec3 camPos = m_engine->cameraPosition();
    glm::vec3 dir = glm::normalize(camPos - m_position);
    float camAngle = glm::degrees(atan2f(dir.x, dir.z));

    // Relative angle between camera and character's facing direction
    float diff = camAngle - m_facingAngle;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;

    //   diff ≈ 0°    → camera behind character → Back
    //   diff ≈ 90°   → camera to character's right → Right
    //   diff ≈ ±180° → camera in front → Front
    //   diff ≈ -90°  → camera to character's left → Left
    if (diff > -45.0f && diff <= 45.0f) return Direction::Back;
    if (diff > 45.0f && diff <= 135.0f) return Direction::Right;
    if (diff > -135.0f && diff <= -45.0f) return Direction::Left;
    return Direction::Front;
}