#include <Crab/Platform/Window.h>
#include <Crab/Core/Logger.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace Crab {

Window::~Window() {
    destroy();
}

bool Window::create(int width, int height, const char* title) {
    m_width = width;
    m_height = height;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logger::error("SDL_Init failed");
        return false;
    }

    m_window = SDL_CreateWindow(
        title,
        m_width,
        m_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        Logger::error("SDL_CreateWindow failed");
        SDL_Quit();
        return false;
    }

    Logger::info("Window created");
    return true;
}

void Window::destroy() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        Logger::info("Window destroyed");
    }
    SDL_Quit();
}

bool Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            m_resized = true;
        }
    }
    return true;
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(m_window, instance, nullptr, &surface)) {
        Logger::error("SDL_Vulkan_CreateSurface failed");
        return VK_NULL_HANDLE;
    }
    return surface;
}

} // namespace Crab
