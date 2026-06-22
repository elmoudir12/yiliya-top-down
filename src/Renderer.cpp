#include "Renderer.h"
#include "Engine.h"
#include <stdexcept>
#include <array>
#include <cstring>

static const std::vector<QuadVertex> quadVertices = {
    {{-0.5f, -0.5f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f}, {0.0f, 1.0f}},
};
static const std::vector<uint16_t> quadIndices = { 0, 1, 2, 2, 3, 0 };

static const std::vector<QuadVertex> fsQuadVertices = {
    {{-1.0f, -1.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f}, {0.0f, 1.0f}},
};
static const std::vector<uint16_t> fsQuadIndices = { 0, 1, 2, 2, 3, 0 };

Renderer::Renderer(Engine* engine)
    : m_engine(engine) {
    createVertexBuffer();
    createIndexBuffer();
    createFullscreenQuad();
    createDescriptorPool();
    createTextureDescriptorSetLayout();
    createUniformBuffer();
    createUniformDescriptorSet();
    createLightingResources();
}

Renderer::~Renderer() {
    destroyLightingResources();
    VkDevice dev = m_engine->device();
    if (m_uniformBufferMapped) vkUnmapMemory(dev, m_uniformBufferMemory);
    if (m_uniformBuffer) vkDestroyBuffer(dev, m_uniformBuffer, nullptr);
    if (m_uniformBufferMemory) vkFreeMemory(dev, m_uniformBufferMemory, nullptr);
    if (m_indexBuffer) vkDestroyBuffer(dev, m_indexBuffer, nullptr);
    if (m_indexBufferMemory) vkFreeMemory(dev, m_indexBufferMemory, nullptr);
    if (m_vertexBuffer) vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
    if (m_vertexBufferMemory) vkFreeMemory(dev, m_vertexBufferMemory, nullptr);
    if (m_fsIndexBuffer) vkDestroyBuffer(dev, m_fsIndexBuffer, nullptr);
    if (m_fsIndexBufferMemory) vkFreeMemory(dev, m_fsIndexBufferMemory, nullptr);
    if (m_fsVertexBuffer) vkDestroyBuffer(dev, m_fsVertexBuffer, nullptr);
    if (m_fsVertexBufferMemory) vkFreeMemory(dev, m_fsVertexBufferMemory, nullptr);
    if (m_textureDescriptorLayout) vkDestroyDescriptorSetLayout(dev, m_textureDescriptorLayout, nullptr);
    if (m_descriptorPool) vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);
}

// ─── Buffer creation helpers ───

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

void Renderer::createFullscreenQuad() {
    VkDeviceSize vertexSize = sizeof(QuadVertex) * fsQuadVertices.size();
    VkDeviceSize indexSize = sizeof(uint16_t) * fsQuadIndices.size();
    m_fsIndexCount = static_cast<uint32_t>(fsQuadIndices.size());

    m_engine->createBuffer(vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_fsVertexBuffer, m_fsVertexBufferMemory);
    void* data;
    vkMapMemory(m_engine->device(), m_fsVertexBufferMemory, 0, vertexSize, 0, &data);
    memcpy(data, fsQuadVertices.data(), vertexSize);
    vkUnmapMemory(m_engine->device(), m_fsVertexBufferMemory);

    m_engine->createBuffer(indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_fsIndexBuffer, m_fsIndexBufferMemory);
    vkMapMemory(m_engine->device(), m_fsIndexBufferMemory, 0, indexSize, 0, &data);
    memcpy(data, fsQuadIndices.data(), indexSize);
    vkUnmapMemory(m_engine->device(), m_fsIndexBufferMemory);
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
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 64;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 64;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 128;

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

// ─── Frame lifecycle ───

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
    float viewSize = m_engine->getViewSize();
    float left = -aspect * viewSize;
    float right = aspect * viewSize;

    glm::vec2 camPos = m_engine->cameraPos();
    UniformBufferObject ubo{};
    glm::mat4 proj = glm::ortho(left, right, -viewSize, viewSize, -1.0f, 1.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-camPos, 0.0f));
    ubo.projection = proj * view;
    memcpy(m_uniformBufferMapped, &ubo, sizeof(ubo));

    m_currentCommandBuffer = m_engine->commandBuffer(m_imageIndex);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(m_currentCommandBuffer, &beginInfo);
    return true;
}

void Renderer::cmdBeginScenePass() {
    VkExtent2D extent = m_engine->swapChainExtent();

    VkClearValue clearValues[2];
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = m_sceneRenderPass;
    info.framebuffer = m_offscreenResources[m_imageIndex].sceneFramebuffer;
    info.renderArea.offset = { 0, 0 };
    info.renderArea.extent = extent;
    info.clearValueCount = 2;
    info.pClearValues = clearValues;

    vkCmdBeginRenderPass(m_currentCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

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
}

void Renderer::cmdBeginLightPass(const glm::vec3& ambientColor, float ambientIntensity) {
    vkCmdEndRenderPass(m_currentCommandBuffer);

    VkExtent2D extent = m_engine->swapChainExtent();

    glm::vec3 ambient = ambientColor * ambientIntensity;
    VkClearValue clearValue;
    clearValue.color = { {ambient.r, ambient.g, ambient.b, 1.0f} };

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = m_lightRenderPass;
    info.framebuffer = m_offscreenResources[m_imageIndex].lightFramebuffer;
    info.renderArea.offset = { 0, 0 };
    info.renderArea.extent = extent;
    info.clearValueCount = 1;
    info.pClearValues = &clearValue;

    vkCmdBeginRenderPass(m_currentCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

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
}

void Renderer::cmdComposite() {
    vkCmdEndRenderPass(m_currentCommandBuffer);

    VkExtent2D extent = m_engine->swapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = m_engine->renderPass();
    info.framebuffer = m_engine->framebuffer(m_imageIndex);
    info.renderArea.offset = { 0, 0 };
    info.renderArea.extent = extent;
    info.clearValueCount = static_cast<uint32_t>(clearValues.size());
    info.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_currentCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

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

    vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline);

    auto& res = m_offscreenResources[m_imageIndex];
    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_compositePipelineLayout, 0, 1, &res.compositeDescSet, 0, nullptr);

    VkBuffer vertexBuffers[] = { m_fsVertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, m_fsIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(m_currentCommandBuffer, m_fsIndexCount, 1, 0, 0, 0);
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

// ─── Draw calls ───

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

void Renderer::drawTilemap(VkDescriptorSet descriptorSet, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount) {
    glm::mat4 model(1.0f);
    SpritePushConstants push{};
    push.model = model;
    vkCmdPushConstants(m_currentCommandBuffer, m_engine->pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SpritePushConstants), &push);

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_engine->pipelineLayout(), 1, 1, &descriptorSet, 0, nullptr);

    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(m_currentCommandBuffer, indexCount, 1, 0, 0, 0);

    VkBuffer defaultVertexBuffers[] = { m_vertexBuffer };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, defaultVertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
}

void Renderer::drawLight(const glm::vec2& position, const glm::vec2& scale, const glm::vec4& color) {
    vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightPipeline);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
    model = glm::scale(model, glm::vec3(scale, 1.0f));

    LightPushConstants push{};
    push.model = model;
    push.color = color;

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_lightPipelineLayout, 0, 1, &m_uniformDescriptorSet, 0, nullptr);

    vkCmdPushConstants(m_currentCommandBuffer, m_lightPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(LightPushConstants), &push);

    VkBuffer vertexBuffers[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

void Renderer::drawShadow(float cx, float cy, float sx, float sy) {
    vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(cx, cy, 0.0f));
    model = glm::scale(model, glm::vec3(sx, sy, 1.0f));

    SpritePushConstants push{};
    push.model = model;

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_shadowPipelineLayout, 0, 1, &m_uniformDescriptorSet, 0, nullptr);

    vkCmdPushConstants(m_currentCommandBuffer, m_shadowPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SpritePushConstants), &push);

    VkBuffer vertexBuffers[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

// ─── Lighting resource management ───

void Renderer::createLightingResources() {
    createOffscreenRenderPasses();
    createOffscreenFramebuffers();
    createOffscreenSampler();
    createCompositeDescriptorSetLayout();
    createCompositeDescriptorSets();
    createLightPipeline();
    createShadowPipeline();
    createCompositePipeline();
}

void Renderer::destroyLightingResources() {
    VkDevice dev = m_engine->device();
    destroyOffscreenFramebuffers();
    if (m_compositePipeline) { vkDestroyPipeline(dev, m_compositePipeline, nullptr); m_compositePipeline = VK_NULL_HANDLE; }
    if (m_compositePipelineLayout) { vkDestroyPipelineLayout(dev, m_compositePipelineLayout, nullptr); m_compositePipelineLayout = VK_NULL_HANDLE; }
    if (m_compositeDescSetLayout) { vkDestroyDescriptorSetLayout(dev, m_compositeDescSetLayout, nullptr); m_compositeDescSetLayout = VK_NULL_HANDLE; }
    if (m_shadowPipeline) { vkDestroyPipeline(dev, m_shadowPipeline, nullptr); m_shadowPipeline = VK_NULL_HANDLE; }
    if (m_shadowPipelineLayout) { vkDestroyPipelineLayout(dev, m_shadowPipelineLayout, nullptr); m_shadowPipelineLayout = VK_NULL_HANDLE; }
    if (m_lightPipeline) { vkDestroyPipeline(dev, m_lightPipeline, nullptr); m_lightPipeline = VK_NULL_HANDLE; }
    if (m_lightPipelineLayout) { vkDestroyPipelineLayout(dev, m_lightPipelineLayout, nullptr); m_lightPipelineLayout = VK_NULL_HANDLE; }
    if (m_offscreenSampler) { vkDestroySampler(dev, m_offscreenSampler, nullptr); m_offscreenSampler = VK_NULL_HANDLE; }
    if (m_sceneRenderPass) { vkDestroyRenderPass(dev, m_sceneRenderPass, nullptr); m_sceneRenderPass = VK_NULL_HANDLE; }
    if (m_lightRenderPass) { vkDestroyRenderPass(dev, m_lightRenderPass, nullptr); m_lightRenderPass = VK_NULL_HANDLE; }
}

void Renderer::destroyAttachment(OffscreenAttachment& att) {
    VkDevice dev = m_engine->device();
    if (att.view) { vkDestroyImageView(dev, att.view, nullptr); att.view = VK_NULL_HANDLE; }
    if (att.image) { vkDestroyImage(dev, att.image, nullptr); att.image = VK_NULL_HANDLE; }
    if (att.memory) { vkFreeMemory(dev, att.memory, nullptr); att.memory = VK_NULL_HANDLE; }
}

void Renderer::createOffscreenRenderPasses() {
    VkDevice dev = m_engine->device();

    // Scene render pass: color + depth
    VkAttachmentDescription sceneColorAtt{};
    sceneColorAtt.format = m_engine->swapChainImageFormat();
    sceneColorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    sceneColorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    sceneColorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    sceneColorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    sceneColorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    sceneColorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sceneColorAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription sceneDepthAtt{};
    sceneDepthAtt.format = m_engine->depthFormat();
    sceneDepthAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    sceneDepthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    sceneDepthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    sceneDepthAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    sceneDepthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    sceneDepthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sceneDepthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference sceneColorRef{};
    sceneColorRef.attachment = 0;
    sceneColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference sceneDepthRef{};
    sceneDepthRef.attachment = 1;
    sceneDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sceneSubpass{};
    sceneSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sceneSubpass.colorAttachmentCount = 1;
    sceneSubpass.pColorAttachments = &sceneColorRef;
    sceneSubpass.pDepthStencilAttachment = &sceneDepthRef;

    std::array<VkSubpassDependency, 2> sceneDeps{};
    sceneDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    sceneDeps[0].dstSubpass = 0;
    sceneDeps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sceneDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sceneDeps[0].srcAccessMask = 0;
    sceneDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sceneDeps[1].srcSubpass = 0;
    sceneDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    sceneDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    sceneDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    sceneDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    sceneDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 2> sceneAtts = { sceneColorAtt, sceneDepthAtt };
    VkRenderPassCreateInfo sceneRP{};
    sceneRP.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    sceneRP.attachmentCount = static_cast<uint32_t>(sceneAtts.size());
    sceneRP.pAttachments = sceneAtts.data();
    sceneRP.subpassCount = 1;
    sceneRP.pSubpasses = &sceneSubpass;
    sceneRP.dependencyCount = static_cast<uint32_t>(sceneDeps.size());
    sceneRP.pDependencies = sceneDeps.data();

    if (vkCreateRenderPass(dev, &sceneRP, nullptr, &m_sceneRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create scene render pass");
    }

    // Light render pass: color only
    VkAttachmentDescription lightColorAtt{};
    lightColorAtt.format = m_engine->swapChainImageFormat();
    lightColorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    lightColorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    lightColorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    lightColorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    lightColorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    lightColorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    lightColorAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference lightColorRef{};
    lightColorRef.attachment = 0;
    lightColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription lightSubpass{};
    lightSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lightSubpass.colorAttachmentCount = 1;
    lightSubpass.pColorAttachments = &lightColorRef;

    std::array<VkSubpassDependency, 2> lightDeps{};
    lightDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    lightDeps[0].dstSubpass = 0;
    lightDeps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    lightDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    lightDeps[0].srcAccessMask = 0;
    lightDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    lightDeps[1].srcSubpass = 0;
    lightDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    lightDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    lightDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    lightDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    lightDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo lightRP{};
    lightRP.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    lightRP.attachmentCount = 1;
    lightRP.pAttachments = &lightColorAtt;
    lightRP.subpassCount = 1;
    lightRP.pSubpasses = &lightSubpass;
    lightRP.dependencyCount = static_cast<uint32_t>(lightDeps.size());
    lightRP.pDependencies = lightDeps.data();

    if (vkCreateRenderPass(dev, &lightRP, nullptr, &m_lightRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create light render pass");
    }
}

void Renderer::createOffscreenFramebuffers() {
    VkDevice dev = m_engine->device();
    VkExtent2D extent = m_engine->swapChainExtent();
    VkFormat colorFormat = m_engine->swapChainImageFormat();
    VkFormat depthFormat = m_engine->depthFormat();
    uint32_t count = static_cast<uint32_t>(m_engine->swapChainImageCount());

    m_offscreenResources.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        auto& res = m_offscreenResources[i];

        // Scene color attachment
        m_engine->createImage(extent.width, extent.height, colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            res.sceneColor.image, res.sceneColor.memory);
        res.sceneColor.view = m_engine->createImageView(res.sceneColor.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // Scene depth attachment
        m_engine->createImage(extent.width, extent.height, depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            res.sceneDepth.image, res.sceneDepth.memory);
        res.sceneDepth.view = m_engine->createImageView(res.sceneDepth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        // Scene framebuffer
        std::array<VkImageView, 2> sceneAtts = { res.sceneColor.view, res.sceneDepth.view };
        VkFramebufferCreateInfo sceneFB{};
        sceneFB.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        sceneFB.renderPass = m_sceneRenderPass;
        sceneFB.attachmentCount = static_cast<uint32_t>(sceneAtts.size());
        sceneFB.pAttachments = sceneAtts.data();
        sceneFB.width = extent.width;
        sceneFB.height = extent.height;
        sceneFB.layers = 1;
        if (vkCreateFramebuffer(dev, &sceneFB, nullptr, &res.sceneFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create scene framebuffer");
        }

        // Light color attachment
        m_engine->createImage(extent.width, extent.height, colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            res.lightColor.image, res.lightColor.memory);
        res.lightColor.view = m_engine->createImageView(res.lightColor.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // Light framebuffer
        VkFramebufferCreateInfo lightFB{};
        lightFB.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        lightFB.renderPass = m_lightRenderPass;
        lightFB.attachmentCount = 1;
        lightFB.pAttachments = &res.lightColor.view;
        lightFB.width = extent.width;
        lightFB.height = extent.height;
        lightFB.layers = 1;
        if (vkCreateFramebuffer(dev, &lightFB, nullptr, &res.lightFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create light framebuffer");
        }
    }
}

void Renderer::destroyOffscreenFramebuffers() {
    VkDevice dev = m_engine->device();
    for (auto& res : m_offscreenResources) {
        if (res.sceneFramebuffer) { vkDestroyFramebuffer(dev, res.sceneFramebuffer, nullptr); res.sceneFramebuffer = VK_NULL_HANDLE; }
        if (res.lightFramebuffer) { vkDestroyFramebuffer(dev, res.lightFramebuffer, nullptr); res.lightFramebuffer = VK_NULL_HANDLE; }
        if (res.compositeDescSet) { /* don't free - pool manages */ res.compositeDescSet = VK_NULL_HANDLE; }
        destroyAttachment(res.lightColor);
        destroyAttachment(res.sceneDepth);
        destroyAttachment(res.sceneColor);
    }
    m_offscreenResources.clear();
}

void Renderer::createOffscreenSampler() {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.anisotropyEnable = VK_FALSE;
    info.maxAnisotropy = 1.0f;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_engine->device(), &info, nullptr, &m_offscreenSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen sampler");
    }
}

void Renderer::createCompositeDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_engine->device(), &layoutInfo, nullptr, &m_compositeDescSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create composite descriptor set layout");
    }
}

void Renderer::createCompositeDescriptorSets() {
    VkDevice dev = m_engine->device();
    uint32_t count = static_cast<uint32_t>(m_offscreenResources.size());

    for (uint32_t i = 0; i < count; ++i) {
        auto& res = m_offscreenResources[i];

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_compositeDescSetLayout;

        if (vkAllocateDescriptorSets(dev, &allocInfo, &res.compositeDescSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate composite descriptor set");
        }

        std::array<VkDescriptorImageInfo, 2> imageInfos{};
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = res.sceneColor.view;
        imageInfos[0].sampler = m_offscreenSampler;

        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = res.lightColor.view;
        imageInfos[1].sampler = m_offscreenSampler;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = res.compositeDescSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &imageInfos[0];

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = res.compositeDescSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &imageInfos[1];

        vkUpdateDescriptorSets(dev, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void Renderer::createLightPipeline() {
    VkDevice dev = m_engine->device();

    auto vertCode = readFile("shaders/light_vert.spv");
    auto fragCode = readFile("shaders/light_frag.spv");
    VkShaderModule vertMod = m_engine->createShaderModule(vertCode);
    VkShaderModule fragMod = m_engine->createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    auto bindingDesc = QuadVertex::getBindingDescription();
    auto attributeDesc = QuadVertex::getAttributeDescriptions();
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
    vertexInput.pVertexAttributeDescriptions = attributeDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blend;

    std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dynamic.pDynamicStates = dynStates.data();

    // Pipeline layout: set 0 = UBO, push constant = model + color
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(LightPushConstants);

    VkDescriptorSetLayout setLayouts[] = { m_engine->descriptorSetLayout() };
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_lightPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create light pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = m_lightPipelineLayout;
    pipelineInfo.renderPass = m_lightRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_lightPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create light pipeline");
    }

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);
}

void Renderer::createShadowPipeline() {
    VkDevice dev = m_engine->device();

    auto vertCode = readFile("shaders/shadow_vert.spv");
    auto fragCode = readFile("shaders/shadow_frag.spv");
    VkShaderModule vertMod = m_engine->createShaderModule(vertCode);
    VkShaderModule fragMod = m_engine->createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    auto bindingDesc = QuadVertex::getBindingDescription();
    auto attributeDesc = QuadVertex::getAttributeDescriptions();
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
    vertexInput.pVertexAttributeDescriptions = attributeDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blend;

    std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dynamic.pDynamicStates = dynStates.data();

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(SpritePushConstants);

    VkDescriptorSetLayout setLayouts[] = { m_engine->descriptorSetLayout() };
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_shadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = m_shadowPipelineLayout;
    pipelineInfo.renderPass = m_lightRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_shadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline");
    }

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);
}

void Renderer::createCompositePipeline() {
    VkDevice dev = m_engine->device();

    auto vertCode = readFile("shaders/composite_vert.spv");
    auto fragCode = readFile("shaders/composite_frag.spv");
    VkShaderModule vertMod = m_engine->createShaderModule(vertCode);
    VkShaderModule fragMod = m_engine->createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    auto bindingDesc = QuadVertex::getBindingDescription();
    auto attributeDesc = QuadVertex::getAttributeDescriptions();
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
    vertexInput.pVertexAttributeDescriptions = attributeDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_FALSE;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blend;

    std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dynamic.pDynamicStates = dynStates.data();

    VkDescriptorSetLayout setLayouts[] = { m_compositeDescSetLayout };
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_compositePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create composite pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = m_compositePipelineLayout;
    pipelineInfo.renderPass = m_engine->renderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_compositePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create composite pipeline");
    }

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);
}
