#pragma once

#include <string>
#include <vulkan/vulkan.h>

struct SDL_Window;

namespace Crab {

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(int width, int height, const char* title);
    void destroy();
    bool pollEvents();

    SDL_Window* handle() const { return m_window; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    float aspectRatio() const { return m_width / (float)m_height; }

    // Vulkan surface
    VkSurfaceKHR createSurface(VkInstance instance) const;

    // Resize tracking
    bool wasResized() const { return m_resized; }
    void resetResized() { m_resized = false; }

private:
    SDL_Window* m_window = nullptr;
    int m_width = 800;
    int m_height = 600;
    bool m_resized = false;
};

} // namespace Crab
