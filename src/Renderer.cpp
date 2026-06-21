#include "Renderer.h"
#include "Engine.h"
#include <stdexcept>
#include <array>
#include <cstring>

static const std::vector<QuadVertex> quadVertices = {
    {{-0.5f, -0.5f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f}},
};

static const std::vector<uint16_t> quadIndices = { 0, 1, 2, 2, 3, 0 };

Renderer::Renderer(Engine* engine)
    : m_engine(engine) {
    createVertexBuffer();
    createIndexBuffer();
    createDescriptorPool();
    createTextureDescriptorSetLayout();
    createUniformBuffer();
    createUniformDescriptorSet();
}

Renderer::~Renderer() {
    VkDevice dev = m_engine->device();
    if (m_uniformBufferMapped) vkUnmapMemory(dev, m_uniformBufferMemory);
    if (m_uniformBuffer) vkDestroyBuffer(dev, m_uniformBuffer, nullptr);
    if (m_uniformBufferMemory) vkFreeMemory(dev, m_uniformBufferMemory, nullptr);
    if (m_indexBuffer) vkDestroyBuffer(dev, m_indexBuffer, nullptr);
    if (m_indexBufferMemory) vkFreeMemory(dev, m_indexBufferMemory, nullptr);
    if (m_vertexBuffer) vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
    if (m_vertexBufferMemory) vkFreeMemory(dev, m_vertexBufferMemory, nullptr);
    if (m_textureDescriptorLayout) vkDestroyDescriptorSetLayout(dev, m_textureDescriptorLayout, nullptr);
    if (m_descriptorPool) vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);
}

void Renderer::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(QuadVertex) * quadVertices.size();
    m_engine->createBuffer(bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_vertexBuffer, m_vertexBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), m_vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, quadVertices.data(), bufferSize);
    vkUnmapMemory(m_engine->device(), m_vertexBufferMemory);
}

void Renderer::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(uint16_t) * quadIndices.size();
    m_engine->createBuffer(bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_indexBuffer, m_indexBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), m_indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, quadIndices.data(), bufferSize);
    vkUnmapMemory(m_engine->device(), m_indexBufferMemory);
}

void Renderer::createUniformBuffer() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    m_engine->createBuffer(bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_uniformBuffer, m_uniformBufferMemory);

    vkMapMemory(m_engine->device(), m_uniformBufferMemory, 0, bufferSize, 0, &m_uniformBufferMapped);
}

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 64;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 65;

    if (vkCreateDescriptorPool(m_engine->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void Renderer::createTextureDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_engine->device(), &layoutInfo, nullptr, &m_textureDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture descriptor set layout");
    }
}

void Renderer::createUniformDescriptorSet() {
    VkDescriptorSetLayout layout = m_engine->descriptorSetLayout();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(m_engine->device(), &allocInfo, &m_uniformDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate uniform descriptor set");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_uniformDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_engine->device(), 1, &descriptorWrite, 0, nullptr);
}

bool Renderer::beginFrame() {
    VkResult result = vkAcquireNextImageKHR(m_engine->device(), m_engine->swapChain(),
        UINT64_MAX, m_engine->imageAvailableSemaphore(), VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_engine->recreateSwapChain();
        return false;
    }

    VkFence& fence = m_engine->inFlightFence();
    vkWaitForFences(m_engine->device(), 1, &fence, VK_TRUE, UINT64_MAX);

    VkExtent2D extent = m_engine->swapChainExtent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    float left = -aspect * 360.0f;
    float right = aspect * 360.0f;

    UniformBufferObject ubo{};
    ubo.projection = glm::ortho(left, right, 360.0f, -360.0f, -1.0f, 1.0f);
    memcpy(m_uniformBufferMapped, &ubo, sizeof(ubo));

    m_currentCommandBuffer = m_engine->commandBuffer(m_imageIndex);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(m_currentCommandBuffer, &beginInfo);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_engine->renderPass();
    renderPassInfo.framebuffer = m_engine->framebuffer(m_imageIndex);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.1f, 0.3f, 0.05f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_currentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_engine->graphicsPipeline());

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_engine->pipelineLayout(), 0, 1, &m_uniformDescriptorSet, 0, nullptr);
    return true;
}

void Renderer::drawSprite(VkDescriptorSet descriptorSet, const glm::vec2& position, const glm::vec2& scale, float rotation) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
    model = glm::scale(model, glm::vec3(scale, 1.0f));

    SpritePushConstants push{};
    push.model = model;
    vkCmdPushConstants(m_currentCommandBuffer, m_engine->pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SpritePushConstants), &push);

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_engine->pipelineLayout(), 1, 1, &descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

void Renderer::endFrame() {
    vkCmdEndRenderPass(m_currentCommandBuffer);

    if (vkEndCommandBuffer(m_currentCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_engine->imageAvailableSemaphore() };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_currentCommandBuffer;

    VkSemaphore signalSemaphores[] = { m_engine->renderFinishedSemaphore() };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkFence& fence = m_engine->inFlightFence();
    vkResetFences(m_engine->device(), 1, &fence);

    if (vkQueueSubmit(m_engine->graphicsQueue(), 1, &submitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_engine->swapChain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_engine->presentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_engine->recreateSwapChain();
    }
}
