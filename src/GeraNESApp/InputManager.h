#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <functional>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <iostream>
#include <map>

#include <SDL.h>
#include <SDL_gamecontroller.h>

#define ANALOG_TO_DIGITAL_THRESHOLD 0.6f

class InputManager
{

private:

    struct ButtonState {
        bool pressed;
        int pressedFrameId;
    };

    std::map<std::string, ButtonState> m_inputMap;

    int m_currentFrameId = 0;  

    InputManager()
    {
        SDL_Init(SDL_INIT_GAMECONTROLLER);
    }

    ~InputManager()
    {
    }

    const char* SDL_KeyIndexName(uint8_t index) {
        // https://wiki.libsdl.org/SDL2/SDL_Scancode
        // 'Return' is used twice, so we need this
        if(index == SDL_SCANCODE_RETURN2) return "Return2";
        return SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(index)));        
    }

    void setInputState(const std::string& keyName, bool state, int frameId)
    {
        if(state && !m_inputMap[keyName].pressed)
            m_inputMap[keyName] = ButtonState{state, frameId};
        else
            m_inputMap[keyName] = ButtonState{state, -1};
    }

public:

    static InputManager& instance()
    {
        static InputManager staticInstance;
        return staticInstance;
    }

    void updateInputs(int frameId)
    {
        m_currentFrameId = frameId;

        const Uint8* keyboardState = SDL_GetKeyboardState(NULL);

        for(int i = 0; i < 256; i++) {
            setInputState(SDL_KeyIndexName(i), keyboardState[i], m_currentFrameId);
            //if(keyboardState[i]) std::cout << "'" << SDL_KeyIndexName(i) << "'" << std::endl;
        }

        int numControllers = SDL_NumJoysticks();

        for(int i = 0; i < numControllers; i++) {
            
            SDL_GameController* controller = SDL_GameControllerOpen(i);

            //read buttons
            for(int j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++) {
                std::stringstream name;
                name << "J" << i << "B" << j;
                setInputState(
                    name.str(),
                    SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(j)),
                    m_currentFrameId
                );
            }

            //read axis
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

    bool isPressed(const std::string& name)
    {
        if(name.empty()) return false;

        if(m_inputMap.find(name) != m_inputMap.end()){
            return m_inputMap[name].pressed;
        }

        return false;
    }

    bool isJustPressed(const std::string& name)
    {
        if(name.empty()) return false;

        if(m_inputMap.find(name) != m_inputMap.end()){
            return m_inputMap[name].pressed && m_inputMap[name].pressedFrameId == m_currentFrameId;
        }

        return false;
    }

    std::vector<std::string> capture()
    {
        std::vector<std::string> ret;

        for(auto i : m_inputMap){
            if(isPressed(i.first)) ret.push_back(i.first);
        }

        return ret;
    }    

};

#endif // INPUTMANAGER_H
