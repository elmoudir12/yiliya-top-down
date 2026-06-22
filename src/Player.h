#pragma once

#include "Texture.h"
#include <glm/glm.hpp>
#include <array>
#include <memory>

class Engine;
class Renderer;
class Map;

enum class Direction {
    Front,
    Back,
    Left,
    Right
};

class Player {
public:
    Player(Engine* engine, Renderer* renderer);
    ~Player() = default;

    void update(float deltaTime, const Map* currentMap);
    void render();

    glm::vec3 position() const { return m_position; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }

    static constexpr float PLAYER_WIDTH = 48.0f;
    static constexpr float PLAYER_HEIGHT = 48.0f;

private:
    Engine* m_engine;
    Renderer* m_renderer;

    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    Direction m_direction = Direction::Front;
    float m_facingAngle = 0.0f; // character's world-facing angle (degrees, 0 = -Z)
    int m_frame = 0;
    float m_animTimer = 0.0f;
    bool m_moving = false;

    static constexpr float MOVE_SPEED = 160.0f;
    static constexpr float ANIM_SPEED = 0.15f;

    std::array<std::array<std::unique_ptr<Texture>, 4>, 4> m_textures;

    void loadTextures();
    int directionIndex(Direction dir) const;
    bool canMoveTo(float x, float z, const Map* map) const;
    Direction idleDirection() const;
};
