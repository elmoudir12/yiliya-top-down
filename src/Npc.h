#pragma once

#include "Texture.h"
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <string>

class Engine;
class Renderer;
class Map;

class Npc {
public:
    Npc(Engine* engine, Renderer* renderer, const std::string& folder);
    ~Npc() = default;

    void update(float deltaTime, const Map* currentMap, const glm::vec3& target);
    void render();

    glm::vec3 position() const { return m_position; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    glm::vec4 visibleBounds3D() const;

    static constexpr float NPC_WIDTH = 48.0f;

private:
    Engine* m_engine;
    Renderer* m_renderer;

    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    float m_facingAngle = 0.0f;
    int m_frame = 0;
    float m_animTimer = 0.0f;
    bool m_moving = false;

    static constexpr float MOVE_SPEED = 120.0f;
    static constexpr float ANIM_SPEED = 0.18f;
    static constexpr float FOLLOW_DIST = 48.0f;
    static constexpr float STOP_DIST = 40.0f;

    std::array<std::array<std::unique_ptr<Texture>, 4>, 4> m_textures;

    void loadTextures(const std::string& folder);
    int directionIndex(const std::string& dir) const;

    bool canMoveTo(float x, float z, const Map* map, const glm::vec3& playerPos) const;
    void getBounds(float x, float z, const Map* map,
                   float& outLeft, float& outTop,
                   float& outRight, float& outBottom) const;
};
