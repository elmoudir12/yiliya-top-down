#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstddef>

class Engine;

struct QuadVertex {
    glm::vec2 pos;
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
        attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
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
};

class Renderer {
public:
    Renderer(Engine* engine);
    ~Renderer();

    bool beginFrame();
    void drawSprite(VkDescriptorSet descriptorSet, const glm::vec2& position, const glm::vec2& scale, float rotation = 0.0f);
    void endFrame();

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

    VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;
    uint32_t m_imageIndex = 0;

    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffer();
    void createDescriptorPool();
    void createTextureDescriptorSetLayout();
    void createUniformDescriptorSet();
};
