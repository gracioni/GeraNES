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

class InputInfo {

public:

    static constexpr std::array<const char*, 11> BUTTONS {"A", "B", "Select", "Start", "Up", "Down", "Left", "Right", "Save", "Load", "Rewind"};

    std::string a;
    std::string b;
    std::string select;
    std::string start;
    std::string up;
    std::string down;
    std::string left;
    std::string right;
    std::string saveState;
    std::string loadState;
    std::string rewind;

private:

    std::map<const std::string, std::string*> _map;

public:    

    InputInfo() {
        _map.insert(std::make_pair(BUTTONS[0], &a));
        _map.insert(std::make_pair(BUTTONS[1], &b));
        _map.insert(std::make_pair(BUTTONS[2], &select));
        _map.insert(std::make_pair(BUTTONS[3], &start));
        _map.insert(std::make_pair(BUTTONS[4], &up));
        _map.insert(std::make_pair(BUTTONS[5], &down));
        _map.insert(std::make_pair(BUTTONS[6], &left));
        _map.insert(std::make_pair(BUTTONS[7], &right));
        _map.insert(std::make_pair(BUTTONS[8], &saveState));
        _map.insert(std::make_pair(BUTTONS[9], &loadState));
        _map.insert(std::make_pair(BUTTONS[10], &rewind));
    }

    const std::string& getByButtonName(const std::string& name) {
        return *_map[name];
    }

    const void setByButtonIndex(int index, const std::string& input) {
        
        switch(index){
            case 0: a = input; break;
            case 1: b = input; break;
            case 2: select = input; break;
            case 3: start = input; break;
            case 4: up = input; break;
            case 5: down = input; break;
            case 6: left = input; break;
            case 7: right = input; break;
            case 8: saveState = input; break;
            case 9: loadState = input; break;
            case 10: rewind = input; break;
        }
    }



};

class InputManager
{

private:

    std::map<std::string, bool > m_inputMap;    

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

public:

    static InputManager& instance()
    {
        static InputManager staticInstance;
        return staticInstance;
    }

    void updateInputs()
    {
        const Uint8* keyboardState = SDL_GetKeyboardState(NULL);

        for(int i = 0; i < 256; i++) {
            setInputState(SDL_KeyIndexName(i), keyboardState[i]);
            //if(keyboardState[i]) std::cout << "'" << SDL_KeyIndexName(i) << "'" << std::endl;
        }

        int numControllers = SDL_NumJoysticks();

        for(int i = 0; i < numControllers; i++) {
            
            SDL_GameController* controller = SDL_GameControllerOpen(i);

            //read buttons
            for(int j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++) {
                std::stringstream name;
                name << "J" << i << "B" << j;
                setInputState(name.str(), SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(j)));
            }

            //read axis
            for(int j = 0; j < SDL_CONTROLLER_AXIS_MAX; j++) {
                std::stringstream posName;
                posName << "J" << i << "A" << j << "+";
                bool state = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(j)) > ANALOG_TO_DIGITAL_THRESHOLD * 0x7FFF;
                setInputState(posName.str(), state);

                std::stringstream negName;
                negName << "J" << i << "A" << j << "-";
                state = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(j)) < ANALOG_TO_DIGITAL_THRESHOLD * -0x7FFF;
                setInputState(negName.str(), state);
            }
        }
    }

    bool get(const std::string& name)
    {
        //std::cout << "testing: " << name << std::endl;

        if(name.empty()) return false;

        if(m_inputMap.find(name) != m_inputMap.end()){
            //if(m_inputMap[name]) std::cout << name << std::endl;
            return m_inputMap[name];
        }

        return false;
    }

    std::vector<std::string> capture()
    {
        std::vector<std::string> ret;

        for(auto i : m_inputMap){
            if(get(i.first)) ret.push_back(i.first);
        }

        return ret;
    }

    void setInputState(const std::string& keyName, bool state)
    {
        m_inputMap[keyName] = state;
    }

};

#endif // INPUTMANAGER_H
