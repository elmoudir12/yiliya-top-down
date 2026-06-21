#include "Player.h"
#include "Engine.h"
#include "Renderer.h"
#include "Map.h"
#include <GLFW/glfw3.h>
#include <string>
#include <algorithm>

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

bool Player::canMoveTo(float x, float y, const Map* map) const {
    if (!map) return true;

    int tileSize = map->tileSize();
    float halfW = PLAYER_WIDTH / 2.0f - 2.0f;
    float halfH = PLAYER_HEIGHT / 2.0f - 2.0f;

    int tx1 = static_cast<int>((x - halfW)) / tileSize;
    int ty1 = static_cast<int>((y - halfH)) / tileSize;
    int tx2 = static_cast<int>((x + halfW - 1)) / tileSize;
    int ty2 = static_cast<int>((y + halfH - 1)) / tileSize;

    for (int ty = ty1; ty <= ty2; ++ty) {
        for (int tx = tx1; tx <= tx2; ++tx) {
            if (map->isTileBlocked(tx, ty)) return false;
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

    if (wPressed) {
        float newY = m_position.y - step;
        if (canMoveTo(m_position.x, newY, currentMap)) {
            m_position.y = newY;
            m_moving = true;
        }
    }
    if (sPressed) {
        float newY = m_position.y + step;
        if (canMoveTo(m_position.x, newY, currentMap)) {
            m_position.y = newY;
            m_moving = true;
        }
    }
    if (aPressed) {
        float newX = m_position.x - step;
        if (canMoveTo(newX, m_position.y, currentMap)) {
            m_position.x = newX;
            m_moving = true;
        }
    }
    if (dPressed) {
        float newX = m_position.x + step;
        if (canMoveTo(newX, m_position.y, currentMap)) {
            m_position.x = newX;
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
    m_renderer->drawSprite(tex->descriptorSet(), m_position, glm::vec2(scaleX, scaleY));
}
