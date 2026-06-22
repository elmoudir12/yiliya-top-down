#pragma once

#include <glm/glm.hpp>
#include <vector>

class Engine;
class Renderer;
class Map;

class LightManager {
public:
    struct Light {
        glm::vec2 position;
        glm::vec3 color;
        float radius;
        float intensity;
    };

    LightManager(Engine* engine, Renderer* renderer);
    ~LightManager();

    void setAmbient(const glm::vec3& color, float intensity);
    void addLight(const Light& light);
    void clearLights();

    void renderLights(const Map* currentMap);
    void renderShadows(const Map* currentMap);

    const glm::vec3& ambientColor() const { return m_ambientColor; }
    float ambientIntensity() const { return m_ambientIntensity; }

private:
    Engine* m_engine;
    Renderer* m_renderer;

    glm::vec3 m_ambientColor{ 0.2f, 0.18f, 0.15f };
    float m_ambientIntensity = 1.0f;
    std::vector<Light> m_lights;
};
