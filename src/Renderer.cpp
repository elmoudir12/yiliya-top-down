#include "Renderer.h"
#include "Engine.h"
#include <stdexcept>
#include <array>
#include <cstring>

static const std::vector<QuadVertex> quadVertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}},
};

static const std::vector<uint16_t> quadIndices = { 0, 1, 2, 2, 3, 0 };

// Unit box wireframe: 12 edges (24 vertices), one line per edge
static const std::vector<QuadVertex> boxLineVerts = {
    // bottom face (y = -0.5)
    {{-0.5f, -0.5f, -0.5f}, {0,0}}, {{ 0.5f, -0.5f, -0.5f}, {0,0}},
    {{ 0.5f, -0.5f, -0.5f}, {0,0}}, {{ 0.5f, -0.5f,  0.5f}, {0,0}},
    {{ 0.5f, -0.5f,  0.5f}, {0,0}}, {{-0.5f, -0.5f,  0.5f}, {0,0}},
    {{-0.5f, -0.5f,  0.5f}, {0,0}}, {{-0.5f, -0.5f, -0.5f}, {0,0}},
    // top face (y = +0.5)
    {{-0.5f,  0.5f, -0.5f}, {0,0}}, {{ 0.5f,  0.5f, -0.5f}, {0,0}},
    {{ 0.5f,  0.5f, -0.5f}, {0,0}}, {{ 0.5f,  0.5f,  0.5f}, {0,0}},
    {{ 0.5f,  0.5f,  0.5f}, {0,0}}, {{-0.5f,  0.5f,  0.5f}, {0,0}},
    {{-0.5f,  0.5f,  0.5f}, {0,0}}, {{-0.5f,  0.5f, -0.5f}, {0,0}},
    // vertical edges
    {{-0.5f, -0.5f, -0.5f}, {0,0}}, {{-0.5f,  0.5f, -0.5f}, {0,0}},
    {{ 0.5f, -0.5f, -0.5f}, {0,0}}, {{ 0.5f,  0.5f, -0.5f}, {0,0}},
    {{ 0.5f, -0.5f,  0.5f}, {0,0}}, {{ 0.5f,  0.5f,  0.5f}, {0,0}},
    {{-0.5f, -0.5f,  0.5f}, {0,0}}, {{-0.5f,  0.5f,  0.5f}, {0,0}},
};

Renderer::Renderer(Engine* engine)
    : m_engine(engine) {
    createVertexBuffer();
    createIndexBuffer();
    createBoxLineBuffer();
    createDescriptorPool();
    createTextureDescriptorSetLayout();
    createUniformBuffer();
    createUniformDescriptorSet();
    createDebugPipeline();
    createBoxDebugPipeline();
}

Renderer::~Renderer() {
    VkDevice dev = m_engine->device();
    if (m_boxDebugPipeline) vkDestroyPipeline(dev, m_boxDebugPipeline, nullptr);
    if (m_boxLineBuffer) vkDestroyBuffer(dev, m_boxLineBuffer, nullptr);
    if (m_boxLineBufferMemory) vkFreeMemory(dev, m_boxLineBufferMemory, nullptr);
    if (m_debugPipeline) vkDestroyPipeline(dev, m_debugPipeline, nullptr);
    if (m_debugPipelineLayout) vkDestroyPipelineLayout(dev, m_debugPipelineLayout, nullptr);
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

void Renderer::createBoxLineBuffer() {
    VkDeviceSize bufferSize = sizeof(QuadVertex) * boxLineVerts.size();
    m_engine->createBuffer(bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_boxLineBuffer, m_boxLineBufferMemory);

    void* data;
    vkMapMemory(m_engine->device(), m_boxLineBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, boxLineVerts.data(), bufferSize);
    vkUnmapMemory(m_engine->device(), m_boxLineBufferMemory);
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

    UniformBufferObject ubo{};
    ubo.projection = m_engine->projMatrix();
    ubo.view = m_engine->viewMatrix();
    ubo.lightPos = m_engine->lightPos();
    ubo.lightColor = m_engine->lightColor();
    memcpy(m_uniformBufferMapped, &ubo, sizeof(ubo));

    VkExtent2D extent = m_engine->swapChainExtent();

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
    clearValues[0].color = { {m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]} };
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
    model = glm::rotate(model, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(scale, 1.0f));

    SpritePushConstants push{};
    push.model = model;
    vkCmdPushConstants(m_currentCommandBuffer, m_engine->pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpritePushConstants), &push);

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_engine->pipelineLayout(), 1, 1, &descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

void Renderer::drawSprite3D(VkDescriptorSet descriptorSet, const glm::mat4& model, const glm::vec4& color) {
    SpritePushConstants push{};
    push.model = model;
    push.color = color;
    vkCmdPushConstants(m_currentCommandBuffer, m_engine->pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpritePushConstants), &push);

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_engine->pipelineLayout(), 1, 1, &descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

void Renderer::drawTilemap(VkDescriptorSet descriptorSet, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount) {
    glm::mat4 model(1.0f);
    SpritePushConstants push{};
    push.model = model;
    vkCmdPushConstants(m_currentCommandBuffer, m_engine->pipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpritePushConstants), &push);

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

void Renderer::drawDebugRect(const glm::vec3& position, const glm::vec2& scale, const glm::vec4& color) {
    vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);

    // Place rect on the XZ floor plane: rotate the XY quad 90° around X axis
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale, 1.0f));

    DebugPushConstants push{};
    push.model = model;
    push.color = color;

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_debugPipelineLayout, 0, 1, &m_uniformDescriptorSet, 0, nullptr);

    vkCmdPushConstants(m_currentCommandBuffer, m_debugPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(DebugPushConstants), &push);

    VkBuffer vertexBuffers[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

void Renderer::drawDebugBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color) {
    vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_boxDebugPipeline);

    glm::vec3 center = (min + max) * 0.5f;
    glm::vec3 scale = max - min;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, scale);

    DebugPushConstants push{};
    push.model = model;
    push.color = color;

    vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_debugPipelineLayout, 0, 1, &m_uniformDescriptorSet, 0, nullptr);

    vkCmdPushConstants(m_currentCommandBuffer, m_debugPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(DebugPushConstants), &push);

    VkBuffer vertexBuffers[] = { m_boxLineBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdDraw(m_currentCommandBuffer, static_cast<uint32_t>(boxLineVerts.size()), 1, 0, 0);
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

void Renderer::setClearColor(float r, float g, float b) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
}

void Renderer::createDebugPipeline() {
    VkDevice dev = m_engine->device();

    auto vertCode = readFile("shaders/debug_vert.spv");
    auto fragCode = readFile("shaders/debug_frag.spv");
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
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(DebugPushConstants);

    VkDescriptorSetLayout setLayouts[] = { m_engine->descriptorSetLayout() };
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_debugPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug pipeline layout");
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
    pipelineInfo.layout = m_debugPipelineLayout;
    pipelineInfo.renderPass = m_engine->renderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_debugPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug pipeline");
    }

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);
}

void Renderer::createBoxDebugPipeline() {
    VkDevice dev = m_engine->device();

    auto vertCode = readFile("shaders/debug_vert.spv");
    auto fragCode = readFile("shaders/debug_frag.spv");
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
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 2.0f;
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
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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
    pipelineInfo.layout = m_debugPipelineLayout;
    pipelineInfo.renderPass = m_engine->renderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_boxDebugPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create box debug pipeline");
    }

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);
}
