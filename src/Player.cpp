#include "Player.h"
#include "Engine.h"
#include "Renderer.h"
#include <GLFW/glfw3.h>
#include <string>

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

void Player::update(float deltaTime) {
    m_moving = false;

    if (glfwGetKey(m_engine->window(), GLFW_KEY_W) == GLFW_PRESS) {
        m_position.y += MOVE_SPEED * deltaTime;
        m_direction = Direction::Back;
        m_moving = true;
    }
    if (glfwGetKey(m_engine->window(), GLFW_KEY_S) == GLFW_PRESS) {
        m_position.y -= MOVE_SPEED * deltaTime;
        m_direction = Direction::Front;
        m_moving = true;
    }
    if (glfwGetKey(m_engine->window(), GLFW_KEY_A) == GLFW_PRESS) {
        m_position.x -= MOVE_SPEED * deltaTime;
        m_direction = Direction::Left;
        m_moving = true;
    }
    if (glfwGetKey(m_engine->window(), GLFW_KEY_D) == GLFW_PRESS) {
        m_position.x += MOVE_SPEED * deltaTime;
        m_direction = Direction::Right;
        m_moving = true;
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
