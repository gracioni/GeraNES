#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

class InputManager
{

private:

    struct ButtonState {
        bool pressed;
        int pressedFrameId;
    };

    std::map<std::string, ButtonState> m_inputMap;

    int m_currentFrameId = -1;  

    static constexpr float ANALOG_TO_DIGITAL_THRESHOLD = 0.6f;

    InputManager();
    ~InputManager();

    const char* SDL_KeyIndexName(uint8_t index);
    void setInputState(const std::string& keyName, bool state, int frameId);

public:

    static InputManager& instance();

    /**
     * Must be called once per frame
    */
    void updateInputs(bool captureKeyboard = true);
    bool isPressed(const std::string& name);
    bool isJustPressed(const std::string& name);
    std::vector<std::string> capture();

};
