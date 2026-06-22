#include "Texture.h"
#include "Engine.h"
#include "Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstring>
#include <stdexcept>

Texture::Texture(Engine* engine, const std::string& filepath)
    : m_engine(engine) {
    createTextureImage(filepath);
    createTextureImageView();
    createTextureSampler();
    updateDescriptorSet();
}

Texture::Texture(Engine* engine, const void* pixelData, int width, int height,
    VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV)
    : m_engine(engine), m_addressModeU(addressModeU), m_addressModeV(addressModeV) {
    m_size = glm::vec2(width, height);
    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_engine->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixelData, imageSize);
    vkUnmapMemory(m_engine->device(), stagingBufferMemory);

    m_engine->createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);

    m_engine->transitionImageLayout(m_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_engine->copyBufferToImage(stagingBuffer, m_image, width, height);
    m_engine->transitionImageLayout(m_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_engine->device(), stagingBuffer, nullptr);
    vkFreeMemory(m_engine->device(), stagingBufferMemory, nullptr);

    createTextureImageView();
    createTextureSampler();
    updateDescriptorSet();
}

Texture::~Texture() {
    VkDevice dev = m_engine->device();
    if (m_descriptorSet) {
        vkFreeDescriptorSets(dev, m_engine->renderer()->descriptorPool(), 1, &m_descriptorSet);
    }
    if (m_sampler) vkDestroySampler(dev, m_sampler, nullptr);
    if (m_imageView) vkDestroyImageView(dev, m_imageView, nullptr);
    if (m_image) vkDestroyImage(dev, m_image, nullptr);
    if (m_imageMemory) vkFreeMemory(dev, m_imageMemory, nullptr);
}

void Texture::createTextureImage(const std::string& filepath) {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) throw std::runtime_error("Failed to load texture: " + filepath);

    m_size = glm::vec2(texWidth, texHeight);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_engine->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_engine->device(), stagingBufferMemory);

    // Scan for visible pixel bounds (non-zero alpha)
    int minX = texWidth, minY = texHeight, maxX = 0, maxY = 0;
    bool hasVisible = false;
    for (int y = 0; y < texHeight; ++y) {
        for (int x = 0; x < texWidth; ++x) {
            if (pixels[(y * texWidth + x) * 4 + 3] > 0) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
                hasVisible = true;
            }
        }
    }
    if (hasVisible) {
        m_visibleBounds = glm::vec4(minX, minY, maxX + 1, maxY + 1);
    } else {
        m_visibleBounds = glm::vec4(0, 0, texWidth, texHeight);
    }

    stbi_image_free(pixels);

    m_engine->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);

    m_engine->transitionImageLayout(m_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_engine->copyBufferToImage(stagingBuffer, m_image, texWidth, texHeight);
    m_engine->transitionImageLayout(m_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_engine->device(), stagingBuffer, nullptr);
    vkFreeMemory(m_engine->device(), stagingBufferMemory, nullptr);
}

void Texture::createTextureImageView() {
    m_imageView = m_engine->createImageView(m_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Texture::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = m_addressModeU;
    samplerInfo.addressModeV = m_addressModeV;
    samplerInfo.addressModeW = m_addressModeU;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_engine->device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
}

void Texture::setAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v) {
    m_addressModeU = u;
    m_addressModeV = v;
    VkDevice dev = m_engine->device();
    if (m_sampler) vkDestroySampler(dev, m_sampler, nullptr);
    if (m_descriptorSet) {
        vkFreeDescriptorSets(dev, m_engine->renderer()->descriptorPool(), 1, &m_descriptorSet);
        m_descriptorSet = VK_NULL_HANDLE;
    }
    createTextureSampler();
    updateDescriptorSet();
}

void Texture::updateDescriptorSet() {
    VkDescriptorSetLayout texLayout = m_engine->renderer()->textureDescriptorLayout();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_engine->renderer()->descriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &texLayout;

    if (vkAllocateDescriptorSets(m_engine->device(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_imageView;
    imageInfo.sampler = m_sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 1;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_engine->device(), 1, &descriptorWrite, 0, nullptr);
}
