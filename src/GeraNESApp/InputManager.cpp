#include "GeraNESApp/InputManager.h"

#include <sstream>

#include <SDL.h>
#include <SDL_gamecontroller.h>

InputManager::InputManager()
{
    SDL_Init(SDL_INIT_GAMECONTROLLER);
}

InputManager::~InputManager()
{
}

const char* InputManager::SDL_KeyIndexName(uint8_t index)
{
    // https://wiki.libsdl.org/SDL2/SDL_Scancode
    // 'Return' is used twice, so we need this.
    if(index == SDL_SCANCODE_RETURN2) return "Return2";
    return SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(index)));
}

void InputManager::setInputState(const std::string& keyName, bool state, int frameId)
{
    if(state && !m_inputMap[keyName].pressed)
        m_inputMap[keyName] = ButtonState{state, frameId};
    else
        m_inputMap[keyName] = ButtonState{state, -1};
}

InputManager& InputManager::instance()
{
    static InputManager staticInstance;
    return staticInstance;
}

void InputManager::updateInputs(bool captureKeyboard)
{
    ++m_currentFrameId;

    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);

    for(int i = 0; i < 256; i++) {
        setInputState(SDL_KeyIndexName(static_cast<uint8_t>(i)), captureKeyboard && keyboardState[i], m_currentFrameId);
    }

    int numControllers = SDL_NumJoysticks();
    for(int i = 0; i < numControllers; i++) {
        SDL_GameController* controller = SDL_GameControllerOpen(i);
        if(controller == nullptr) continue;

        for(int j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++) {
            std::stringstream name;
            name << "J" << i << "B" << j;
            setInputState(
                name.str(),
                SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(j)),
                m_currentFrameId
            );
        }

        for(int j = 0; j < SDL_CONTROLLER_AXIS_MAX; j++) {
            std::stringstream posName;
            posName << "J" << i << "A" << j << "+";
            bool state = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(j)) > ANALOG_TO_DIGITAL_THRESHOLD * 0x7FFF;
            setInputState(posName.str(), state, m_currentFrameId);

            std::stringstream negName;
            negName << "J" << i << "A" << j << "-";
            state = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(j)) < ANALOG_TO_DIGITAL_THRESHOLD * -0x7FFF;
            setInputState(negName.str(), state, m_currentFrameId);
        }
    }
}

bool InputManager::isPressed(const std::string& name)
{
    if(name.empty()) return false;

    if(name == "+") {
        return isPressed("=") || isPressed("Keypad +") || isPressed("KP +") || isPressed("Plus");
    }

    if(m_inputMap.find(name) != m_inputMap.end()){
        return m_inputMap[name].pressed;
    }

    return false;
}

bool InputManager::isJustPressed(const std::string& name)
{
    if(name.empty()) return false;

    if(name == "+") {
        return isJustPressed("=") || isJustPressed("Keypad +") || isJustPressed("KP +") || isJustPressed("Plus");
    }

    if(m_inputMap.find(name) != m_inputMap.end()){
        return m_inputMap[name].pressed && m_inputMap[name].pressedFrameId == m_currentFrameId;
    }

    return false;
}

std::vector<std::string> InputManager::capture()
{
    std::vector<std::string> ret;

    for(const auto& i : m_inputMap){
        if(isJustPressed(i.first)) ret.push_back(i.first);
    }

    return ret;
}
