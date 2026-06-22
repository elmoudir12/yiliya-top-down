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

struct LightPushConstants {
    glm::mat4 model;
    glm::vec4 color;
};

struct OffscreenAttachment {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

class Renderer {
public:
    Renderer(Engine* engine);
    ~Renderer();

    // Frame lifecycle
    bool beginFrame();
    void cmdBeginScenePass();
    void cmdBeginLightPass(const glm::vec3& ambientColor, float ambientIntensity);
    void cmdComposite();
    void endFrame();

    // Drawing
    void drawSprite(VkDescriptorSet descriptorSet, const glm::vec2& position, const glm::vec2& scale, float rotation = 0.0f);
    void drawTilemap(VkDescriptorSet descriptorSet, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount);
    void drawLight(const glm::vec2& position, const glm::vec2& scale, const glm::vec4& color);
    void drawShadow(float cx, float cy, float sx, float sy);

    // Descriptor access
    VkDescriptorPool& descriptorPool() { return m_descriptorPool; }
    VkDescriptorSetLayout& textureDescriptorLayout() { return m_textureDescriptorLayout; }

    // Lighting resources (call on init and swapchain recreate)
    void createLightingResources();
    void destroyLightingResources();

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

    // Fullscreen quad for compositing
    VkBuffer m_fsVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_fsVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_fsIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_fsIndexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_fsIndexCount = 0;

    // Offscreen render passes
    VkRenderPass m_sceneRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_lightRenderPass = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

    struct OffscreenResources {
        OffscreenAttachment sceneColor;
        OffscreenAttachment sceneDepth;
        VkFramebuffer sceneFramebuffer = VK_NULL_HANDLE;
        OffscreenAttachment lightColor;
        VkFramebuffer lightFramebuffer = VK_NULL_HANDLE;
        VkDescriptorSet compositeDescSet = VK_NULL_HANDLE;
    };
    std::vector<OffscreenResources> m_offscreenResources;

    VkSampler m_offscreenSampler = VK_NULL_HANDLE;

    // Lighting pipelines
    VkPipelineLayout m_lightPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_lightPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;

    // Composite
    VkDescriptorSetLayout m_compositeDescSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_compositePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_compositePipeline = VK_NULL_HANDLE;

    // Helper methods
    void createVertexBuffer();
    void createIndexBuffer();
    void createFullscreenQuad();
    void createUniformBuffer();
    void createDescriptorPool();
    void createTextureDescriptorSetLayout();
    void createUniformDescriptorSet();
    void createOffscreenRenderPasses();
    void createOffscreenFramebuffers();
    void destroyOffscreenFramebuffers();
    void createOffscreenSampler();
    void createCompositeDescriptorSetLayout();
    void createCompositeDescriptorSets();
    void createLightPipeline();
    void createShadowPipeline();
    void createCompositePipeline();
    void destroyAttachment(OffscreenAttachment& att);
};
