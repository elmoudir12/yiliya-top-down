#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstddef>

class Engine;

struct QuadVertex {
    glm::vec3 pos;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(QuadVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributes{};
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(QuadVertex, pos);
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[1].offset = offsetof(QuadVertex, texCoord);
        return attributes;
    }
};

struct SpritePushConstants {
    glm::mat4 model;
    glm::vec4 color = glm::vec4(1.0f);
};

struct DebugPushConstants {
    glm::mat4 model;
    glm::vec4 color;
};

class Renderer {
public:
    Renderer(Engine* engine);
    ~Renderer();

    bool beginFrame();
    void drawSprite(VkDescriptorSet descriptorSet, const glm::vec2& position, const glm::vec2& scale, float rotation = 0.0f);
    void drawSprite3D(VkDescriptorSet descriptorSet, const glm::mat4& model, const glm::vec4& color = glm::vec4(1.0f));
    void drawTilemap(VkDescriptorSet descriptorSet, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount);
    void drawDebugRect(const glm::vec3& position, const glm::vec2& scale, const glm::vec4& color);
    void drawDebugBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);
    void setScissor(int x, int y, int w, int h);
    void resetScissor();
    void endFrame();
    void setClearColor(float r, float g, float b);

    VkDescriptorPool& descriptorPool() { return m_descriptorPool; }
    VkDescriptorSetLayout& textureDescriptorLayout() { return m_textureDescriptorLayout; }

private:
    Engine* m_engine;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_textureDescriptorLayout = VK_NULL_HANDLE;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uniformBufferMemory = VK_NULL_HANDLE;
    void* m_uniformBufferMapped = nullptr;

    VkDescriptorSet m_uniformDescriptorSet = VK_NULL_HANDLE;

    // Debug pipelines
    VkPipelineLayout m_debugPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_debugPipeline = VK_NULL_HANDLE;
    VkPipeline m_boxDebugPipeline = VK_NULL_HANDLE;

    VkBuffer m_boxLineBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_boxLineBufferMemory = VK_NULL_HANDLE;

    VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;
    uint32_t m_imageIndex = 0;
    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    void createVertexBuffer();
    void createDebugPipeline();
    void createBoxLineBuffer();
    void createBoxDebugPipeline();
    void createIndexBuffer();
    void createUniformBuffer();
    void createDescriptorPool();
    void createTextureDescriptorSetLayout();
    void createUniformDescriptorSet();
};
