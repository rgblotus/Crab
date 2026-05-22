#pragma once

#include <glm/glm.hpp>
#include <cstdint>

union SDL_Event;

namespace Crab {

class Input {
public:
    void beginFrame();
    void handleEvent(const SDL_Event& event);
    void endFrame();

    bool isKeyDown(int scancode) const;
    bool isKeyPressed(int scancode) const;
    bool isKeyReleased(int scancode) const;

    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    glm::vec2 mousePosition() const { return m_mousePos; }
    glm::vec2 mouseDelta() const { return m_mouseDelta; }
    float scrollDelta() const { return m_scrollDelta; }

private:
    static constexpr int KEY_COUNT = 512;

    bool m_keys[KEY_COUNT] = {};
    bool m_keysPrev[KEY_COUNT] = {};
    bool m_mouseButtons[8] = {};
    bool m_mouseButtonsPrev[8] = {};

    glm::vec2 m_mousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    float m_scrollDelta = 0.0f;
};

} // namespace Crab
