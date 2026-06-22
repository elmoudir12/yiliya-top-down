#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <glm/glm.hpp>

class Engine;

class Texture {
public:
    Texture(Engine* engine, const std::string& filepath);
    Texture(Engine* engine, const void* pixelData, int width, int height,
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    ~Texture();

    VkDescriptorSet descriptorSet() const { return m_descriptorSet; }
    glm::vec2 size() const { return m_size; }
    glm::vec4 visibleBounds() const { return m_visibleBounds; }
    void setAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v);
    void setFilter(VkFilter mag, VkFilter min);

private:
    Engine* m_engine;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    glm::vec2 m_size{};
    glm::vec4 m_visibleBounds{0,0,0,0};
    VkSamplerAddressMode m_addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode m_addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    void createTextureImage(const std::string& filepath);
    void createTextureImageView();
    void createTextureSampler();
    void updateDescriptorSet();
};
