#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <glm/glm.hpp>

class Engine;

class Texture {
public:
    Texture(Engine* engine, const std::string& filepath);
    ~Texture();

    VkDescriptorSet descriptorSet() const { return m_descriptorSet; }
    glm::vec2 size() const { return m_size; }

private:
    Engine* m_engine;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    glm::vec2 m_size{};

    void createTextureImage(const std::string& filepath);
    void createTextureImageView();
    void createTextureSampler();
    void updateDescriptorSet();
};
