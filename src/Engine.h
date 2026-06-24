#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <optional>
#include <array>
#include <chrono>

class Renderer;
class Player;
class Npc;
class Map;
class MapManager;
class Texture;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view;
    alignas(16) glm::vec4 lightPos;   // xyz = world position, w = falloff radius
    alignas(16) glm::vec4 lightColor; // rgb = color, a = ambient factor (0..1)
};

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

class Engine {
public:
    Engine();
    ~Engine();

    void run();

    VkDevice device() const { return m_device; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkCommandPool commandPool() const { return m_commandPool; }
    VkDescriptorSetLayout& descriptorSetLayout() { return m_descriptorSetLayout; }
    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkPipeline graphicsPipeline() const { return m_graphicsPipeline; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkExtent2D swapChainExtent() const { return m_swapChainExtent; }
    GLFWwindow* window() const { return m_window; }
    VkSwapchainKHR swapChain() const { return m_swapChain; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    VkSemaphore imageAvailableSemaphore() const { return m_imageAvailableSemaphores[m_currentFrame]; }
    VkSemaphore renderFinishedSemaphore() const { return m_renderFinishedSemaphores[m_currentFrame]; }
    VkFence& inFlightFence() { return m_inFlightFences[m_currentFrame]; }
    VkCommandBuffer commandBuffer(uint32_t imageIndex) const { return m_commandBuffers[imageIndex]; }
    VkFramebuffer framebuffer(uint32_t imageIndex) const { return m_swapChainFramebuffers[imageIndex]; }
    Renderer* renderer() const { return m_renderer; }
    const glm::mat4& viewMatrix() const { return m_viewMatrix; }
    const glm::mat4& projMatrix() const { return m_projMatrix; }
    glm::vec3 cameraPosition() const { return m_camEye; }
    float cameraYaw() const { return m_camYaw; }
    Texture* shadowTexture() const { return m_shadowTexture; }
    Texture* candleTexture() const { return m_candleTexture; }
    Npc* npc() const { return m_npc; }
    const glm::vec4& lightPos() const { return m_lightPos; }
    const glm::vec4& lightColor() const { return m_lightColor; }
    void setLightPos(const glm::vec4& p) { m_lightPos = p; }
    void setLightColor(const glm::vec4& c) { m_lightColor = c; }
    void recreateSwapChain();
    void waitIdle() const { vkDeviceWaitIdle(m_device); }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    GLFWwindow* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;

    // 3D camera
    float m_camYaw = 0.0f;
    float m_camPitch = 25.0f;
    float m_camDistance = 500.0f;
    glm::vec3 m_camTarget{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_camEye{ 0.0f, 0.0f, 0.0f };
    glm::mat4 m_viewMatrix{ 1.0f };
    glm::mat4 m_projMatrix{ 1.0f };
    double m_lastMouseX = 0.0, m_lastMouseY = 0.0;
    bool m_mouseDown = false;
    bool m_menuClickPending = false;
    bool m_rightMouseDown = false;
    bool m_draggingBillboard = false;

    float m_mapWorldWidth = 0.0f;
    float m_mapWorldHeight = 0.0f;
    bool m_showCollisions = false;
    bool m_showMap = false;
    bool m_editMode = false;
    int m_savedFlashFrames = 0;

    Texture* m_playerDotTexture = nullptr;
    Texture* m_shadowTexture = nullptr;
    Texture* m_candleTexture = nullptr;
    glm::vec4 m_lightPos{0.0f, 200.0f, 0.0f, 600.0f};   // front yard default
    glm::vec4 m_lightColor{1.0f, 1.0f, 1.0f, 0.35f};    // white, ambient 0.35

    // Main menu state
    bool m_showMenu = true;
    int m_menuSelection = 0;
    float m_menuScrollAccum = 0.0f;
    float m_menuAnimTimer = 0.0f;
    bool m_menuInputEnabled = false;
    static constexpr int MENU_OPTION_COUNT = 2;
    Texture* m_menuTitleTexture = nullptr;
    Texture* m_menuOptionTextures[MENU_OPTION_COUNT] = { nullptr, nullptr };
    Texture* m_menuCursorTexture = nullptr;
    Texture* m_menuBorderTexture = nullptr;
    Texture* m_menuPanelTexture = nullptr;
    Texture* m_menuSubtitleTexture = nullptr;
    Texture* m_menuBlackOverlay = nullptr;
    void loadMenuTextures();
    void destroyMenuTextures();
    void handleMenuInput();
    void renderMenu();

    Renderer* m_renderer = nullptr;
    Player* m_player = nullptr;
    Npc* m_npc = nullptr;
    MapManager* m_mapManager = nullptr;

    std::chrono::high_resolution_clock::time_point m_lastTime;

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void updateCamera();
    void renderCollisionDebug(Map* map, Player* player);
    void renderMapOverlay(Map* map, Player* player);
    void clickPickBillboard();
    void setWindowShouldClose() { m_framebufferResized = true; glfwSetWindowShouldClose(m_window, GLFW_TRUE); }

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createDepthResources();
    void createCommandBuffers();
    void createSyncObjects();

    void cleanupSwapChain();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    static std::vector<const char*> getRequiredExtensions();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool checkValidationLayerSupport();
public:
    VkShaderModule createShaderModule(const std::vector<char>& code);
private:
};

std::vector<char> readFile(const std::string& filename);
