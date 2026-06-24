#include "Engine.h"
#include "Renderer.h"
#include "Player.h"
#include "Map.h"
#include "MapManager.h"
#include "Npc.h"
#include "Texture.h"
#include "Font.h"
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <algorithm>
#include <limits>

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const bool ENABLE_VALIDATION_LAYERS = false;

Engine::Engine() {
    m_lastTime = std::chrono::high_resolution_clock::now();
}

Engine::~Engine() {
    cleanup();
}

void Engine::run() {
    initWindow();
    initVulkan();
    mainLoop();
}

void Engine::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Behest", nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        auto* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        if (engine) engine->m_framebufferResized = true;
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
        auto* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        if (!engine) return;

        // Billboard dragging in edit mode
        Map* curMap = engine->m_mapManager ? engine->m_mapManager->currentMap() : nullptr;
        if (engine->m_draggingBillboard && engine->m_editMode && curMap && curMap->selectedBillboard() >= 0) {
            double dx = x - engine->m_lastMouseX;
            double dy = y - engine->m_lastMouseY;
            engine->m_lastMouseX = x;
            engine->m_lastMouseY = y;

            float yawRad = glm::radians(engine->m_camYaw);
            glm::vec3 rightDir(std::cos(yawRad), 0.0f, -std::sin(yawRad));
            glm::vec3 fwdDir(std::sin(yawRad), 0.0f, std::cos(yawRad));
            float sensitivity = 0.5f;
            glm::vec3 delta = (rightDir * (float)dx + fwdDir * (float)dy) * sensitivity;

            int sel = curMap->selectedBillboard();
            if (sel >= 0) {
                glm::vec3 pos = curMap->billboardPosition(sel);
                curMap->setBillboardPosition(sel, pos + delta);
            }
            return;
        }

        if (engine->m_mouseDown || engine->m_rightMouseDown) {
            double dx = x - engine->m_lastMouseX;
            double dy = y - engine->m_lastMouseY;
            engine->m_camYaw -= static_cast<float>(dx * 0.3);
            engine->m_camPitch += static_cast<float>(dy * 0.3);
            engine->m_camPitch = std::clamp(engine->m_camPitch, -89.0f, 89.0f);
        }
        engine->m_lastMouseX = x;
        engine->m_lastMouseY = y;
    });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
        auto* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        if (!engine) return;
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            engine->m_mouseDown = (action == GLFW_PRESS);
            if (action == GLFW_PRESS && engine->m_showMenu) {
                engine->m_menuClickPending = true;
            }
            if (action == GLFW_PRESS && engine->m_editMode) {
                engine->clickPickBillboard();
                Map* map = engine->m_mapManager ? engine->m_mapManager->currentMap() : nullptr;
                if (map && map->selectedBillboard() >= 0)
                    engine->m_draggingBillboard = true;
            }
            if (action == GLFW_RELEASE)
                engine->m_draggingBillboard = false;
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            engine->m_rightMouseDown = (action == GLFW_PRESS);
        }
    });
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double x, double y) {
        auto* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        if (!engine) return;
        if (engine->m_showMenu) {
            // While menu is open, scroll changes selection
            engine->m_menuScrollAccum += static_cast<float>(y);
        } else {
            engine->m_camDistance -= static_cast<float>(y * 20.0f);
            engine->m_camDistance = std::clamp(engine->m_camDistance, 100.0f, 1500.0f);
        }
    });
}

void Engine::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createCommandPool();
    m_renderer = new Renderer(this);
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();

    m_player = new Player(this, m_renderer);
    m_mapManager = new MapManager(this, m_renderer, m_player);
    m_mapManager->loadMap("player_house");

    m_npc = new Npc(this, m_renderer, "assets/nort");
    m_npc->setPosition(glm::vec3(-110.0f, 0.0f, -140.0f)); // near the bed in player_house

    // Player dot texture for map overlay
    {
        const int S = 8;
        std::vector<uint8_t> p(S * S * 4, 0);
        int cx = S/2, cy = S/2;
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                float dx = (float)(x - cx), dy = (float)(y - cy);
                float dist = sqrtf(dx*dx + dy*dy) / (float)(S/2);
                if (dist < 1.0f) {
                    float a = 1.0f - dist * dist;
                    p[(y*S+x)*4+0] = 240;
                    p[(y*S+x)*4+1] = 40 + (uint8_t)(40 * (1.0f - dist));
                    p[(y*S+x)*4+2] = 40 + (uint8_t)(40 * (1.0f - dist));
                    p[(y*S+x)*4+3] = (uint8_t)(255 * a);
                }
            }
        }
        m_playerDotTexture = new Texture(this, p.data(), S, S);
    }

    // Shadow blob texture (soft radial gradient for projected shadows)
    {
        const int S = 64;
        std::vector<uint8_t> p(S * S * 4, 0);
        int cx = S/2, cy = S/2;
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                float dx = (float)(x - cx) / (float)(S/2);
                float dy = (float)(y - cy) / (float)(S/2);
                float dist = sqrtf(dx*dx + dy*dy);
                float a = 1.0f - dist;
                a = a < 0.0f ? 0.0f : a * a * 0.4f;
                p[(y*S+x)*4+0] = 0;
                p[(y*S+x)*4+1] = 0;
                p[(y*S+x)*4+2] = 0;
                p[(y*S+x)*4+3] = (uint8_t)(a * 255.0f);
            }
        }
        m_shadowTexture = new Texture(this, p.data(), S, S);
    }

    loadMenuTextures();
}

void Engine::renderCollisionDebug(Map* map, Player* player) {
    float hw = map->width() * map->tileSize() * 0.5f;
    float hh = map->height() * map->tileSize() * 0.5f;
    float wh = map->wallHeight();

    // 3D wall collision volumes (green) — full wireframe box
    glm::vec4 wallCol(0.0f, 0.8f, 0.0f, 1.0f);
    for (auto& r : map->wallCollisionRects()) {
        float wx0 = r.x - hw;
        float wz0 = r.y - hh;
        float wx1 = wx0 + r.w;
        float wz1 = wz0 + r.h;
        m_renderer->drawDebugBox({wx0, 0.0f, wz0}, {wx1, wh, wz1}, wallCol);
    }

    // TMX object-layer collision rects (blue) — floor only
    glm::vec4 objCol(0.0f, 0.0f, 0.8f, 1.0f);
    size_t wallCount = map->wallCollisionRects().size();
    auto& allRects = map->collisionRects();
    for (size_t i = 0; i + wallCount < allRects.size(); ++i) {
        auto& r = allRects[i];
        float cx = r.x - hw + r.w * 0.5f;
        float cz = r.y - hh + r.h * 0.5f;
        m_renderer->drawDebugRect({cx, 0.1f, cz}, {r.w, r.h}, objCol);
    }

    // Player pixel-perfect collision volume (yellow) from current sprite frame
    auto vb = player->visibleBounds3D();
    glm::vec4 playerCol(1.0f, 0.8f, 0.0f, 1.0f);
    m_renderer->drawDebugBox({vb.x, 0.0f, vb.y}, {vb.z, 64.0f, vb.w}, playerCol);

    // Transition/exit zones (red) — floor rect + wireframe box
    glm::vec4 exitCol(1.0f, 0.2f, 0.2f, 1.0f);
    float ts = map->tileSize();
    for (auto& t : map->transitions()) {
        float wx0 = t.tileX * ts - hw;
        float wz0 = t.tileY * ts - hh;
        float wx1 = (t.tileX + t.tileW) * ts - hw;
        float wz1 = (t.tileY + t.tileH) * ts - hh;
        float cx = (wx0 + wx1) * 0.5f;
        float cz = (wz0 + wz1) * 0.5f;
        float w = wx1 - wx0;
        float d = wz1 - wz0;
        m_renderer->drawDebugRect({cx, 0.05f, cz}, {w, d}, exitCol);
        m_renderer->drawDebugBox({wx0, 0.0f, wz0}, {wx1, 48.0f, wz1}, exitCol);
    }
}

void Engine::renderMapOverlay(Map* map, Player* player) {
    Texture* mapTex = map->mapOverlayTexture();
    if (!mapTex) return;

    int gW = map->globalPixW(), gH = map->globalPixH();
    if (gW == 0 || gH == 0) return;
    float mapAspect = (float)gW / (float)gH;

    // Map quad size in ortho space (screen is [-asp, asp] × [-1, 1])
    float fill = 0.75f;
    float qw, qh;
    if (mapAspect > 1.0f) {
        qw = fill * 2.0f;
        qh = qw / mapAspect;
    } else {
        qh = fill * 2.0f;
        qw = qh * mapAspect;
    }

    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(qw, -qh, 1));
    m_renderer->drawSprite3D(mapTex->descriptorSet(), model);

    // Player marker — convert 3D world pos to global tile coords, then to ortho UV
    if (m_playerDotTexture && player) {
        glm::vec3 pos = player->position();
        float tileSize = (float)map->tileSize();
        float hw = map->width() * tileSize * 0.5f;
        float hh = map->height() * tileSize * 0.5f;
        float localTileX = (pos.x + hw) / tileSize;
        float localTileY = (pos.z + hh) / tileSize;
        float globalTileX = (float)map->curWorldX() + localTileX;
        float globalTileY = (float)map->curWorldY() + localTileY;

        // Convert to UV on the global map texture
        float originX = (float)map->globalOriginX();
        float originY = (float)map->globalOriginY();
        float u = (globalTileX - originX) * Map::mapPixPerTile() / (float)gW;
        float v = (globalTileY - originY) * Map::mapPixPerTile() / (float)gH;
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float ox = (u - 0.5f) * qw;
        float oy = -(v - 0.5f) * qh;

        float dotSize = 0.04f;
        glm::mat4 mm = glm::translate(glm::mat4(1.0f), glm::vec3(ox, oy, 0));
        mm = glm::scale(mm, glm::vec3(dotSize, -dotSize, 1));
        m_renderer->drawSprite3D(m_playerDotTexture->descriptorSet(), mm);
    }

    // Room name labels (high-resolution textures with proper alpha)
    float originX = (float)map->globalOriginX();
    float originY = (float)map->globalOriginY();
    float numTilesW = (float)gW / Map::mapPixPerTile();
    float numTilesH = (float)gH / Map::mapPixPerTile();
    float labelH = 0.055f;
    for (auto& label : map->textLabels()) {
        float u = (label.worldCenterX - originX) / numTilesW;
        float v = (label.worldCenterY - originY) / numTilesH;
        float lx = (u - 0.5f) * qw;
        float ly = -(v - 0.5f) * qh;
        float lw = labelH * label.texW / label.texH;
        glm::mat4 lm = glm::translate(glm::mat4(1.0f), glm::vec3(lx, ly, 0));
        lm = glm::scale(lm, glm::vec3(lw, -labelH, 1));
        m_renderer->drawSprite3D(label.texture->descriptorSet(), lm);
    }
}

void Engine::clickPickBillboard() {
    Map* map = m_mapManager ? m_mapManager->currentMap() : nullptr;
    if (!map) return;
    int count = map->billboardCount();
    fprintf(stderr, "clickPick: %d billboards\n", count);
    if (count == 0) { map->selectBillboard(-1); return; }

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);
    VkExtent2D ext = m_swapChainExtent;
    fprintf(stderr, "mouse: %.0f,%.0f  ext: %dx%d\n", mx, my, ext.width, ext.height);

    // Use screen-space distance instead of raycasting
    glm::mat4 pv = m_projMatrix * m_viewMatrix;
    int best = -1;
    float bestDist = 1e30f;
    for (int i = 0; i < count; ++i) {
        glm::vec3 wp = map->billboardPosition(i);
        wp.y = 32.0f;
        glm::vec4 clip = pv * glm::vec4(wp, 1.0f);
        if (clip.w <= 0.0f) continue;
        float ndx = clip.x / clip.w;
        float ndy = clip.y / clip.w;
        float sx = (ndx + 1.0f) * 0.5f * ext.width;
        float sy = (ndy + 1.0f) * 0.5f * ext.height;
        float d = std::sqrt((sx - mx) * (sx - mx) + (sy - my) * (sy - my));
        fprintf(stderr, "  bill %d -> screen %.0f,%.0f  dist %.1f\n", i, sx, sy, d);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    if (bestDist > 80.0f) best = -1;
    fprintf(stderr, "selected: %d (dist %.1f)\n", best, bestDist);
    map->selectBillboard(best);
}

void Engine::updateCamera() {
    VkExtent2D extent = m_swapChainExtent;
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

    // glm::perspective produces OpenGL NDC (Y-up), but Vulkan NDC is Y-down
    // Flip Y in clip space to correct the orientation
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 1.0f, 3000.0f);
    proj[1][1] *= -1.0f;
    m_projMatrix = proj;

    float yawRad = glm::radians(m_camYaw);
    float pitchRad = glm::radians(m_camPitch);
    m_camEye = m_camTarget + glm::vec3(
        m_camDistance * std::cos(pitchRad) * std::sin(yawRad),
        m_camDistance * std::sin(pitchRad),
        m_camDistance * std::cos(pitchRad) * std::cos(yawRad)
    );
    m_viewMatrix = glm::lookAt(m_camEye, m_camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Engine::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;

        Map* currentMap = m_mapManager->currentMap();

        if (m_showMenu) {
            handleMenuInput();
        } else if (!m_showMap) {
            if (!m_mapManager->isTransitioning() && currentMap) {
                m_player->update(deltaTime, currentMap);
                if (m_npc) m_npc->update(deltaTime, currentMap, m_player->position());
            }
            m_mapManager->update(deltaTime);
        }
        // Menu-to-game fade transition
        if (m_menuTransitionActive) {
            if (m_showMenu) {
                // Phase 1: fade out from menu
                m_menuTransitionAlpha += deltaTime * (1.0f / m_menuTransitionSpeed);
                if (m_menuTransitionAlpha >= 1.0f) {
                    m_menuTransitionAlpha = 1.0f;
                    m_showMenu = false;
                    m_mapManager->loadMap("player_house");
                }
            } else {
                // Phase 2: fade in to game
                m_menuTransitionAlpha -= deltaTime * (1.0f / m_menuTransitionSpeed);
                if (m_menuTransitionAlpha <= 0.0f) {
                    m_menuTransitionAlpha = 0.0f;
                    m_menuTransitionActive = false;
                }
            }
        }

        currentMap = m_mapManager->currentMap();

        if (!m_showMenu) {
            updateCamera();
        }

        if (!m_showMenu && glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }

        static bool prevF1 = false;
        bool currF1 = glfwGetKey(m_window, GLFW_KEY_F1) == GLFW_PRESS;
        if (currF1 && !prevF1) m_showCollisions = !m_showCollisions;
        prevF1 = currF1;

        static bool prevF2 = false;
        bool currF2 = glfwGetKey(m_window, GLFW_KEY_F2) == GLFW_PRESS;
        if (currF2 && !prevF2) {
            m_editMode = !m_editMode;
            m_mouseDown = false;
            m_draggingBillboard = false;
            glfwSetWindowTitle(m_window, m_editMode ? "Behest [EDIT MODE]" : "Behest");
            if (!m_editMode && currentMap) currentMap->selectBillboard(-1);
        }
        prevF2 = currF2;

        // Edit mode: arrow keys move selected billboard
        if (m_editMode && currentMap && currentMap->selectedBillboard() >= 0) {
            float step = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 10.0f : 1.0f;
            glm::vec3 pos = currentMap->billboardPosition(currentMap->selectedBillboard());
            bool moved = false;
            if (glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS) { pos.x -= step; moved = true; }
            if (glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS) { pos.x += step; moved = true; }
            if (glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS) { pos.z += step; moved = true; }
            if (glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS) { pos.z -= step; moved = true; }
            if (glfwGetKey(m_window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) { pos.y += step; moved = true; }
            if (glfwGetKey(m_window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) { pos.y -= step; moved = true; }
            if (moved) currentMap->setBillboardPosition(currentMap->selectedBillboard(), pos);
        }

        // Tab to cycle billboards in edit mode
        static bool prevTab = false;
        bool currTab = glfwGetKey(m_window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (m_editMode && currTab && !prevTab && currentMap) {
            int next = currentMap->selectedBillboard() + 1;
            if (next >= currentMap->billboardCount()) next = 0;
            currentMap->selectBillboard(next);
        }
        prevTab = currTab;

        // F5 to save billboard positions
        static bool prevF5 = false;
        bool currF5 = glfwGetKey(m_window, GLFW_KEY_F5) == GLFW_PRESS;
        if (currF5 && !prevF5 && currentMap) {
            currentMap->saveBillboards("billboards.txt");
            m_savedFlashFrames = 60;
        }
        if (m_savedFlashFrames > 0) {
            --m_savedFlashFrames;
            glfwSetWindowTitle(m_window, m_editMode ? "Behest [EDIT MODE] [SAVED]" : "Behest [SAVED]");
        } else {
            glfwSetWindowTitle(m_window, m_editMode ? "Behest [EDIT MODE]" : "Behest");
        }
        prevF5 = currF5;

        static bool prevM = false;
        bool currM = glfwGetKey(m_window, GLFW_KEY_M) == GLFW_PRESS;
        if (currM && !prevM && !m_showMenu) { m_showMap = !m_showMap; m_mouseDown = false; }
        prevM = currM;

        if (m_showMenu) {
            m_renderer->setClearColor(0.0f, 0.0f, 0.0f);
        } else if (m_showMap && currentMap) {
            m_renderer->setClearColor(0.0f, 0.0f, 0.0f);
            // Switch to orthographic projection for 2D overlay
            VkExtent2D ext = m_swapChainExtent;
            float asp = (float)ext.width / (float)ext.height;
            m_projMatrix = glm::ortho(-asp, asp, -1.0f, 1.0f, -1.0f, 1.0f);
            m_projMatrix[1][1] *= -1.0f;
            m_viewMatrix = glm::mat4(1.0f);
        } else if (currentMap && currentMap->mapId() == "front_yard") {
            m_renderer->setClearColor(0.5f, 0.7f, 1.0f);
        } else {
            m_renderer->setClearColor(0.0f, 0.0f, 0.0f);
        }

        // Set dynamic light based on current map
        if (currentMap && currentMap->mapId() == "front_yard") {
            m_lightPos = glm::vec4(0.0f, 200.0f, 0.0f, 600.0f);
            m_lightColor = glm::vec4(1.0f, 0.95f, 0.9f, 0.35f);
        } else {
            m_lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // full ambient = flat
        }

        if (m_renderer->beginFrame()) {
            if (m_showMenu) {
                renderMenu();
            } else if (currentMap) {
                if (m_showMap) {
                    renderMapOverlay(currentMap, m_player);
                } else {
                    m_mapManager->render();
                    if (m_npc) m_npc->render();
                    m_player->render();
                }
            }

            if (m_showCollisions && currentMap && !m_showMap && !m_showMenu) {
                renderCollisionDebug(currentMap, m_player);
            }

            if (m_editMode && currentMap && currentMap->selectedBillboard() >= 0 && !m_showMap && !m_showMenu) {
                int sel = currentMap->selectedBillboard();
                glm::vec3 pos = currentMap->billboardPosition(sel);
                std::string name = currentMap->billboardName(sel);
                glm::vec4 highlightCol(1.0f, 0.8f, 0.0f, 1.0f);
                m_renderer->drawDebugBox(pos - glm::vec3(16, 0, 16), pos + glm::vec3(16, 64, 16), highlightCol);
                // Axis arrows: X=red, Y=green, Z=blue
                m_renderer->drawDebugBox(pos, pos + glm::vec3(24, 1, 1), glm::vec4(1, 0, 0, 1));
                m_renderer->drawDebugBox(pos, pos + glm::vec3(1, 24, 1), glm::vec4(0, 1, 0, 1));
                m_renderer->drawDebugBox(pos, pos + glm::vec3(1, 1, 24), glm::vec4(0, 0, 1, 1));
                (void)name;
            }

            // Menu-to-game transition overlay (drawn on top of everything)
            if (m_menuTransitionActive && m_menuTransitionAlpha > 0.01f && m_menuBlackOverlay) {
                VkExtent2D ext = m_swapChainExtent;
                float asp = (float)ext.width / (float)ext.height;
                glm::mat4 proj = glm::ortho(-asp, asp, -1.0f, 1.0f, -1.0f, 1.0f);
                proj[1][1] *= -1.0f;
                m_projMatrix = proj;
                m_viewMatrix = glm::mat4(1.0f);
                glm::mat4 model = glm::scale(
                    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
                    glm::vec3(asp * 2.0f, 2.0f, 1.0f));
                m_renderer->drawSprite3D(m_menuBlackOverlay->descriptorSet(), model,
                    glm::vec4(1.0f, 1.0f, 1.0f, m_menuTransitionAlpha));
            }

            m_renderer->endFrame();
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(m_device);
}

void Engine::cleanup() {
    destroyMenuTextures();
    Font::shutdown();
    delete m_mapManager;
    delete m_player;
    delete m_npc;
    delete m_renderer;
    delete m_playerDotTexture;
    delete m_shadowTexture;

    cleanupSwapChain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    vkDestroyDevice(m_device, nullptr);

    if (ENABLE_VALIDATION_LAYERS) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Engine::createInstance() {
    if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Behest";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void Engine::setupDebugMessenger() {
    if (!ENABLE_VALIDATION_LAYERS) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (!func || func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger");
    }
}

void Engine::createSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
}

void Engine::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan-capable GPUs found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable GPU found");
}

void Engine::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_FALSE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

    if (ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void Engine::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void Engine::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());
    for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
        m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Engine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void Engine::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void Engine::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDesc = QuadVertex::getBindingDescription();
    auto attributeDesc = QuadVertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    std::array<VkDescriptorSetLayout, 2> setLayouts = { m_descriptorSetLayout, m_renderer->textureDescriptorLayout() };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SpritePushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
}

void Engine::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    for (size_t i = 0; i < m_swapChainImageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
            m_swapChainImageViews[i],
            m_depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void Engine::createCommandPool() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void Engine::createDepthResources() {
    VkFormat depthFormat = findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    createImage(m_swapChainExtent.width, m_swapChainExtent.height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);
    m_depthImageView = createImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Engine::createCommandBuffers() {
    m_commandBuffers.resize(m_swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void Engine::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}

void Engine::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device);
    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
}

void Engine::cleanupSwapChain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

QueueFamilyIndices Engine::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
        ++i;
    }

    return indices;
}

SwapChainSupportDetails Engine::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool Engine::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool Engine::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkSurfaceFormatKHR Engine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Engine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Engine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

std::vector<const char*> Engine::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool Engine::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : VALIDATION_LAYERS) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Engine::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Engine::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

uint32_t Engine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

VkFormat Engine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported format");
}

void Engine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

void Engine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void Engine::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(m_device, image, imageMemory, 0);
}

VkImageView Engine::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
    return imageView;
}

void Engine::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(commandBuffer);
}

void Engine::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer Engine::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void Engine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

VkShaderModule Engine::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

// ============================================================================
// Main Menu
// ============================================================================

void Engine::loadMenuTextures() {
    if (!Font::load("fonts/alagard.ttf")) return;

    // ---- Black overlay for fade-in animation ----
    {
        const uint8_t black[4] = { 0, 0, 0, 255 };
        m_menuBlackOverlay = new Texture(this, black, 1, 1);
    }

    m_menuAnimTimer = 0.0f;
    m_menuInputEnabled = false;

    // ---- Title ----
    {
        const char* title = "BEHEST";
        int titleSize = 48;
        int pad = 8;
        int tw = Font::textWidth(title, titleSize) + pad * 2;
        int th = Font::textHeight(titleSize) + pad * 2;
        int baseline = pad + Font::ascent(titleSize);
        std::vector<uint8_t> pixels(tw * th * 4, 0);
        // Drop shadow (supersampled for smoother edges)
        Font::renderText(pixels.data(), tw, th, title, pad + 3, baseline + 3, titleSize, 30, 18, 8, 3);
        // Main text
        Font::renderText(pixels.data(), tw, th, title, pad, baseline, titleSize, 240, 220, 180, 3);
        m_menuTitleTexture = new Texture(this, pixels.data(), tw, th);
        m_menuTitleTexture->setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
    }

    // ---- Subtitle / copyright ----
    {
        const char* sub = "(C) 1992 PIRYL SOFT";
        int subSize = 12;
        int subPad = 4;
        int tw = Font::textWidth(sub, subSize) + subPad * 2;
        int th = Font::textHeight(subSize) + subPad * 2;
        int baseline = subPad + Font::ascent(subSize);
        std::vector<uint8_t> pixels(tw * th * 4, 0);
        Font::renderText(pixels.data(), tw, th, sub, subPad + 2, baseline + 2, subSize, 20, 16, 10, 3);
        Font::renderText(pixels.data(), tw, th, sub, subPad, baseline, subSize, 160, 150, 130, 3);
        m_menuSubtitleTexture = new Texture(this, pixels.data(), tw, th);
        m_menuSubtitleTexture->setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
    }

    // ---- Options ----
    const char* options[MENU_OPTION_COUNT] = { "NEW GAME", "QUIT" };
    int optionSize = 28;
    int optPad = 4;
    int optTh = Font::textHeight(optionSize) + optPad * 2;
    int optBaseline = optPad + Font::ascent(optionSize);
    for (int i = 0; i < MENU_OPTION_COUNT; ++i) {
        int tw = Font::textWidth(options[i], optionSize) + optPad * 2;
        std::vector<uint8_t> pixels(tw * optTh * 4, 0);
        Font::renderText(pixels.data(), tw, optTh, options[i], optPad, optBaseline, optionSize, 230, 220, 200, 3);
        m_menuOptionTextures[i] = new Texture(this, pixels.data(), tw, optTh);
        m_menuOptionTextures[i]->setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
    }

    // ---- Cursor (yellow triangle pointing right) ----
    {
        const int S = 24;
        std::vector<uint8_t> pixels(S * S * 4, 0);
        for (int y = 0; y < S; ++y) {
            int halfY = S / 2;
            int dy = y - halfY;
            int span = halfY - std::abs(dy);
            int x0 = (S / 4) - span / 2;
            int x1 = x0 + span;
            for (int x = x0; x < x1; ++x) {
                if (x >= 0 && x < S) {
                    pixels[(y * S + x) * 4 + 0] = 255;
                    pixels[(y * S + x) * 4 + 1] = 220;
                    pixels[(y * S + x) * 4 + 2] = 80;
                    pixels[(y * S + x) * 4 + 3] = 255;
                }
            }
        }
        m_menuCursorTexture = new Texture(this, pixels.data(), S, S);
    }
}

void Engine::destroyMenuTextures() {
    if (m_menuTitleTexture) { delete m_menuTitleTexture; m_menuTitleTexture = nullptr; }
    if (m_menuSubtitleTexture) { delete m_menuSubtitleTexture; m_menuSubtitleTexture = nullptr; }
    if (m_menuCursorTexture) { delete m_menuCursorTexture; m_menuCursorTexture = nullptr; }
    if (m_menuBlackOverlay) { delete m_menuBlackOverlay; m_menuBlackOverlay = nullptr; }
    for (int i = 0; i < MENU_OPTION_COUNT; ++i) {
        if (m_menuOptionTextures[i]) { delete m_menuOptionTextures[i]; m_menuOptionTextures[i] = nullptr; }
    }
}

void Engine::handleMenuInput() {
    if (!m_menuInputEnabled) return;
    static bool prevUp = false, prevDown = false, prevEnter = false, prevSpace = false;

    bool up = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
    bool down = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;
    bool enter = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    bool space = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;

    if (up && !prevUp) {
        m_menuSelection = (m_menuSelection - 1 + MENU_OPTION_COUNT) % MENU_OPTION_COUNT;
    }
    if (down && !prevDown) {
        m_menuSelection = (m_menuSelection + 1) % MENU_OPTION_COUNT;
    }

    // Mouse wheel
    if (m_menuScrollAccum >= 1.0f) {
        int steps = (int)m_menuScrollAccum;
        m_menuSelection = (m_menuSelection - steps + MENU_OPTION_COUNT * steps) % MENU_OPTION_COUNT;
        m_menuScrollAccum -= steps;
    } else if (m_menuScrollAccum <= -1.0f) {
        int steps = -(int)m_menuScrollAccum;
        m_menuSelection = (m_menuSelection + steps) % MENU_OPTION_COUNT;
        m_menuScrollAccum += steps;
    }
    m_menuScrollAccum *= 0.5f;
    if (std::abs(m_menuScrollAccum) < 0.01f) m_menuScrollAccum = 0.0f;

    // --- Mouse hover over options (text-only hit region) ---
    VkExtent2D ext = m_swapChainExtent;
    const int optionSize = 28;
    const int optPad = 4;
    const float targetOptionPx = std::min(ext.width * 0.12f, 140.0f);
    const float optionScale = targetOptionPx / Font::textWidth("NEW GAME", optionSize);
    float representativeOptionH = m_menuOptionTextures[0] ? m_menuOptionTextures[0]->size().y * optionScale : 36.0f;
    const float optionSpacing = representativeOptionH * 1.6f;
    const float optionsStartY = (ext.height - optionSpacing * (MENU_OPTION_COUNT - 1)) * 0.5f + 30.0f;
    const char* optionLabels[MENU_OPTION_COUNT] = { "NEW GAME", "QUIT" };

    for (int i = 0; i < MENU_OPTION_COUNT; ++i) {
        if (!m_menuOptionTextures[i]) continue;
        glm::vec2 os = m_menuOptionTextures[i]->size();
        float drawPxW = os.x * optionScale;
        float ox = (ext.width - drawPxW) / 2.0f;
        float oy = optionsStartY + i * optionSpacing;

        // Use text-only bounds (skip the 4px texture padding)
        float textW = Font::textWidth(optionLabels[i], optionSize) * optionScale;
        float textH = Font::textHeight(optionSize) * optionScale;
        float tx = ox + optPad * optionScale;
        float ty = oy + optPad * optionScale;

        if (m_lastMouseX >= tx && m_lastMouseX <= tx + textW &&
            m_lastMouseY >= ty && m_lastMouseY <= ty + textH) {
            m_menuSelection = i;
        }
    }

    auto startNewGame = [&]() {
        if (!m_menuTransitionActive) {
            m_menuTransitionActive = true;
            m_menuTransitionAlpha = 0.0f;
            m_menuInputEnabled = false;
        }
    };

    // --- Mouse click ---
    if (m_menuClickPending) {
        m_menuClickPending = false;
        switch (m_menuSelection) {
            case 0: startNewGame(); break;
            case 1: glfwSetWindowShouldClose(m_window, GLFW_TRUE); break;
        }
    }

    if ((enter && !prevEnter) || (space && !prevSpace)) {
        switch (m_menuSelection) {
            case 0: startNewGame(); break;
            case 1: glfwSetWindowShouldClose(m_window, GLFW_TRUE); break;
        }
    }

    prevUp = up;
    prevDown = down;
    prevEnter = enter;
    prevSpace = space;
}

void Engine::renderMenu() {
    // Update animation timer
    m_menuAnimTimer += 1.0f / 60.0f;

    // Animation phases (in seconds):
    //   0.0 – 0.5 : black screen, nothing visible
    //   0.5 – 1.5 : title fades in
    //   1.5 – 2.5 : subtitle + options fade in
    //   2.5+      : fully visible, enable input
    float t = m_menuAnimTimer;
    float titleAlpha = 0.0f;
    float otherAlpha = 0.0f;
    float overlayAlpha = 0.0f;

    if (t < 0.5f) {
        overlayAlpha = 1.0f;
    } else if (t < 1.5f) {
        titleAlpha = (t - 0.5f) / 1.0f;
        overlayAlpha = 1.0f - titleAlpha;
    } else if (t < 2.5f) {
        titleAlpha = 1.0f;
        otherAlpha = (t - 1.5f) / 1.0f;
    } else {
        titleAlpha = 1.0f;
        otherAlpha = 1.0f;
        if (!m_menuInputEnabled) m_menuInputEnabled = true;
    }

    // Orthographic projection for menu
    VkExtent2D ext = m_swapChainExtent;
    float asp = (float)ext.width / (float)ext.height;
    m_projMatrix = glm::ortho(-asp, asp, -1.0f, 1.0f, -1.0f, 1.0f);
    m_projMatrix[1][1] *= -1.0f;
    m_viewMatrix = glm::mat4(1.0f);

    // Convert pixel size to sprite size (clip-space units)
    auto pxSize = [&](float pxW, float pxH) {
        return glm::vec2(pxW / ext.width * 2.0f * asp, pxH / ext.height * 2.0f);
    };
    // Convert pixel center to sprite position (clip-space units)
    auto pxCenter = [&](float cx, float cy) {
        return glm::vec2((cx / ext.width) * 2.0f * asp - asp,
                          1.0f - (cy / ext.height) * 2.0f);
    };

    // Helper to draw a sprite with pixel top-left, pixel size, and alpha.
    auto drawPx = [&](Texture* tex, float px, float py, float pw, float ph, float alpha = 1.0f) {
        if (!tex) return;
        glm::vec2 center = pxCenter(px + pw * 0.5f, py + ph * 0.5f);
        glm::vec2 size = pxSize(pw, ph);
        glm::mat4 model = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f)),
            glm::vec3(size.x, -size.y, 1.0f));
        m_renderer->drawSprite3D(tex->descriptorSet(), model, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
    };

    // Full-screen black overlay during fade
    if (overlayAlpha > 0.01f && m_menuBlackOverlay) {
        drawPx(m_menuBlackOverlay, 0.0f, 0.0f, (float)ext.width, (float)ext.height, overlayAlpha);
    }

    // Title at top center of screen
    if (m_menuTitleTexture && titleAlpha > 0.01f) {
        glm::vec2 ts = m_menuTitleTexture->size();
        float titleScale = std::min(ext.width * 0.50f, 600.0f) / ts.x;
        float drawPxW = ts.x * titleScale;
        float drawPxH = ts.y * titleScale;
        float tx = (ext.width - drawPxW) / 2.0f;
        float ty = ext.height * 0.18f;
        drawPx(m_menuTitleTexture, tx, ty, drawPxW, drawPxH, titleAlpha);
    }

    // Subtitle at bottom
    if (m_menuSubtitleTexture && otherAlpha > 0.01f) {
        glm::vec2 ss = m_menuSubtitleTexture->size();
        float subScale = std::min(ext.width * 0.20f, 300.0f) / ss.x;
        float drawPxW = ss.x * subScale;
        float drawPxH = ss.y * subScale;
        float sx = (ext.width - drawPxW) / 2.0f;
        float sy = ext.height - drawPxH - 16.0f;
        drawPx(m_menuSubtitleTexture, sx, sy, drawPxW, drawPxH, otherAlpha);
    }

    // Options centered vertically
    if (otherAlpha > 0.01f) {
        const int optionSize = 28;
        const float targetOptionPx = std::min(ext.width * 0.12f, 140.0f);
        const float optionScale = targetOptionPx / Font::textWidth("NEW GAME", optionSize);
        float representativeOptionH = m_menuOptionTextures[0] ? m_menuOptionTextures[0]->size().y * optionScale : 36.0f;
        const float optionSpacing = representativeOptionH * 1.6f;
        const float optionsStartY = (ext.height - optionSpacing * (MENU_OPTION_COUNT - 1)) * 0.5f + 30.0f;
        for (int i = 0; i < MENU_OPTION_COUNT; ++i) {
            if (!m_menuOptionTextures[i]) continue;
            glm::vec2 os = m_menuOptionTextures[i]->size();
            bool selected = (i == m_menuSelection);
            float scale = optionScale * (selected ? 1.15f : 1.0f);
            float drawPxW = os.x * scale;
            float drawPxH = os.y * scale;
            float ox = (ext.width - drawPxW) / 2.0f;
            float oy = optionsStartY + i * optionSpacing;

            drawPx(m_menuOptionTextures[i], ox, oy, drawPxW, drawPxH, otherAlpha);

            if (selected && m_menuCursorTexture) {
                glm::vec2 cs = m_menuCursorTexture->size();
                float curScale = drawPxH * 0.55f / cs.y;
                float curPxW = cs.x * curScale;
                float curPxH = cs.y * curScale;
                float curX = ox - curPxW - 16.0f;
                float curY = oy + (drawPxH - curPxH) / 2.0f;
                drawPx(m_menuCursorTexture, curX, curY, curPxW, curPxH, otherAlpha);
            }
        }
    }
}
