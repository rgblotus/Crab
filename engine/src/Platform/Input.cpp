#include <Crab/Platform/Input.h>
#include <SDL3/SDL.h>
#include <cstring>

namespace Crab {

void Input::beginFrame() {
    std::memcpy(m_keysPrev, m_keys, sizeof(m_keys));
    std::memcpy(m_mouseButtonsPrev, m_mouseButtons, sizeof(m_mouseButtons));
    m_mouseDelta = glm::vec2(0.0f);
    m_scrollDelta = 0.0f;
}

void Input::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode < KEY_COUNT) {
                m_keys[event.key.scancode] = true;
            }
            break;
        case SDL_EVENT_KEY_UP:
            if (event.key.scancode < KEY_COUNT) {
                m_keys[event.key.scancode] = false;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            m_mouseDelta.x += event.motion.xrel;
            m_mouseDelta.y += event.motion.yrel;
            m_mousePos.x = event.motion.x;
            m_mousePos.y = event.motion.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button < 8) {
                m_mouseButtons[event.button.button] = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button < 8) {
                m_mouseButtons[event.button.button] = false;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            m_scrollDelta = event.wheel.y;
            break;
    }
}

void Input::endFrame() {
    // Nothing needed
}

bool Input::isKeyDown(int scancode) const {
    if (scancode < 0 || scancode >= KEY_COUNT) return false;
    return m_keys[scancode];
}

bool Input::isKeyPressed(int scancode) const {
    if (scancode < 0 || scancode >= KEY_COUNT) return false;
    return m_keys[scancode] && !m_keysPrev[scancode];
}

bool Input::isKeyReleased(int scancode) const {
    if (scancode < 0 || scancode >= KEY_COUNT) return false;
    return !m_keys[scancode] && m_keysPrev[scancode];
}

bool Input::isMouseButtonDown(int button) const {
    if (button < 0 || button >= 8) return false;
    return m_mouseButtons[button];
}

bool Input::isMouseButtonPressed(int button) const {
    if (button < 0 || button >= 8) return false;
    return m_mouseButtons[button] && !m_mouseButtonsPrev[button];
}

} // namespace Crab
